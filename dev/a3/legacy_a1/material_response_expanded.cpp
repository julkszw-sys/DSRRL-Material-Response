#include "closed_plan.hpp"

typedef unsigned long long usize;
typedef long long isize;
extern "C" void *memcpy(void *d,const void *s,usize n){unsigned char *dd=(unsigned char*)d;const unsigned char *ss=(const unsigned char*)s;for(usize i=0;i<n;++i)dd[i]=ss[i];return d;}
extern "C" void *memset(void *d,int c,usize n){unsigned char *dd=(unsigned char*)d;for(usize i=0;i<n;++i)dd[i]=(unsigned char)c;return d;}

struct ShaderDesc {
    const void *code;
    usize code_size;
    const char *entry_point;
    u32 spec_constants;
    const u32 *spec_constant_ids;
    const u32 *spec_constant_values;
};
struct PipelineSubobject { u32 type; u32 count; void *data; };

static const u32 EVT_INIT_PIPELINE = 26u;
static const u32 EVT_CREATE_PIPELINE = 27u;
static const u32 EVT_BIND_PIPELINE = 42u;
static const u32 PIXEL_SHADER_SUBOBJECT = 5u;
static const u32 PIXEL_PIPELINE_STAGE = 0x80u;
static const u32 API_VERSION = 20u;
static const u32 SCRATCH_SIZE = 32768u;
struct PlanStorage { volatile u32 state; u32 fail_reason; u8 data[SCRATCH_SIZE]; };
// state: 0=EMPTY, 1=BUILDING, 2=READY, 3=FAILED. One persistent buffer per certified plan.
static PlanStorage g_plan_store[kPlanCount] = {};
static const u32 PIPE_MAP_COUNT = 4096u;
struct PipeMap { volatile u64 pipeline; u32 plan_index; u32 pad; };
static PipeMap g_pipe_map[PIPE_MAP_COUNT] = {};
static volatile u32 g_plan_bind_logged[kPlanCount] = {};
static void *g_addon_module = 0;
static void *g_reshade_module = 0;

typedef bool (*PFN_RegisterAddon)(void *, u32);
typedef void (*PFN_UnregisterAddon)(void *);
typedef void (*PFN_RegisterEvent)(u32, void *);
typedef void (*PFN_UnregisterEvent)(u32, void *);
typedef void (*PFN_LogMessage)(void *, int, const char *);
static PFN_RegisterAddon g_register_addon = 0;
static PFN_UnregisterAddon g_unregister_addon = 0;
static PFN_RegisterEvent g_register_event = 0;
static PFN_UnregisterEvent g_unregister_event = 0;
static PFN_LogMessage g_log = 0;
static volatile u32 g_active_logged = 0u;

static inline u32 rd32(const void *vp) {
    const u8 *p=(const u8*)vp;
    return (u32)p[0] | ((u32)p[1]<<8) | ((u32)p[2]<<16) | ((u32)p[3]<<24);
}
static inline u16 rd16(const void *vp) {
    const u8 *p=(const u8*)vp; return (u16)((u16)p[0] | ((u16)p[1]<<8));
}
static inline void wr32(void *vp,u32 v) {
    u8 *p=(u8*)vp; p[0]=(u8)v; p[1]=(u8)(v>>8); p[2]=(u8)(v>>16); p[3]=(u8)(v>>24);
}
static void copy_bytes(void *vd,const void *vs,usize n) {
    volatile u8 *d=(volatile u8*)vd; const volatile u8 *s=(const volatile u8*)vs;
    for(usize i=0;i<n;++i) d[i]=s[i];
}
static void zero_bytes(void *vd,usize n) { volatile u8 *d=(volatile u8*)vd; for(usize i=0;i<n;++i)d[i]=0; }
static bool bytes_eq(const u8 *a,const u8 *b,usize n) { for(usize i=0;i<n;++i) if(a[i]!=b[i]) return false; return true; }
static bool str_eq(const char *a,const char *b) { if(!a||!b)return false; while(*a&&*b){ if(*a++!=*b++)return false; } return *a==*b; }

// Resolve ReShade API exports directly from the supplied ReShade module. No Win32/CRT imports.
static void *find_export(void *module,const char *wanted) {
    if(!module||!wanted) return 0;
    u8 *base=(u8*)module;
    if(rd16(base)!=0x5A4Du) return 0;
    u32 lfanew=rd32(base+0x3c);
    if(rd32(base+lfanew)!=0x00004550u) return 0;
    u8 *opt=base+lfanew+24;
    if(rd16(opt)!=0x20Bu) return 0;
    u32 exp_rva=rd32(opt+112);
    u32 exp_size=rd32(opt+116);
    if(!exp_rva||!exp_size) return 0;
    u8 *e=base+exp_rva;
    u32 n_names=rd32(e+0x18);
    u32 funcs=rd32(e+0x1c), names=rd32(e+0x20), ords=rd32(e+0x24);
    for(u32 i=0;i<n_names;++i) {
        const char *name=(const char*)(base+rd32(base+names+i*4u));
        if(!str_eq(name,wanted)) continue;
        u16 ord=rd16(base+ords+i*2u);
        u32 rva=rd32(base+funcs+(u32)ord*4u);
        if(rva>=exp_rva && rva<exp_rva+exp_size) return 0; // forwarded export: fail closed
        return base+rva;
    }
    return 0;
}

// SHA-256
static const u32 SHA_K[64]={
0x428a2f98u,0x71374491u,0xb5c0fbcfu,0xe9b5dba5u,0x3956c25bu,0x59f111f1u,0x923f82a4u,0xab1c5ed5u,
0xd807aa98u,0x12835b01u,0x243185beu,0x550c7dc3u,0x72be5d74u,0x80deb1feu,0x9bdc06a7u,0xc19bf174u,
0xe49b69c1u,0xefbe4786u,0x0fc19dc6u,0x240ca1ccu,0x2de92c6fu,0x4a7484aau,0x5cb0a9dcu,0x76f988dau,
0x983e5152u,0xa831c66du,0xb00327c8u,0xbf597fc7u,0xc6e00bf3u,0xd5a79147u,0x06ca6351u,0x14292967u,
0x27b70a85u,0x2e1b2138u,0x4d2c6dfcu,0x53380d13u,0x650a7354u,0x766a0abbu,0x81c2c92eu,0x92722c85u,
0xa2bfe8a1u,0xa81a664bu,0xc24b8b70u,0xc76c51a3u,0xd192e819u,0xd6990624u,0xf40e3585u,0x106aa070u,
0x19a4c116u,0x1e376c08u,0x2748774cu,0x34b0bcb5u,0x391c0cb3u,0x4ed8aa4fu,0x5b9cca4fu,0x682e6ff3u,
0x748f82eeu,0x78a5636fu,0x84c87814u,0x8cc70208u,0x90befffau,0xa4506cebu,0xbef9a3f7u,0xc67178f2u};
static inline u32 rotr32(u32 x,u32 n){return (x>>n)|(x<<(32u-n));}
static void sha_block(u32 h[8],const u8 *p){
    u32 w[64];
    for(u32 i=0;i<16;++i) w[i]=((u32)p[4*i]<<24)|((u32)p[4*i+1]<<16)|((u32)p[4*i+2]<<8)|p[4*i+3];
    for(u32 i=16;i<64;++i){u32 s0=rotr32(w[i-15],7)^rotr32(w[i-15],18)^(w[i-15]>>3);u32 s1=rotr32(w[i-2],17)^rotr32(w[i-2],19)^(w[i-2]>>10);w[i]=w[i-16]+s0+w[i-7]+s1;}
    u32 a=h[0],b=h[1],c=h[2],d=h[3],e=h[4],f=h[5],g=h[6],hh=h[7];
    for(u32 i=0;i<64;++i){u32 S1=rotr32(e,6)^rotr32(e,11)^rotr32(e,25);u32 ch=(e&f)^((~e)&g);u32 t1=hh+S1+ch+SHA_K[i]+w[i];u32 S0=rotr32(a,2)^rotr32(a,13)^rotr32(a,22);u32 maj=(a&b)^(a&c)^(b&c);u32 t2=S0+maj;hh=g;g=f;f=e;e=d+t1;d=c;c=b;b=a;a=t1+t2;}
    h[0]+=a;h[1]+=b;h[2]+=c;h[3]+=d;h[4]+=e;h[5]+=f;h[6]+=g;h[7]+=hh;
}
static void sha256(const u8 *p,usize n,u8 out[32]){
    u32 h[8]={0x6a09e667u,0xbb67ae85u,0x3c6ef372u,0xa54ff53au,0x510e527fu,0x9b05688cu,0x1f83d9abu,0x5be0cd19u};
    usize full=n&~(usize)63; for(usize o=0;o<full;o+=64) sha_block(h,p+o);
    u8 b[128]; zero_bytes(b,128); usize tail=n-full; for(usize i=0;i<tail;++i)b[i]=p[full+i]; b[tail]=0x80;
    usize used=(tail<56)?64:128; unsigned long long bits=(unsigned long long)n*8ull;
    for(u32 i=0;i<8;++i)b[used-1-i]=(u8)(bits>>(8u*i)); sha_block(h,b); if(used==128)sha_block(h,b+64);
    for(u32 i=0;i<8;++i){out[4*i]=(u8)(h[i]>>24);out[4*i+1]=(u8)(h[i]>>16);out[4*i+2]=(u8)(h[i]>>8);out[4*i+3]=(u8)h[i];}
}

// Legacy DXBC checksum used by D3D shader containers (same algorithm as the audited P2.2 builder).
static const u32 MD5_K[64]={
0xd76aa478u,0xe8c7b756u,0x242070dbu,0xc1bdceeeu,0xf57c0fafu,0x4787c62au,0xa8304613u,0xfd469501u,
0x698098d8u,0x8b44f7afu,0xffff5bb1u,0x895cd7beu,0x6b901122u,0xfd987193u,0xa679438eu,0x49b40821u,
0xf61e2562u,0xc040b340u,0x265e5a51u,0xe9b6c7aau,0xd62f105du,0x02441453u,0xd8a1e681u,0xe7d3fbc8u,
0x21e1cde6u,0xc33707d6u,0xf4d50d87u,0x455a14edu,0xa9e3e905u,0xfcefa3f8u,0x676f02d9u,0x8d2a4c8au,
0xfffa3942u,0x8771f681u,0x6d9d6122u,0xfde5380cu,0xa4beea44u,0x4bdecfa9u,0xf6bb4b60u,0xbebfbc70u,
0x289b7ec6u,0xeaa127fau,0xd4ef3085u,0x04881d05u,0xd9d4d039u,0xe6db99e5u,0x1fa27cf8u,0xc4ac5665u,
0xf4292244u,0x432aff97u,0xab9423a7u,0xfc93a039u,0x655b59c3u,0x8f0ccc92u,0xffeff47du,0x85845dd1u,
0x6fa87e4fu,0xfe2ce6e0u,0xa3014314u,0x4e0811a1u,0xf7537e82u,0xbd3af235u,0x2ad7d2bbu,0xeb86d391u};
static const u8 MD5_S[64]={7,12,17,22,7,12,17,22,7,12,17,22,7,12,17,22,5,9,14,20,5,9,14,20,5,9,14,20,5,9,14,20,4,11,16,23,4,11,16,23,4,11,16,23,4,11,16,23,6,10,15,21,6,10,15,21,6,10,15,21,6,10,15,21};
static inline u32 rol32(u32 x,u32 n){return (x<<n)|(x>>(32u-n));}
static void md5_transform(u32 st[4],const u8 *block){
    u32 M[16]; for(u32 i=0;i<16;++i)M[i]=rd32(block+4*i);
    u32 a=st[0],b=st[1],c=st[2],d=st[3],A=a,B=b,C=c,D=d;
    for(u32 i=0;i<64;++i){u32 f;u32 g;if(i<16){f=(b&c)|((~b)&d);g=i;}else if(i<32){f=(d&b)|((~d)&c);g=(5*i+1)%16;}else if(i<48){f=b^c^d;g=(3*i+5)%16;}else{f=c^(b|(~d));g=(7*i)%16;}u32 oldd=d;d=c;c=b;b=b+rol32(a+f+MD5_K[i]+M[g],MD5_S[i]);a=oldd;}
    st[0]=A+a;st[1]=B+b;st[2]=C+c;st[3]=D+d;
}
static bool fix_dxbc_checksum(u8 *v,usize n){
    if(n<0x20||v[0]!='D'||v[1]!='X'||v[2]!='B'||v[3]!='C')return false;
    const u8 *p=v+0x14;usize sz=n-0x14;unsigned long long bit=(unsigned long long)sz*8ull;u32 st[4]={0x67452301u,0xefcdab89u,0x98badcfeu,0x10325476u};
    usize full=sz&~(usize)63;for(usize o=0;o<full;o+=64)md5_transform(st,p+o);usize tail=sz-full;u8 b[64];zero_bytes(b,64);
    if(tail>=56){for(usize i=0;i<tail;++i)b[i]=p[full+i];b[tail]=0x80;md5_transform(st,b);zero_bytes(b,64);wr32(b,(u32)bit);wr32(b+60,(u32)((bit>>2)|1ull));md5_transform(st,b);}
    else{wr32(b,(u32)bit);for(usize i=0;i<tail;++i)b[4+i]=p[full+i];b[4+tail]=0x80;wr32(b+60,(u32)((bit>>2)|1ull));md5_transform(st,b);}
    for(u32 i=0;i<4;++i)wr32(v+4+4*i,st[i]);return true;
}

static bool is_candidate_size(usize n){for(u32 i=0;i<kPlanCount;++i)if(kPlans[i].size==n)return true;return false;}
static const Plan *find_plan(usize n,const u8 digest[32]){for(u32 i=0;i<kPlanCount;++i){const Plan &p=kPlans[i];if(p.size==n&&bytes_eq(p.sha,digest,32))return &p;}return 0;}
static ShaderDesc *find_ps(u32 n,const PipelineSubobject *sub){if(!sub)return 0;for(u32 i=0;i<n;++i)if(sub[i].type==PIXEL_SHADER_SUBOBJECT&&sub[i].count==1&&sub[i].data)return (ShaderDesc*)sub[i].data;return 0;}
static char hx(u32 v){v&=15u;return (char)(v<10u?'0'+v:'A'+(v-10u));}
static char *put_s(char *d,const char *s){while(*s)*d++=*s++;return d;}
static char *put_h32(char *d,u32 v){for(int i=7;i>=0;--i)*d++=hx(v>>(i*4));return d;}
static char *put_h16(char *d,u32 v){for(int i=3;i>=0;--i)*d++=hx(v>>(i*4));return d;}
static char *put_sha8(char *d,const u8 *s){for(u32 i=0;i<8;++i){*d++=hx(s[i]>>4);*d++=hx(s[i]);}return d;}
static u32 plan_index_of(const Plan *p){ return (u32)(p-kPlans); }
static void log_plan_state(const char *tag,u32 pi,u32 reason){
    if(!g_log||pi>=kPlanCount)return; const Plan &p=kPlans[pi]; char b[208]; char *q=b;
    q=put_s(q,"DSRRL MRX A1 ");q=put_s(q,tag);q=put_s(q," plan=0x");q=put_h32(q,pi);
    q=put_s(q," mask=0x");q=put_h16(q,p.mask);q=put_s(q," reason=0x");q=put_h32(q,reason);
    q=put_s(q," sha=");q=put_sha8(q,p.sha);*q=0; g_log(g_addon_module,3,b);
}
static const u8 *materialize_plan(u32 pi,const void *src,usize n){
    if(pi>=kPlanCount||!src)return 0; PlanStorage &st=g_plan_store[pi]; const Plan &p=kPlans[pi];
    for(;;){
        u32 state=__atomic_load_n(&st.state,__ATOMIC_ACQUIRE);
        if(state==2u)return st.data;
        if(state==3u)return 0;
        if(state==0u){
            u32 expected=0u;
            if(!__atomic_compare_exchange_n(&st.state,&expected,1u,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED))continue;
            u32 reason=0u;
            if(n!=p.size||n>SCRATCH_SIZE)reason=1u;
            if(!reason){copy_bytes(st.data,src,n);
                for(u32 i=0;i<p.op_count;++i){const PatchOp &op=p.ops[i];
                    if((usize)op.off+4ull>n){reason=2u;break;}
                    if(rd32(st.data+op.off)!=op.oldw){reason=3u;break;}
                    wr32(st.data+op.off,op.neww);
                }}
            if(!reason&&!fix_dxbc_checksum(st.data,n))reason=4u;
            if(!reason){u8 repl[32];sha256(st.data,n,repl);if(!bytes_eq(repl,p.repl_sha,32))reason=5u;}
            if(reason){st.fail_reason=reason;__atomic_store_n(&st.state,3u,__ATOMIC_RELEASE);log_plan_state("FAIL",pi,reason);return 0;}
            __atomic_store_n(&st.state,2u,__ATOMIC_RELEASE);log_plan_state("READY",pi,0u);return st.data;
        }
        __builtin_ia32_pause();
    }
}
static void log_plan_bind(u32 pi){
    if(!g_log||pi>=kPlanCount)return;
    if(__atomic_exchange_n(&g_plan_bind_logged[pi],1u,__ATOMIC_ACQ_REL)!=0u)return;
    const Plan &p=kPlans[pi]; char b[192]; char *q=b;
    q=put_s(q,"DSRRL MRX A1 BIND plan=0x");q=put_h32(q,pi);
    q=put_s(q," mask=0x");q=put_h16(q,p.mask);
    q=put_s(q," size=0x");q=put_h32(q,p.size);
    q=put_s(q," sha=");q=put_sha8(q,p.sha);*q=0;
    g_log(g_addon_module,3,b);
}
static void map_pipeline(u64 pipeline,u32 plan_index){
    if(!pipeline||plan_index>=kPlanCount)return;
    for(u32 i=0;i<PIPE_MAP_COUNT;++i){
        u64 cur=__atomic_load_n(&g_pipe_map[i].pipeline,__ATOMIC_ACQUIRE);
        if(cur==pipeline){g_pipe_map[i].plan_index=plan_index;return;}
        if(cur==0ull){u64 expected=0ull;if(__atomic_compare_exchange_n(&g_pipe_map[i].pipeline,&expected,pipeline,false,__ATOMIC_ACQ_REL,__ATOMIC_RELAXED)){g_pipe_map[i].plan_index=plan_index;return;}}
    }
}
static void map_storage_code(const void *code,u64 pipeline){
    if(!code)return;
    for(u32 i=0;i<kPlanCount;++i)if(code==(const void*)g_plan_store[i].data&&__atomic_load_n(&g_plan_store[i].state,__ATOMIC_ACQUIRE)==2u){map_pipeline(pipeline,i);return;}
}
static void on_bind_pipeline(void *,u32 stages,u64 pipeline){
    if((stages&PIXEL_PIPELINE_STAGE)==0u||!pipeline)return;
    for(u32 i=0;i<PIPE_MAP_COUNT;++i){u64 cur=__atomic_load_n(&g_pipe_map[i].pipeline,__ATOMIC_ACQUIRE);if(cur==pipeline){log_plan_bind(g_pipe_map[i].plan_index);return;}}
}

static bool on_create_pipeline(void *,u64,u32 n,const PipelineSubobject *sub){
    ShaderDesc *ps=find_ps(n,sub); if(!ps||!ps->code||ps->code_size==0||ps->code_size>SCRATCH_SIZE)return false;
    if(!is_candidate_size(ps->code_size))return false;
    u8 digest[32]; sha256((const u8*)ps->code,ps->code_size,digest); const Plan *plan=find_plan(ps->code_size,digest); if(!plan)return false;
    const u32 pi=plan_index_of(plan); const u8 *replacement=materialize_plan(pi,ps->code,ps->code_size); if(!replacement)return false;
    ps->code=replacement;
    if(g_log && __atomic_exchange_n(&g_active_logged,1u,__ATOMIC_ACQ_REL)==0u) g_log(g_addon_module,3,"DSRRL Material Response Expanded A1: FIRST BRIDGE ACTIVE.");
    return true;
}
static void on_init_pipeline(void *,u64,u32 n,const PipelineSubobject *sub,u64 pipeline){ShaderDesc *ps=find_ps(n,sub);if(ps)map_storage_code(ps->code,pipeline);}

extern "C" __declspec(dllexport) const char *NAME="DSRRL Material Response 1.45 Expanded A1";
extern "C" __declspec(dllexport) const char *AUTHOR="DSR Restored Lighting";
extern "C" __declspec(dllexport) const char *DESCRIPTION="Expanded PTDE material-response diagnostic across global non-protected PntS/PntSS/PntSSSS receiver families; persistent per-plan materialization; EnvSpec only deleted on certified PTDE no-EnvSpec homologs; shared Phn DifSpc* hosts fail open pending material routing.";

extern "C" __declspec(dllexport) bool AddonInit(void *addon_module,void *reshade_module){
    g_addon_module=addon_module;g_reshade_module=reshade_module;
    g_register_addon=(PFN_RegisterAddon)find_export(reshade_module,"ReShadeRegisterAddon");
    g_unregister_addon=(PFN_UnregisterAddon)find_export(reshade_module,"ReShadeUnregisterAddon");
    g_register_event=(PFN_RegisterEvent)find_export(reshade_module,"ReShadeRegisterEvent");
    g_unregister_event=(PFN_UnregisterEvent)find_export(reshade_module,"ReShadeUnregisterEvent");
    g_log=(PFN_LogMessage)find_export(reshade_module,"ReShadeLogMessage");
    if(!g_register_addon||!g_unregister_addon||!g_register_event||!g_unregister_event)return false;
    if(!g_register_addon(addon_module,API_VERSION))return false;
    g_register_event(EVT_CREATE_PIPELINE,(void*)&on_create_pipeline);
    g_register_event(EVT_INIT_PIPELINE,(void*)&on_init_pipeline);
    g_register_event(EVT_BIND_PIPELINE,(void*)&on_bind_pipeline);
    if(g_log)g_log(addon_module,3,"DSRRL Material Response Expanded A1: armed 144 CONFIRMED+CLOSED global-safe plans / 252 aliases; PntS/PntSS/PntSSSS enabled; shared Phn DifSpc* fail-open; EnvSpec replacement OFF; certified no-EnvSpec delete only; persistent storage.");
    return true;
}
extern "C" __declspec(dllexport) void AddonUninit(void *addon_module,void *){
    if(g_unregister_event){g_unregister_event(EVT_BIND_PIPELINE,(void*)&on_bind_pipeline);g_unregister_event(EVT_INIT_PIPELINE,(void*)&on_init_pipeline);g_unregister_event(EVT_CREATE_PIPELINE,(void*)&on_create_pipeline);}if(g_unregister_addon)g_unregister_addon(addon_module);
}
