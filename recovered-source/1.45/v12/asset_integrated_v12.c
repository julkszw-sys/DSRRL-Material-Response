
typedef unsigned char u8;
typedef unsigned short u16;
typedef unsigned int u32;
typedef unsigned long long u64;
typedef long long i64;
typedef u64 uptr;

#define TARGET_BASE_SENTINEL 0x7A6B5C4D3E2F1908ULL
#define RVA_STATE_GET 0x113308ULL
#define RVA_CMD_LOOKUP 0x113494ULL
#define RVA_EXACT_LOOKUP 0x113AC2ULL
#define RVA_ENSURE_ASSET 0x11350CULL
#define RVA_RESTORE_T10 0x113B45ULL
#define RVA_ORIGINAL_PUSH 0x1133D9ULL
#define RVA_TLS_INDEX 0x107B50ULL
#define RVA_REGISTRY 0x81E80ULL
#define REGISTRY_STRIDE 0x48ULL
#define REGISTRY_FLAG_OFF 0x44ULL
#define RVA_ASSET_MAP 0x112980ULL
#define ASSET_MAP_COUNT 32u
#define RVA_LOG_CONTEXT 0x112008ULL
#define RVA_LOG_FN 0x112010ULL
#define RVA_TELEM_FLAGS 0x112F80ULL
#define TELEM_CAPTURE_SPEC 0u
#define TELEM_CAPTURE_NORMAL 1u
#define TELEM_CAPTURE_DIFFUSE 2u
#define TELEM_DIFF_GATE 3u
#define TELEM_DIFF_READY 4u
#define TELEM_DIFF_BIND 5u
#define TELEM_NORMAL_GATE 6u
#define TELEM_NORMAL_READY 7u
#define TELEM_NORMAL_BIND 8u
#define TELEM_DIAG_DIFF_RX_REJECT 9u
#define TELEM_DIAG_DIFF_RX_PASS 10u
#define TELEM_DIAG_DIFF_EXACT_PASS 11u
#define TELEM_DIAG_DIFF_TUPLE_REJECT 12u
#define TELEM_DIAG_NORM_RX_REJECT 13u
#define TELEM_DIAG_NORM_RX_PASS 14u
#define TELEM_DIAG_NORM_EXACT_PASS 15u
#define TELEM_DIAG_NORM_TUPLE_REJECT 16u
#define TELEM_RX_DIFF_TLS_ARRAY_NULL 17u
#define TELEM_RX_DIFF_TLS_BLOCK_NULL 18u
#define TELEM_RX_DIFF_NEGATIVE 19u
#define TELEM_RX_DIFF_0_23 20u
#define TELEM_RX_DIFF_24_35 21u
#define TELEM_RX_DIFF_36_47 22u
#define TELEM_RX_DIFF_48_PLUS 23u
#define TELEM_RX_NORM_TLS_ARRAY_NULL 24u
#define TELEM_RX_NORM_TLS_BLOCK_NULL 25u
#define TELEM_RX_NORM_NEGATIVE 26u
#define TELEM_RX_NORM_0_23 27u
#define TELEM_RX_NORM_24_35 28u
#define TELEM_RX_NORM_36_47 29u
#define TELEM_RX_NORM_48_PLUS 30u
#define LOAD_NONE 0u
#define LOAD_NORMAL 1u
#define LOAD_DIFFUSE 2u
#define F_NORMAL_BOUND 1u
#define F_DIFF_READY 2u
#define F_DIFF_BOUND 4u

struct update_desc { u64 table; u32 binding; u32 array_offset; u32 count; u32 type; const u64 *descriptors; };
struct asset_entry { uptr native_ctx; u64 current_t0; u64 current_t2; u64 prepared_t0; u64 loading_resource; u32 flags; u32 load_mode; };
struct pair_hash { u64 spec; u64 asset; };
struct triple_hash { u64 diffuse; u64 spec; u64 bump; };

void asset_push_wrapper(void *cmd, u32 stages, u64 layout, u32 layout_param, const struct update_desc *u);

__attribute__((noinline)) static uptr module_base(void) { uptr p; __asm__("leaq asset_push_wrapper(%%rip), %0" : "=r"(p)); return p-(uptr)TARGET_BASE_SENTINEL; }
static uptr native_from_cmd(void *cmd) { if(!cmd)return 0; void **vt=*(void ***)cmd; if(!vt||!vt[0])return 0; typedef uptr(*fn_t)(void*); return ((fn_t)vt[0])(cmd); }
static void log_once(uptr base,u32 slot,const char *msg) {
 if(!base||!msg)return; void *ctx=*(void**)(base+RVA_LOG_CONTEXT); typedef void(*log_t)(void*,u32,const char*); log_t fn=*(log_t*)(base+RVA_LOG_FN); if(!ctx||!fn)return;
 volatile u32 *flag=(volatile u32*)(base+RVA_TELEM_FLAGS+(uptr)slot*4); if(__sync_val_compare_and_swap(flag,0,1)==0)fn(ctx,3,msg);
}
static struct asset_entry *find_asset(uptr base, uptr native_ctx, int create) {
 if(!native_ctx)return (struct asset_entry*)0; struct asset_entry *tab=(struct asset_entry*)(base+RVA_ASSET_MAP);
 u32 start=(u32)(((native_ctx>>4)^(native_ctx>>11)^(native_ctx>>19))&(ASSET_MAP_COUNT-1));
 for(u32 i=0;i<ASSET_MAP_COUNT;++i){ struct asset_entry *e=&tab[(start+i)&(ASSET_MAP_COUNT-1)]; uptr key=e->native_ctx; if(key==native_ctx)return e; if(key==0&&create){
   /* Claim with sentinel 1, initialize, then publish native_ctx. This keeps deferred-context first-use races fail-safe without a CRT lock. */
   if(__sync_val_compare_and_swap(&e->native_ctx,0,1)==0){e->current_t0=e->current_t2=e->prepared_t0=e->loading_resource=0;e->flags=e->load_mode=0;__sync_synchronize();e->native_ctx=native_ctx;return e;}
  } } return 0;
}
static u64 fnv_name(const u16 *s,u32 n) { u64 h=14695981039346656037ULL; for(u32 i=0;i<n;++i){u32 c=s[i];if(c>='A'&&c<='Z')c+=32;h^=(u64)c;h*=1099511628211ULL;} return h; }
static int u16_eq_ascii(const u16 *s,u32 n,const char*a){u32 i=0;for(;i<n&&a[i];++i){u32 c=s[i],d=(u8)a[i];if(c>='A'&&c<='Z')c+=32;if(d>='A'&&d<='Z')d+=32;if(c!=d)return 0;}return i==n&&a[i]==0;}

static const struct triple_hash k_normal_triples[] = {
    { 0x003edc3c61d5b42bULL, 0xc3161f1793254f9dULL, 0xc3162217932554b6ULL }, /* lg_f_5370_lg_m_5370 | lg_f_5370_lg_m_5370_s | lg_f_5370_lg_m_5370_n (10) */
    { 0x00b515c367a88e31ULL, 0xaedd13872d25cffbULL, 0xaedcfe872d25ac4cULL }, /* hd_a_9550 | hd_a_9550_s | hd_a_9550_n (2) */
    { 0x00b89bc367aba7baULL, 0x63ca919020287584ULL, 0x63ca969020287e03ULL }, /* hd_a_9560 | hd_a_9560_s | hd_a_9560_n (4) */
    { 0x00bc01c367ae8ae3ULL, 0x02add798757032e5ULL, 0x02adea987570532eULL }, /* hd_a_9570 | hd_a_9570_s | hd_a_9570_n (2) */
    { 0x00bf87c367b1a46cULL, 0xb79b55a16872d86eULL, 0xb79b42a16872b825ULL }, /* hd_a_9500 | hd_a_9500_s | hd_a_9500_n (2) */
    { 0x019dea2c07dd144aULL, 0x6f8726c0736122d4ULL, 0x6f870bc07360f4f3ULL }, /* am_m_9540 | am_m_9540_s | am_m_9540_n (2) */
    { 0x01a85c2c07e62a85ULL, 0x7baa68dab18fa147ULL, 0x7baa4bdab18f7000ULL }, /* am_m_9530 | am_m_9530_s | am_m_9530_n (6) */
    { 0x01aee82c07eb8417ULL, 0x90c6ccea2393bb11ULL, 0x90c6d7ea2393cdc2ULL }, /* am_m_9510 | am_m_9510_s | am_m_9510_n (6) */
    { 0x01b3953ef3405cb0ULL, 0x696c2a47c19558eaULL, 0x696c2f47c1956169ULL }, /* bd_m_9300_1 | bd_m_9300_1_s | bd_m_9300_1_n (6) */
    { 0x01b3983ef34061c9ULL, 0x81e48d47cf031963ULL, 0x81e48847cf0310e4ULL }, /* bd_m_9300_2 | bd_m_9300_2_s | bd_m_9300_2_n (10) */
    { 0x01fa980ab8aae51bULL, 0x7d2ed7cce732c74dULL, 0x7d2ebacce7329606ULL }, /* lg_f_9550_lg_m_9550 | lg_f_9550_lg_m_9550_s | lg_f_9550_lg_m_9550_n (10) */
    { 0x023e86468b8e3f83ULL, 0x46a6a127fc2001c5ULL, 0x46a6b427fc20220eULL }, /* bd_m_9710_bd_f_9710_1 | bd_m_9710_bd_f_9710_1_s | bd_m_9710_bd_f_9710_1_n (8) */
    { 0x023e87468b8e4136ULL, 0x4eced228009a07c8ULL, 0x4ecebf280099e77fULL }, /* bd_m_9710_bd_f_9710_2 | bd_m_9710_bd_f_9710_2_s | bd_m_9710_bd_f_9710_2_n (4) */
    { 0x0284be4d34f2f9d3ULL, 0x54acd536e1489895ULL, 0x54acc836e148827eULL }, /* lg_f_9560_lg_m_9560 | lg_f_9560_lg_m_9560_s | lg_f_9560_lg_m_9560_n (10) */
    { 0x03662a75ea919557ULL, 0x3868b404d2878351ULL, 0x3868bf04d2879602ULL }, /* lg_f_9260_lg_m_9260 | lg_f_9260_lg_m_9260_s | lg_f_9260_lg_m_9260_n (8) */
    { 0x04efcec4cd60a667ULL, 0x812cbd62e44e9da1ULL, 0x812ca862e44e79f2ULL }, /* lg_f_2570_lg_m_2570 | lg_f_2570_lg_m_2570_s | lg_f_2570_lg_m_2570_n (9) */
    { 0x061b7a5a0cc9cfdfULL, 0x23c5fe569e517ee9ULL, 0x23c5f9569e51766aULL }, /* bd_f_9710_2 | bd_f_9710_2_s | bd_f_9710_2_n (4) */
    { 0x061b7b5a0cc9d192ULL, 0x2e7a2f56a4f50a2cULL, 0x2e7a4456a4f52ddbULL }, /* bd_f_9710_1 | bd_f_9710_1_s | bd_f_9710_1_n (8) */
    { 0x07177b1cc47f41d3ULL, 0x63e83dd6d2982095ULL, 0x63e830d6d2980a7eULL }, /* bd_a_8200_2 | bd_a_8200_2_s | bd_a_8200_2_n (6) */
    { 0x07177c1cc47f4386ULL, 0x6e9d6ed6d93d5ed8ULL, 0x6e9d7bd6d93d74efULL }, /* bd_a_8200_1 | bd_a_8200_1_s | bd_a_8200_1_n (12) */
    { 0x076811c4e36ce91fULL, 0xbba5a80bd1c50529ULL, 0xbba5a30bd1c4fcaaULL }, /* bd_f_9220_bd_m_9220_1 | bd_f_9220_bd_m_9220_1_s | bd_f_9220_bd_m_9220_1_n (4) */
    { 0x076812c4e36cead2ULL, 0xc2f4d90bd5871a6cULL, 0xc2f4ee0bd5873e1bULL }, /* bd_f_9220_bd_m_9220_2 | bd_f_9220_bd_m_9220_2_s | bd_f_9220_bd_m_9220_2_n (4) */
    { 0x07cffd6b2cb194ffULL, 0x27f36b2ec87a2ac9ULL, 0x27f3662ec87a224aULL }, /* bd_m_9521_bd_f_9521_1 | bd_m_9521_bd_f_9521_1_s | bd_m_9521_bd_f_9521_1_n (16) */
    { 0x07cffe6b2cb196b2ULL, 0x32a89c2ecf1f690cULL, 0x32a8b12ecf1f8cbbULL }, /* bd_m_9521_bd_f_9521_2 | bd_m_9521_bd_f_9521_2_s | bd_m_9521_bd_f_9521_2_n (10) */
    { 0x07e098b4b30a43c0ULL, 0x7d98a0c74dd924baULL, 0x7d9885c74dd8f6d9ULL }, /* bd_a_9490_1 | bd_a_9490_1_s | bd_a_9490_1_n (8) */
    { 0x07e09bb4b30a48d9ULL, 0x96eb03c75c0088f3ULL, 0x96eb1ec75c00b6d4ULL }, /* bd_a_9490_2 | bd_a_9490_2_s | bd_a_9490_2_n (6) */
    { 0x07fa0359694cdde3ULL, 0x38421f4ac1647de5ULL, 0x3842324ac1649e2eULL }, /* am_f_5370_am_m_5370 | am_f_5370_am_m_5370_s | am_f_5370_am_m_5370_n (2) */
    { 0x084f157c8ac84e6bULL, 0x5975809273d455ddULL, 0x5975839273d45af6ULL }, /* lg_m_9389_lg_m_9380 | lg_m_9389_lg_m_9380_s | lg_m_9389_lg_m_9380_n (6) */
    { 0x086daa148a0a03bbULL, 0xf4c212c504d548adULL, 0xf4c1f5c504d51766ULL }, /* lg_f_5320_lg_m_5320 | lg_f_5320_lg_m_5320_s | lg_f_5320_lg_m_5320_n (4) */
    { 0x095c0a0aaf526f4fULL, 0x562ae6d10acf19d9ULL, 0x562b01d10acf47baULL }, /* hd_m_9389_hd_m_9380 | hd_m_9389_hd_m_9380_s | hd_m_9389_hd_m_9380_n (4) */
    { 0x09631719200afad7ULL, 0xdbf6bd85d2f944d1ULL, 0xdbf6c885d2f95782ULL }, /* bd_m_9420_1 | bd_m_9420_1_s | bd_m_9420_1_n (8) */
    { 0x09631819200afc8aULL, 0xe6aaee85d99cd014ULL, 0xe6aad385d99ca233ULL }, /* bd_m_9420_2 | bd_m_9420_2_s | bd_m_9420_2_n (8) */
    { 0x09b72cc36cdc0bc6ULL, 0x23f7d18ba4b7c318ULL, 0x23f7de8ba4b7d92fULL }, /* hd_a_9490 | hd_a_9490_s | hd_a_9490_n (4) */
    { 0x09bdb8c36ce16558ULL, 0x3914359b16bbdce2ULL, 0x39142a9b16bbca31ULL }, /* hd_a_9470 | hd_a_9470_s | hd_a_9470_n (2) */
    { 0x0a51f72c0cce5f90ULL, 0x9d3e4a05cba0020aULL, 0x9d3e4f05cba00a89ULL }, /* am_m_9410 | am_m_9410_s | am_m_9410_n (2) */
    { 0x0a54fd2c0cd09f99ULL, 0xfd6d280c4aa168b3ULL, 0xfd6d430c4aa19694ULL }, /* am_m_9400 | am_m_9400_s | am_m_9400_n (10) */
    { 0x0a58832c0cd3b922ULL, 0xb333a6153e5bfefcULL, 0xb3339b153e5bec4bULL }, /* am_m_9430 | am_m_9430_s | am_m_9430_n (2) */
    { 0x0a5c092c0cd6d2abULL, 0x68212c1e315eb21dULL, 0x68212f1e315eb736ULL }, /* am_m_9420 | am_m_9420_s | am_m_9420_n (2) */
    { 0x0a5f8f2c0cd9ec34ULL, 0x1d0d8a27245f6e46ULL, 0x1d0da727245f9f8dULL }, /* am_m_9450 | am_m_9450_s | am_m_9450_n (4) */
    { 0x0a62952c0cdc2c3dULL, 0x7d3d882da362be4fULL, 0x7d3d7b2da362a838ULL }, /* am_m_9440 | am_m_9440_s | am_m_9440_n (4) */
    { 0x0a69812c0ce228efULL, 0xd10e4c3eebad2139ULL, 0xd10e673eebad4f1aULL }, /* am_m_9460 | am_m_9460_s | am_m_9460_n (4) */
    { 0x0a7aeb6afc0bada5ULL, 0x348514b815176e27ULL, 0x3484f7b815173ce0ULL }, /* lg_m_9379_lg_m_9372 | lg_m_9379_lg_m_9372_s | lg_m_9379_lg_m_9372_n (2) */
    { 0x0bd3ac6f2051d557ULL, 0x09a4eb186711c351ULL, 0x09a4f6186711d602ULL }, /* lg_f_9740_lg_m_9740 | lg_f_9740_lg_m_9740_s | lg_f_9740_lg_m_9740_n (8) */
    { 0x0be9f79f25cef827ULL, 0xaeb095a18567b061ULL, 0xaeb080a185678cb2ULL }, /* hd_f_9400_hd_m_9400 | hd_f_9400_hd_m_9400_s | hd_f_9400_hd_m_9400_n (2) */
    { 0x0c5a84de310d8b64ULL, 0x4f37222ff41683f6ULL, 0x4f371f2ff4167eddULL }, /* bd_f_9521_1 | bd_f_9521_1_s | bd_f_9521_1_n (16) */
    { 0x0c5a87de310d907dULL, 0x6889a530023e1e8fULL, 0x68899830023e0878ULL }, /* bd_f_9521_2 | bd_f_9521_2_s | bd_f_9521_2_n (12) */
    { 0x10565b76fc8d9c9bULL, 0x2a9fac1b3e38aacdULL, 0x2a9f8f1b3e387986ULL }, /* bd_f_2520_bd_m_2520_1 | bd_f_2520_bd_m_2520_1_s | bd_f_2520_bd_m_2520_1_n (10) */
    { 0x10565c76fc8d9e4eULL, 0x31edbd1b41f8d6b0ULL, 0x31edba1b41f8d197ULL }, /* bd_f_2520_bd_m_2520_2 | bd_f_2520_bd_m_2520_2_s | bd_f_2520_bd_m_2520_2_n (12) */
    { 0x127bd7c371db23b9ULL, 0x31ae32f8d4b95e93ULL, 0x31ae4df8d4b98c74ULL }, /* hd_a_9730 | hd_a_9730_s | hd_a_9730_n (2) */
    { 0x127f5dc371de3d42ULL, 0xe5c1b101c702605cULL, 0xe5c1a601c7024dabULL }, /* hd_a_9700 | hd_a_9700_s | hd_a_9700_n (8) */
    { 0x12d477ce8ff66ec7ULL, 0xd5b8fca96fb86841ULL, 0xd5b8e7a96fb84492ULL }, /* am_f_9540_am_m_9540 | am_f_9540_am_m_9540_s | am_f_9540_am_m_9540_n (2) */
    { 0x135da02c120976c0ULL, 0x887e6a2034f14fbaULL, 0x887e4f2034f121d9ULL }, /* am_m_9340 | am_m_9340_s | am_m_9340_n (4) */
    { 0x135da12c12097873ULL, 0x90a68320396b2cf5ULL, 0x90a67620396b16deULL }, /* am_m_9341 | am_m_9341_s | am_m_9341_n (4) */
    { 0x1361262c120c9049ULL, 0x3d6ac82927f20be3ULL, 0x3d6ac32927f20364ULL }, /* am_m_9350 | am_m_9350_s | am_m_9350_n (8) */
    { 0x1364ac2c120fa9d2ULL, 0xf25846321af4b16cULL, 0xf2585b321af4d51bULL }, /* am_m_9360 | am_m_9360_s | am_m_9360_n (4) */
    { 0x1368312c1212c1a8ULL, 0xa0cf933b0aed3272ULL, 0xa0cfa83b0aed5621ULL }, /* am_m_9371 | am_m_9371_s | am_m_9371_n (4) */
    { 0x1368322c1212c35bULL, 0xa744cc3b0df5b18dULL, 0xa744af3b0df58046ULL }, /* am_m_9370 | am_m_9370_s | am_m_9370_n (4) */
    { 0x1368342c1212c6c1ULL, 0xb948163b185b294bULL, 0xb948213b185b3bfcULL }, /* am_m_9372 | am_m_9372_s | am_m_9372_n (6) */
    { 0x136b382c12150364ULL, 0x084daa418db0bbf6ULL, 0x084da7418db0b6ddULL }, /* am_m_9300 | am_m_9300_s | am_m_9300_n (2) */
    { 0x1372242c121b0016ULL, 0x5eab8652d8267fe8ULL, 0x5eab7352d8265f9fULL }, /* am_m_9320 | am_m_9320_s | am_m_9320_n (2) */
    { 0x1386a82c122c896cULL, 0x324baa857930856eULL, 0x324b978579306525ULL }, /* am_m_9380 | am_m_9380_s | am_m_9380_n (2) */
    { 0x13a99dde34cf77dfULL, 0xd9002e095b3166e9ULL, 0xd90029095b315e6aULL }, /* bd_f_9520_1 | bd_f_9520_1_s | bd_f_9520_1_n (12) */
    { 0x13a99ede34cf7992ULL, 0xe3b45f0961d4f22cULL, 0xe3b4740961d515dbULL }, /* bd_f_9520_2 | bd_f_9520_2_s | bd_f_9520_2_n (12) */
    { 0x1604dc6d4b86f9d7ULL, 0x431b5187baf21bd1ULL, 0x431b5c87baf22e82ULL }, /* lg_f_9240_lg_m_9240 | lg_f_9240_lg_m_9240_s | lg_f_9240_lg_m_9240_n (8) */
    { 0x1638132e0d51bd87ULL, 0x398585d3191eb301ULL, 0x398570d3191e8f52ULL }, /* lg_f_2540_lg_m_2540 | lg_f_2540_lg_m_2540_s | lg_f_2540_lg_m_2540_n (6) */
    { 0x1ad91da62036966fULL, 0x8fc3c198bc4b2ab9ULL, 0x8fc3dc98bc4b589aULL }, /* lg_f_9310_lg_m_9310 | lg_f_9310_lg_m_9310_s | lg_f_9310_lg_m_9310_n (4) */
    { 0x1b17fbca50f5f598ULL, 0x73f2d61cfbe22522ULL, 0x73f2cb1cfbe21271ULL }, /* bd_a_9310_2 | bd_a_9310_2_s | bd_a_9310_2_n (10) */
    { 0x1b17feca50f5fab1ULL, 0x8e1e591d0ac1b07bULL, 0x8e1e441d0ac18cccULL }, /* bd_a_9310_1 | bd_a_9310_1_s | bd_a_9310_1_n (8) */
    { 0x1b9598c37722a10dULL, 0xf32e33370c3b38dfULL, 0xf32e46370c3b5928ULL }, /* hd_a_9600 | hd_a_9600_s | hd_a_9600_n (6) */
    { 0x1b98fec377258436ULL, 0x92eb913f623cc2c8ULL, 0x92eb7e3f623ca27fULL }, /* hd_a_9630 | hd_a_9630_s | hd_a_9630_n (2) */
    { 0x1c69c92c17456770ULL, 0xc9550a3d12f9e3aaULL, 0xc9550f3d12f9ec29ULL }, /* am_m_9270 | am_m_9270_s | am_m_9270_n (8) */
    { 0x1c6d4f2c174880f9ULL, 0x7e42684605fc52d3ULL, 0x7e42834605fc80b4ULL }, /* am_m_9260 | am_m_9260_s | am_m_9260_n (4) */
    { 0x1c9ce32c1770ed37ULL, 0x3fb82cbabe695df1ULL, 0x3fb837babe6970a2ULL }, /* am_m_9280 | am_m_9280_s | am_m_9280_n (2) */
    { 0x1d060f67201eb268ULL, 0x7615a43cc15e9332ULL, 0x7615b93cc15eb6e1ULL }, /* hd_a_2570 | hd_a_2570_s | hd_a_2570_n (2) */
    { 0x1d0915672020f271ULL, 0xd645a2434061e33bULL, 0xd6458d434061bf8cULL }, /* hd_a_2560 | hd_a_2560_s | hd_a_2560_n (2) */
    { 0x1e1c0ee70776882bULL, 0xa53dd88b5de3439dULL, 0xa53ddb8b5de348b6ULL }, /* hd_f_9260_hd_m_9260 | hd_f_9260_hd_m_9260_s | hd_f_9260_hd_m_9260_n (8) */
    { 0x1ea054d5a1af3334ULL, 0x254fbaf3a67ccd46ULL, 0x254fd7f3a67cfe8dULL }, /* bd_m_9530_2 | bd_m_9530_2_s | bd_m_9530_2_n (10) */
    { 0x1ea057d5a1af384dULL, 0x3dc93df3b3ec771fULL, 0x3dc950f3b3ec9768ULL }, /* bd_m_9530_1 | bd_m_9530_1_s | bd_m_9530_1_n (12) */
    { 0x1ef9e6b184cb26ebULL, 0xc07fefc4b542825dULL, 0xc07ff2c4b5428776ULL }, /* bd_f_9540_bd_m_9540_1 | bd_f_9540_bd_m_9540_1_s | bd_f_9540_bd_m_9540_1_n (20) */
    { 0x1ef9e7b184cb289eULL, 0xca5b00c4bb2de680ULL, 0xca5b1dc4bb2e17c7ULL }, /* bd_f_9540_bd_m_9540_2 | bd_f_9540_bd_m_9540_2_s | bd_f_9540_bd_m_9540_2_n (4) */
    { 0x2385515e48963d3bULL, 0x1edebea35346fe2dULL, 0x1edea1a35346cce6ULL }, /* lg_f_8200_lg_m_8200 | lg_f_8200_lg_m_8200_s | lg_f_8200_lg_m_8200_n (10) */
    { 0x24a930333bfa86c3ULL, 0xaab31958cdb47005ULL, 0xaab32c58cdb4904eULL }, /* hd_m_9230_hd_f_9230 | hd_m_9230_hd_f_9230_s | hd_m_9230_hd_f_9230_n (6) */
    { 0x250bbc0e68504c13ULL, 0xb209306e811bf2d5ULL, 0xb209236e811bdcbeULL }, /* bd_a_9250_2 | bd_a_9250_2_s | bd_a_9250_2_n (6) */
    { 0x250bbd0e68504dc6ULL, 0xb957616e84dc5518ULL, 0xb9576e6e84dc6b2fULL }, /* bd_a_9250_1 | bd_a_9250_1_s | bd_a_9250_1_n (10) */
    { 0x25945015dcad7dc0ULL, 0xc0da7a89296b6ebaULL, 0xc0da5f89296b40d9ULL }, /* bd_m_9260_2 | bd_m_9260_2_s | bd_m_9260_2_n (6) */
    { 0x25945315dcad82d9ULL, 0xda2cdd893792d2f3ULL, 0xda2cf889379300d4ULL }, /* bd_m_9260_1 | bd_m_9260_1_s | bd_m_9260_1_n (4) */
    { 0x25bec542f7d3cc9fULL, 0x88d25c2218f9f4a9ULL, 0x88d2572218f9ec2aULL }, /* am_a_9509_am_a_9500 | am_a_9509_am_a_9500_s | am_a_9509_am_a_9500_n (8) */
    { 0x25cdc06725200a64ULL, 0xe32203b06fa9daf6ULL, 0xe32200b06fa9d5ddULL }, /* hd_a_2660 | hd_a_2660_s | hd_a_2660_n (4) */
    { 0x273c74a0fa2e90f7ULL, 0x0cf5e2fc7d0969b1ULL, 0x0cf5edfc7d097c62ULL }, /* bd_m_9610_2 | bd_m_9610_2_s | bd_m_9610_2_n (6) */
    { 0x273c75a0fa2e92aaULL, 0x17ab13fc83aea7f4ULL, 0x17aaf8fc83ae7a13ULL }, /* bd_m_9610_1 | bd_m_9610_1_s | bd_m_9610_1_n (6) */
    { 0x28a0e5c74a89c277ULL, 0x22e69c169b88d731ULL, 0x22e6a7169b88e9e2ULL }, /* lg_f_9200_lg_m_9200 | lg_f_9200_lg_m_9200_s | lg_f_9200_lg_m_9200_n (6) */
    { 0x2c27e675c8e6488fULL, 0xdc1c5ecdf1116019ULL, 0xdc1c79cdf1118dfaULL }, /* am_f_9740_am_m_9740 | am_f_9740_am_m_9740_s | am_f_9740_am_m_9740_n (2) */
    { 0x2de3889f704521e3ULL, 0x1d799da08b7361e5ULL, 0x1d79b0a08b73822eULL }, /* am_f_9400_am_m_9400 | am_f_9400_am_m_9400_s | am_f_9400_am_m_9400_n (8) */
    { 0x2ef85f672a75c125ULL, 0xb0b2a217ba5f0da7ULL, 0xb0b28517ba5edc60ULL }, /* hd_a_2780 | hd_a_2780_s | hd_a_2780_n (8) */
    { 0x2f69bf742897212fULL, 0x773e6dc6c5ba5e79ULL, 0x773e88c6c5ba8c5aULL }, /* lg_f_9390_lg_m_9390 | lg_f_9390_lg_m_9390_s | lg_f_9390_lg_m_9390_n (8) */
    { 0x300118e112524c57ULL, 0x896236ef80a9d251ULL, 0x896241ef80a9e502ULL }, /* am_m_9389_am_m_9380 | am_m_9389_am_m_9380_s | am_m_9389_am_m_9380_n (2) */
    { 0x30d8ef7eae2c1cafULL, 0xf8707f133c9925f9ULL, 0xf8709a133c9953daULL }, /* bd_m_9389_bd_m_9380_1 | bd_m_9389_bd_m_9380_1_s | bd_m_9389_bd_m_9380_1_n (8) */
    { 0x30d8f07eae2c1e62ULL, 0xffbfb013405b3b3cULL, 0xffbfa513405b288bULL }, /* bd_m_9389_bd_m_9380_2 | bd_m_9389_bd_m_9380_2_s | bd_m_9389_bd_m_9380_2_n (10) */
    { 0x32bfcfcc4a063497ULL, 0xecf28467c55b7f91ULL, 0xecf28f67c55b9242ULL }, /* bd_m_5320_2 | bd_m_5320_2_s | bd_m_5320_2_n (4) */
    { 0x32bfd0cc4a06364aULL, 0xf441b567c91d94d4ULL, 0xf4419a67c91d66f3ULL }, /* bd_m_5320_1 | bd_m_5320_1_s | bd_m_5320_1_n (8) */
    { 0x345d79c6f7b1e0c7ULL, 0xbac660e426d3aa41ULL, 0xbac64be426d38692ULL }, /* bd_m_9230_bd_f_9230_1 | bd_m_9230_bd_f_9230_1_s | bd_m_9230_bd_f_9230_1_n (20) */
    { 0x345d7ac6f7b1e27aULL, 0xc2ee91e42b4db044ULL, 0xc2ee96e42b4db8c3ULL }, /* bd_m_9230_bd_f_9230_2 | bd_m_9230_bd_f_9230_2_s | bd_m_9230_bd_f_9230_2_n (12) */
    { 0x351e34268c10f350ULL, 0x4c662b815c5a7dcaULL, 0x4c6630815c5a8649ULL }, /* bd_m_9350_2 | bd_m_9350_2_s | bd_m_9350_2_n (6) */
    { 0x351e37268c10f869ULL, 0x66928e816b3b85c3ULL, 0x669289816b3b7d44ULL }, /* bd_m_9350_1 | bd_m_9350_1_s | bd_m_9350_1_n (8) */
    { 0x362308d1b667c8f2ULL, 0x820d4623cee9c74cULL, 0x820d5b23cee9eafbULL }, /* lg_f_9520 | lg_f_9520_s | lg_f_9520_n (10) */
    { 0x362309d1b667caa5ULL, 0x8cc27f23d58f1327ULL, 0x8cc26223d58ee1e0ULL }, /* lg_f_9521 | lg_f_9521_s | lg_f_9521_n (8) */
    { 0x36268ed1b66ae27bULL, 0x36facc2cc1ec7a6dULL, 0x36faaf2cc1ec4926ULL }, /* lg_f_9530 | lg_f_9530_s | lg_f_9530_n (7) */
    { 0x364ef4baf9acb8c4ULL, 0x9a7b63af95907b96ULL, 0x9a7b60af9590767dULL }, /* hd_a_8200 | hd_a_8200_s | hd_a_8200_n (2) */
    { 0x37f4c8c358cc635bULL, 0x2e3f9b9f3a90518dULL, 0x2e3f7e9f3a902046ULL }, /* lg_f_9730_lg_m_9730 | lg_f_9730_lg_m_9730_s | lg_f_9730_lg_m_9730_n (12) */
    { 0x380b94672fb7e4e7ULL, 0x5a8a26467db32021ULL, 0x5a8a11467db2fc72ULL }, /* hd_a_2870 | hd_a_2870_s | hd_a_2870_n (4) */
    { 0x3896d970ec2f49e4ULL, 0x3a6b3fe255698676ULL, 0x3a6b3ce25569815dULL }, /* lg_m_2870 | lg_m_2870_s | lg_m_2870_n (4) */
    { 0x38ddde7918e2aac8ULL, 0xe47daf3b4ebfc0d2ULL, 0xe47dc43b4ebfe481ULL }, /* am_m_2550 | am_m_2550_s | am_m_2550_n (4) */
    { 0x38e1647918e5c451ULL, 0x996a2d4441c0b35bULL, 0x996a184441c08facULL }, /* am_m_2540 | am_m_2540_s | am_m_2540_n (4) */
    { 0x38e1c5853bcd821bULL, 0x420bc8f8d2f4ec4dULL, 0x420babf8d2f4bb06ULL }, /* lg_m_9520_lg_f_9520 | lg_m_9520_lg_f_9520_s | lg_m_9520_lg_f_9520_n (10) */
    { 0x38f5e87918f74da7ULL, 0x70705176e5ade1e1ULL, 0x70703c76e5adbe32ULL }, /* am_m_2520 | am_m_2520_s | am_m_2520_n (4) */
    { 0x396e69fc26095983ULL, 0xb603562f15e52bc5ULL, 0xb603692f15e54c0eULL }, /* bd_a_2570_2 | bd_a_2570_2_s | bd_a_2570_2_n (6) */
    { 0x396e6afc26095b36ULL, 0xbe2b872f1a5f31c8ULL, 0xbe2b742f1a5f117fULL }, /* bd_a_2570_1 | bd_a_2570_1_s | bd_a_2570_1_n (4) */
    { 0x3b0f959565284b98ULL, 0x63d3b2f08633eb22ULL, 0x63d3a7f08633d871ULL }, /* bd_m_9740_1 | bd_m_9740_1_s | bd_m_9740_1_n (12) */
    { 0x3b0f9895652850b1ULL, 0x7dff35f09513767bULL, 0x7dff20f0951352ccULL }, /* bd_m_9740_2 | bd_m_9740_2_s | bd_m_9740_2_n (8) */
    { 0x3b1bc0ea5d89fe04ULL, 0x8bbfe48e545ac0d6ULL, 0x8bbfe18e545abbbdULL }, /* bd_f_9341_1 | bd_f_9341_1_s | bd_f_9341_1_n (8) */
    { 0x3b1bc3ea5d8a031dULL, 0xa512678e62825b6fULL, 0xa5125a8e62824558ULL }, /* bd_f_9341_2 | bd_f_9341_2_s | bd_f_9341_2_n (8) */
    { 0x3bfa0f55c609b0e4ULL, 0x63ec826ab0b90576ULL, 0x63ec7f6ab0b9005dULL }, /* am_m_6200 | am_m_6200_s | am_m_6200_n (4) */
    { 0x3cae991857d67defULL, 0xab0c3d763e2dbe39ULL, 0xab0c58763e2dec1aULL }, /* hd_f_9610_hd_m_9610 | hd_f_9610_hd_m_9610_s | hd_f_9610_hd_m_9610_n (4) */
    { 0x3d78abd1ba2f0b99ULL, 0x1254640ca084b4b3ULL, 0x12547f0ca084e294ULL }, /* lg_f_9420 | lg_f_9420_s | lg_f_9420_n (4) */
    { 0x3d8643d1ba3a983dULL, 0x9224c42df9460a4fULL, 0x9224b72df945f438ULL }, /* lg_f_9460 | lg_f_9460_s | lg_f_9460_n (6) */
    { 0x3d89a9d1ba3d7b66ULL, 0x310822364e8df078ULL, 0x31082f364e8e068fULL }, /* lg_f_9450 | lg_f_9450_s | lg_f_9450_n (8) */
    { 0x3d9aa7d1ba4beb33ULL, 0x5248c85ffec23bb5ULL, 0x5248bb5ffec2259eULL }, /* lg_f_9480 | lg_f_9480_s | lg_f_9480_n (20) */
    { 0x3dfbed368cdbc737ULL, 0x12c8f95affd447f1ULL, 0x12c9045affd45aa2ULL }, /* hd_f_9720_hd_m_9720 | hd_f_9720_hd_m_9720_s | hd_f_9720_hd_m_9720_n (2) */
    { 0x3e5fdb5f72c5166fULL, 0x7d061d98509daab9ULL, 0x7d063898509dd89aULL }, /* bd_m_9440_2_l | bd_m_9440_2_l_s | bd_m_9440_2_l_n (6) */
    { 0x3ecec2e6ba797bffULL, 0xc5deb6408d5329c9ULL, 0xc5deb1408d53214aULL }, /* bd_m_2560_1 | bd_m_2560_1_s | bd_m_2560_1_n (6) */
    { 0x3ecec3e6ba797db2ULL, 0xd093e74093f8680cULL, 0xd093fc4093f88bbbULL }, /* bd_m_2560_2 | bd_m_2560_2_s | bd_m_2560_2_n (2) */
    { 0x3f5039673370b7c1ULL, 0xc9909e059f19c24bULL, 0xc990a9059f19d4fcULL }, /* hd_a_2920 | hd_a_2920_s | hd_a_2920_n (6) */
    { 0x4033b500bbbeb8c4ULL, 0xb3278e15a0727b96ULL, 0xb3278b15a072767dULL }, /* bd_m_9410_1 | bd_m_9410_1_s | bd_m_9410_1_n (4) */
    { 0x4033b800bbbebdddULL, 0xcc7a1115ae9a162fULL, 0xcc7a0415ae9a0018ULL }, /* bd_m_9410_2 | bd_m_9410_2_s | bd_m_9410_2_n (3) */
    { 0x419bdda3abded5e3ULL, 0xbd22735ac9ad35e5ULL, 0xbd22865ac9ad562eULL }, /* lg_m_8200 | lg_m_8200_s | lg_m_8200_n (14) */
    { 0x4211fe35f99a54acULL, 0x0269092d7cf08daeULL, 0x0268f62d7cf06d65ULL }, /* bd_m_9371_1 | bd_m_9371_1_s | bd_m_9371_1_n (8) */
    { 0x42120135f99a59c5ULL, 0x1ae28c2d8a603787ULL, 0x1ae26f2d8a600640ULL }, /* bd_m_9371_2 | bd_m_9371_2_s | bd_m_9371_2_n (4) */
    { 0x421425857266671bULL, 0xc07bc69cee80994dULL, 0xc07ba99cee806806ULL }, /* lg_m_9309_lg_m_9300 | lg_m_9309_lg_m_9300_s | lg_m_9309_lg_m_9300_n (6) */
    { 0x430a40ff28f16893ULL, 0xc06a770e702f0355ULL, 0xc06a6a0e702eed3eULL }, /* bd_m_9480_bd_f_9480 | bd_m_9480_bd_f_9480_s | bd_m_9480_bd_f_9480_n (8) */
    { 0x43b9e4c41792d16bULL, 0x6b1e3e74986c50ddULL, 0x6b1e4174986c55f6ULL }, /* am_f_9610_am_m_9610 | am_f_9610_am_m_9610_s | am_f_9610_am_m_9610_n (2) */
    { 0x43dbb22a240cb887ULL, 0x4dadb1d07fa1e601ULL, 0x4dad9cd07fa1c252ULL }, /* bd_f_9610_bd_m_9610_1 | bd_f_9610_bd_m_9610_1_s | bd_f_9610_bd_m_9610_1_n (6) */
    { 0x43dbb32a240cba3aULL, 0x55d5e2d0841bec04ULL, 0x55d5e7d0841bf483ULL }, /* bd_f_9610_bd_m_9610_2 | bd_f_9610_bd_m_9610_2_s | bd_f_9610_bd_m_9610_2_n (6) */
    { 0x45d059ea642e39ffULL, 0xb9c378bb7bbf97c9ULL, 0xb9c373bb7bbf8f4aULL }, /* bd_f_9340_1 | bd_f_9340_1_s | bd_f_9340_1_n (8) */
    { 0x45d05aea642e3bb2ULL, 0xc478a9bb8264d60cULL, 0xc478bebb8264f9bbULL }, /* bd_f_9340_2 | bd_f_9340_2_s | bd_f_9340_2_n (8) */
    { 0x469958d1bf7c859fULL, 0x287f285c210a95a9ULL, 0x287f235c210a8d2aULL }, /* lg_f_9710 | lg_f_9710_s | lg_f_9710_n (18) */
    { 0x47c246be5e5b5d43ULL, 0xcec259deda6a4a85ULL, 0xcec26cdeda6a6aceULL }, /* am_m_9289_am_m_9280 | am_m_9289_am_m_9280_s | am_m_9289_am_m_9280_n (2) */
    { 0x48188ef2121f178bULL, 0x1b8e3a14a8c3ed3dULL, 0x1b8e3d14a8c3f256ULL }, /* hd_f_9340_hd_m_9340 | hd_f_9340_hd_m_9340_s | hd_f_9340_hd_m_9340_n (22) */
    { 0x48495098059888dbULL, 0x97554b803782d30dULL, 0x97552e803782a1c6ULL }, /* am_f_9280_am_m_9280 | am_f_9280_am_m_9280_s | am_f_9280_am_m_9280_n (2) */
    { 0x4a3a3735fe146847ULL, 0x4a75371a99c1ddc1ULL, 0x4a75221a99c1ba12ULL }, /* bd_m_9370_1 | bd_m_9370_1_s | bd_m_9370_1_n (8) */
    { 0x4a3a3835fe1469faULL, 0x529d681a9e3be3c4ULL, 0x529d6d1a9e3bec43ULL }, /* bd_m_9370_2 | bd_m_9370_2_s | bd_m_9370_2_n (4) */
    { 0x4b6f7ea36f194c63ULL, 0xe8808ce2d21db065ULL, 0xe8809fe2d21dd0aeULL }, /* bd_a_9730_1 | bd_a_9730_1_s | bd_a_9730_1_n (2) */
    { 0x4b6f7fa36f194e16ULL, 0xf25cbde2d80afde8ULL, 0xf25caae2d80add9fULL }, /* bd_a_9730_2 | bd_a_9730_2_s | bd_a_9730_2_n (6) */
    { 0x4d68967a15ae126aULL, 0xe2031122f770a2b4ULL, 0xe202f622f77074d3ULL }, /* lg_m_5370 | lg_m_5370_s | lg_m_5370_n (12) */
    { 0x4d79947a15bc8237ULL, 0x0342b74ca7a33af1ULL, 0x0342c24ca7a34da2ULL }, /* lg_m_5320 | lg_m_5320_s | lg_m_5320_n (4) */
    { 0x4daea8c9fe2b6673ULL, 0x1f719c10fce44af5ULL, 0x1f718f10fce434deULL }, /* hd_f_2550_hd_m_2550 | hd_f_2550_hd_m_2550_s | hd_f_2550_hd_m_2550_n (2) */
    { 0x4fb2f9d1c4c3cc93ULL, 0xd4d00899bb8d0755ULL, 0xd4cffb99bb8cf13eULL }, /* lg_f_9620 | lg_f_9620_s | lg_f_9620_n (4) */
    { 0x4fef1f8581501ccbULL, 0x5668fbabf853167dULL, 0x5668feabf8531b96ULL }, /* am_m_9230_am_f_9230 | am_m_9230_am_f_9230_s | am_m_9230_am_f_9230_n (2) */
    { 0x50e7276104f0b390ULL, 0x3ec3eacca19b760aULL, 0x3ec3efcca19b7e89ULL }, /* am_f_9450_l | am_f_9450_l_s | am_f_9450_l_n (3) */
    { 0x513db3312cadc12eULL, 0x9e8f19d9bd3de750ULL, 0x9e8f16d9bd3de237ULL }, /* bd_f_9480 | bd_f_9480_s | bd_f_9480_n (6) */
    { 0x517372f5f80f427bULL, 0x2b1fff78c65fda6dULL, 0x2b1fe278c65fa926ULL }, /* bd_m_5370_1 | bd_m_5370_1_s | bd_m_5370_1_n (8) */
    { 0x517373f5f80f442eULL, 0x35d51078cd04e250ULL, 0x35d50d78cd04dd37ULL }, /* bd_m_5370_2 | bd_m_5370_2_s | bd_m_5370_2_n (8) */
    { 0x54f966c6e41f2147ULL, 0x5a191d6065847ec1ULL, 0x5a19086065845b12ULL }, /* lg_f_9500_lg_m_9500 | lg_f_9500_lg_m_9500_s | lg_f_9500_lg_m_9500_n (12) */
    { 0x553c3a4eb9010083ULL, 0xebe211a986a3eac5ULL, 0xebe224a986a40b0eULL }, /* bd_f_9440_bd_m_9440_1 | bd_f_9440_bd_m_9440_1_s | bd_f_9440_bd_m_9440_1_n (16) */
    { 0x553c3b4eb9010236ULL, 0xf40a42a98b1df0c8ULL, 0xf40a2fa98b1dd07fULL }, /* bd_f_9440_bd_m_9440_2 | bd_f_9440_bd_m_9440_2_s | bd_f_9440_bd_m_9440_2_n (12) */
    { 0x5643edb7f587223fULL, 0x26ed8f3b6adff809ULL, 0x26ed8a3b6adfef8aULL }, /* bd_f_5370_bd_m_5370_1 | bd_f_5370_bd_m_5370_1_s | bd_f_5370_bd_m_5370_1_n (8) */
    { 0x5643eeb7f58723f2ULL, 0x2e3bc03b6ea05a4cULL, 0x2e3bd53b6ea07dfbULL }, /* bd_f_5370_bd_m_5370_2 | bd_f_5370_bd_m_5370_2_s | bd_f_5370_bd_m_5370_2_n (8) */
    { 0x565df4503c43b7bbULL, 0x304049d122af1cadULL, 0x30402cd122aeeb66ULL }, /* bd_m_9320_2 | bd_m_9320_2_s | bd_m_9320_2_n (8) */
    { 0x565df5503c43b96eULL, 0x378f5ad12670fb90ULL, 0x378f57d12670f677ULL }, /* bd_m_9320_1 | bd_m_9320_1_s | bd_m_9320_1_n (10) */
    { 0x56d8585f80330316ULL, 0xccec595ecaa7fae8ULL, 0xccec465ecaa7da9fULL }, /* bd_m_9440_1_l | bd_m_9440_1_l_s | bd_m_9440_1_l_n (7) */
    { 0x57c29b8c697c8bc7ULL, 0x1797c0ff6bea0d41ULL, 0x1797abff6be9e992ULL }, /* am_m_9520_am_f_9520 | am_m_9520_am_f_9520_s | am_m_9520_am_f_9520_n (4) */
    { 0x5a812828a954bc57ULL, 0x4ca05252b95dc251ULL, 0x4ca05d52b95dd502ULL }, /* bd_f_9280_bd_m_9280_1 | bd_f_9280_bd_m_9280_1_s | bd_f_9280_bd_m_9280_1_n (8) */
    { 0x5a812928a954be0aULL, 0x57548352c0014d94ULL, 0x57546852c0011fb3ULL }, /* bd_f_9280_bd_m_9280_2 | bd_f_9280_bd_m_9280_2_s | bd_f_9280_bd_m_9280_2_n (8) */
    { 0x5b025ef6034a6ebfULL, 0x604ed3e664fcb889ULL, 0x604ecee664fcb00aULL }, /* bd_a_9200_1 | bd_a_9200_1_s | bd_a_9200_1_n (4) */
    { 0x5b025ff6034a7072ULL, 0x679d04e668bd1accULL, 0x679d19e668bd3e7bULL }, /* bd_a_9200_2 | bd_a_9200_2_s | bd_a_9200_2_n (2) */
    { 0x5c3e0236087abb38ULL, 0xdf30d021a6a31882ULL, 0xdf30c521a6a305d1ULL }, /* bd_m_9372_2 | bd_m_9372_2_s | bd_m_9372_2_n (4) */
    { 0x5c3e0536087ac051ULL, 0xf7a95321b4110f5bULL, 0xf7a93e21b410ebacULL }, /* bd_m_9372_1 | bd_m_9372_1_s | bd_m_9372_1_n (8) */
    { 0x5cb9f89ff0f55653ULL, 0x74eb0131ceaee915ULL, 0x74eaf431ceaed2feULL }, /* lg_f_2520_lg_m_2520 | lg_f_2520_lg_m_2520_s | lg_f_2520_lg_m_2520_n (10) */
    { 0x5d526d681648a630ULL, 0xc9a488ff4a159e6aULL, 0xc9a48dff4a15a6e9ULL }, /* bd_m_6200_1 | bd_m_6200_1_s | bd_m_6200_1_n (2) */
    { 0x5d5270681648ab49ULL, 0xe21cebff57835ee3ULL, 0xe21ce6ff57835664ULL }, /* bd_m_6200_2 | bd_m_6200_2_s | bd_m_6200_2_n (2) */
    { 0x5da60e6652956fedULL, 0x34c029c89b5cf97fULL, 0x34c03cc89b5d19c8ULL }, /* hd_f_9230 | hd_f_9230_s | hd_f_9230_n (6) */
    { 0x5dc4319cde49e768ULL, 0x876842cef0491032ULL, 0x876857cef04933e1ULL }, /* bd_m_2550 | bd_m_2550_s | bd_m_2550_n (5) */
    { 0x5e4fae58eb8db06bULL, 0xa3de2737bd5707ddULL, 0xa3de2a37bd570cf6ULL }, /* bd_m_body_m_l | bd_m_body_m_l_s | bd_m_body_m_l_n (1) */
    { 0x5e9afb865f00d740ULL, 0x65fab7c4a421443aULL, 0x65fa9cc4a4211659ULL }, /* bd_f_9420_1 | bd_f_9420_1_s | bd_f_9420_1_n (8) */
    { 0x5e9afe865f00dc59ULL, 0x7f4d1ac4b248a873ULL, 0x7f4d35c4b248d654ULL }, /* bd_f_9420_2 | bd_f_9420_2_s | bd_f_9420_2_n (10) */
    { 0x6047304991c61d96ULL, 0x35049e9740b6b968ULL, 0x35048b9740b6991fULL }, /* hd_m_6200 | hd_m_6200_s | hd_m_6200_n (2) */
    { 0x63ca100764ecbc9bULL, 0xd12474868ed4cacdULL, 0xd12457868ed49986ULL }, /* bd_m_9520_bd_f_9520_1 | bd_m_9520_bd_f_9520_1_s | bd_m_9520_bd_f_9520_1_n (12) */
    { 0x63ca110764ecbe4eULL, 0xd87285869294f6b0ULL, 0xd87282869294f197ULL }, /* bd_m_9520_bd_f_9520_2 | bd_m_9520_bd_f_9520_2_s | bd_m_9520_bd_f_9520_2_n (12) */
    { 0x648220b53f2826d4ULL, 0x303ee55804b18a26ULL, 0x303f025804b1bb6dULL }, /* bd_f_9529_bd_f_9521_1 | bd_f_9529_bd_f_9521_1_s | bd_f_9529_bd_f_9521_1_n (16) */
    { 0x648223b53f282bedULL, 0x4a6a68581391157fULL, 0x4a6a7b58139135c8ULL }, /* bd_f_9529_bd_f_9521_2 | bd_f_9529_bd_f_9521_2_s | bd_f_9529_bd_f_9521_2_n (12) */
    { 0x6623900562fc9a13ULL, 0x78d0189c0bde70d5ULL, 0x78d00b9c0bde5abeULL }, /* lg_f_2550_lg_m_2550 | lg_f_2550_lg_m_2550_s | lg_f_2550_lg_m_2550_n (8) */
    { 0x66a49f6657c5d3f9ULL, 0xf5c869c420a79dd3ULL, 0xf5c884c420a7cbb4ULL }, /* hd_f_9380 | hd_f_9380_s | hd_f_9380_n (4) */
    { 0x66bfaf6657dcb6e1ULL, 0xe1110a0635df6b2bULL, 0xe111150635df7ddcULL }, /* hd_f_9300 | hd_f_9300_s | hd_f_9300_n (12) */
    { 0x66c2653c9c8167bbULL, 0x6670b97e9fa04cadULL, 0x66709c7e9fa01b66ULL }, /* lg_m_9710_lg_f_9710 | lg_m_9710_lg_f_9710_s | lg_m_9710_lg_f_9710_n (18) */
    { 0x66c69b6657e2b393ULL, 0x376ece1780550655ULL, 0x376ec1178054f03eULL }, /* hd_f_9320 | hd_f_9320_s | hd_f_9320_n (6) */
    { 0x66ca216657e5cd1cULL, 0xec5b4c207355f8deULL, 0xec5b592073560ef5ULL }, /* hd_f_9350 | hd_f_9350_s | hd_f_9350_n (10) */
    { 0x66d0ab6657eb2348ULL, 0xef745e2fdaf48d52ULL, 0xef74732fdaf4b101ULL }, /* hd_f_9372 | hd_f_9372_s | hd_f_9372_n (8) */
    { 0x66d0ad6657eb26aeULL, 0x0251a82fe613a8d0ULL, 0x0251a52fe613a3b7ULL }, /* hd_f_9370 | hd_f_9370_s | hd_f_9370_n (8) */
    { 0x66d0ae6657eb2861ULL, 0x099fe12fe9d418abULL, 0x099fec2fe9d42b5cULL }, /* hd_f_9371 | hd_f_9371_s | hd_f_9371_n (8) */
    { 0x66d4336657ee4037ULL, 0xb73e2e38d914a8f1ULL, 0xb73e3938d914bba2ULL }, /* hd_f_9360 | hd_f_9360_s | hd_f_9360_n (2) */
    { 0x673a14bdd77f3427ULL, 0x7e7cda7bb7d54c61ULL, 0x7e7cc57bb7d528b2ULL }, /* bd_m_9540_2 | bd_m_9540_2_s | bd_m_9540_2_n (4) */
    { 0x673a15bdd77f35daULL, 0x84f20b7bbaddbde4ULL, 0x84f2107bbaddc663ULL }, /* bd_m_9540_1 | bd_m_9540_1_s | bd_m_9540_1_n (20) */
    { 0x68c872d1d2b7a810ULL, 0x918aa1e2ea461e8aULL, 0x918aa6e2ea462709ULL }, /* lg_f_9320 | lg_f_9320_s | lg_f_9320_n (6) */
    { 0x68cefed1d2bd01a2ULL, 0xa77ffdf25d021b7cULL, 0xa77ff2f25d0208cbULL }, /* lg_f_9300 | lg_f_9300_s | lg_f_9300_n (6) */
    { 0x68d60ad1d2c334b4ULL, 0x1159e20443058ac6ULL, 0x1159ff044305bc0dULL }, /* lg_f_9360 | lg_f_9360_s | lg_f_9360_n (8) */
    { 0x68d90ed1d2c57157ULL, 0x5eac960ab6e9bf51ULL, 0x5eaca10ab6e9d202ULL }, /* lg_f_9372 | lg_f_9372_s | lg_f_9372_n (2) */
    { 0x68d90fd1d2c5730aULL, 0x6960c70abd8d4a94ULL, 0x6960ac0abd8d1cb3ULL }, /* lg_f_9371 | lg_f_9371_s | lg_f_9371_n (2) */
    { 0x68d910d1d2c574bdULL, 0x7189e00ac208dacfULL, 0x7189d30ac208c4b8ULL }, /* lg_f_9370 | lg_f_9370_s | lg_f_9370_n (2) */
    { 0x68dc76d1d2c857e6ULL, 0x106d3e131750c0f8ULL, 0x106d4b131750d70fULL }, /* lg_f_9340 | lg_f_9340_s | lg_f_9340_n (10) */
    { 0x68dc77d1d2c85999ULL, 0x1a4857131d3c32b3ULL, 0x1a4872131d3c6094ULL }, /* lg_f_9341 | lg_f_9341_s | lg_f_9341_n (10) */
    { 0x68dffcd1d2cb716fULL, 0xc55aa41c0a533db9ULL, 0xc55abf1c0a536b9aULL }, /* lg_f_9350 | lg_f_9350_s | lg_f_9350_n (4) */
    { 0x68ea0ed1d2d3e48aULL, 0x91ef9e347181f814ULL, 0x91ef83347181ca33ULL }, /* lg_f_9380 | lg_f_9380_s | lg_f_9380_n (6) */
    { 0x68f12290e72887d7ULL, 0xa1ff577f04bbd9d1ULL, 0xa1ff627f04bbec82ULL }, /* lg_f_9540_lg_m_9540 | lg_f_9540_lg_m_9540_s | lg_f_9540_lg_m_9540_n (4) */
    { 0x698683d4473e125fULL, 0x264559a796e7a569ULL, 0x264554a796e79ceaULL }, /* bd_m_9289_bd_m_9280_1 | bd_m_9289_bd_m_9280_1_s | bd_m_9289_bd_m_9280_1_n (6) */
    { 0x698684d4473e1412ULL, 0x30f98aa79d8b30acULL, 0x30f99fa79d8b545bULL }, /* bd_m_9289_bd_m_9280_2 | bd_m_9289_bd_m_9280_2_s | bd_m_9289_bd_m_9280_2_n (8) */
    { 0x6bc75e00d244d077ULL, 0xa2b1f01e51e81531ULL, 0xa2b1fb1e51e827e2ULL }, /* lg_f_9440_lg_m_9440 | lg_f_9440_lg_m_9440_s | lg_f_9440_lg_m_9440_n (6) */
    { 0x6c4c821a8d67588bULL, 0x6bb40771faf9563dULL, 0x6bb40a71faf95b56ULL }, /* lg_f_2560_lg_m_2560 | lg_f_2560_lg_m_2560_s | lg_f_2560_lg_m_2560_n (8) */
    { 0x6dee124fc91b5c7fULL, 0xd511b35130519e49ULL, 0xd511ae51305195caULL }, /* bd_f_9260_bd_m_9260_1 | bd_f_9260_bd_m_9260_1_s | bd_f_9260_bd_m_9260_1_n (8) */
    { 0x6dee134fc91b5e32ULL, 0xdfc6e45136f6dc8cULL, 0xdfc6f95136f7003bULL }, /* bd_f_9260_bd_m_9260_2 | bd_f_9260_bd_m_9260_2_s | bd_f_9260_bd_m_9260_2_n (8) */
    { 0x6ede9f771f40bd82ULL, 0x6d39e44be574189cULL, 0x6d39d94be57405ebULL }, /* am_f_9620 | am_f_9620_s | am_f_9620_n (8) */
    { 0x6ee71413ce32d6ceULL, 0x9727d668c7886330ULL, 0x9727d368c7885e17ULL }, /* am_a_2570 | am_a_2570_s | am_a_2570_n (2) */
    { 0x6eea1a13ce3516d7ULL, 0xf757dc6f468bc0d1ULL, 0xf757e76f468bd382ULL }, /* am_a_2560 | am_a_2560_s | am_a_2560_n (2) */
    { 0x6f05f27f35135ad3ULL, 0xb8b087965e7b2195ULL, 0xb8b07a965e7b0b7eULL }, /* am_m_9480_am_f_9480 | am_m_9480_am_f_9480_s | am_m_9480_am_f_9480_n (4) */
    { 0x6fa5e965f3823f2fULL, 0x75ea2244fcfc2c79ULL, 0x75ea3d44fcfc5a5aULL }, /* am_f_9720_am_m_9720 | am_f_9720_am_m_9720_s | am_f_9720_am_m_9720_n (4) */
    { 0x6fbada665d0a37c4ULL, 0x025aebf96526d296ULL, 0x025ae8f96526cd7dULL }, /* hd_f_9480 | hd_f_9480_s | hd_f_9480_n (4) */
    { 0x6fcbd8665d18a791ULL, 0x239baa23155b469bULL, 0x239b9523155b22ecULL }, /* hd_f_9450 | hd_f_9450_s | hd_f_9450_n (2) */
    { 0x6fcf5e665d1bc11aULL, 0xd888282c085c3924ULL, 0xd8882d2c085c41a3ULL }, /* hd_f_9460 | hd_f_9460_s | hd_f_9460_n (12) */
    { 0x6fdcd6665d27175eULL, 0x4175484cc2aaa840ULL, 0x4175654cc2aad987ULL }, /* hd_f_9420 | hd_f_9420_s | hd_f_9420_n (8) */
    { 0x707b05468a039a1eULL, 0x6226ebb480dc9400ULL, 0x622708b480dcc547ULL }, /* am_a_8200 | am_a_8200_s | am_a_8200_n (6) */
    { 0x708aeaadbe0765f7ULL, 0x2d2ba462c9a286b1ULL, 0x2d2baf62c9a29962ULL }, /* hd_f_9600_hd_a_9600 | hd_f_9600_hd_a_9600_s | hd_f_9600_hd_a_9600_n (6) */
    { 0x711e60a2571f6ae7ULL, 0x937f9e7fa5199621ULL, 0x937f897fa5197272ULL }, /* lg_f_9400_lg_m_9400 | lg_f_9400_lg_m_9400_s | lg_f_9400_lg_m_9400_n (17) */
    { 0x71e89fd1d8044896ULL, 0x52f7de2ff6cc9c68ULL, 0x52f7cb2ff6cc7c1fULL }, /* lg_f_9230 | lg_f_9230_s | lg_f_9230_n (9) */
    { 0x724f71e11b34409bULL, 0x389099953254eecdULL, 0x38907c953254bd86ULL }, /* bd_f_body_m | bd_f_body_m_s | bd_f_body_m_n (3) */
    { 0x724f71e11b34409bULL, 0x5e4fbb58eb8dc682ULL, 0x5e4fb058eb8db3d1ULL }, /* bd_f_body_m | bd_m_body_m_s | bd_m_body_m_n (1) */
    { 0x733cf42b060e5e3cULL, 0x3e5fd05f72c503beULL, 0x3e5fdd5f72c519d5ULL }, /* bd_m_9440_2 | bd_m_9440_2_s | bd_m_9440_2_n (18) */
    { 0x733cf72b060e6355ULL, 0x56d8535f8032fa97ULL, 0x56d8565f8032ffb0ULL }, /* bd_m_9440_1 | bd_m_9440_1_s | bd_m_9440_1_n (21) */
    { 0x74c55ad5df860c97ULL, 0xf8427380c35d1791ULL, 0xf8427e80c35d2a42ULL }, /* bd_f_9530_2 | bd_f_9530_2_s | bd_f_9530_2_n (8) */
    { 0x74c55bd5df860e4aULL, 0xff91a480c71f2cd4ULL, 0xff918980c71efef3ULL }, /* bd_f_9530_1 | bd_f_9530_1_s | bd_f_9530_1_n (12) */
    { 0x7589af03e449d5b3ULL, 0x3445bea0b45c4a35ULL, 0x3445b1a0b45c341eULL }, /* hd_m_9520_hd_f_9520 | hd_m_9520_hd_f_9520_s | hd_m_9520_hd_f_9520_n (2) */
    { 0x75a15d8af4cd8b37ULL, 0x2fa39eee8e58abf1ULL, 0x2fa3a9ee8e58bea2ULL }, /* am_m_9309_am_m_9300 | am_m_9309_am_m_9300_s | am_m_9309_am_m_9300_n (2) */
    { 0x772ef36660eb407cULL, 0x1b1c082c9fd191feULL, 0x1b1c152c9fd1a815ULL }, /* hd_f_9530 | hd_f_9530_s | hd_f_9530_n (2) */
    { 0x7732786660ee5852ULL, 0xc7e14d358e5a23ecULL, 0xc7e162358e5a479bULL }, /* hd_f_9521 | hd_f_9521_s | hd_f_9521_n (2) */
    { 0x7732796660ee5a05ULL, 0xd295863594fdbcc7ULL, 0xd295693594fd8b80ULL }, /* hd_f_9520 | hd_f_9520_s | hd_f_9520_n (2) */
    { 0x777ed02aecc3b938ULL, 0x5e4fbb58eb8dc682ULL, 0x5e4fb058eb8db3d1ULL }, /* bd_m_body_m | bd_m_body_m_s | bd_m_body_m_n (224) */
    { 0x777ed02aecc3b938ULL, 0x777ede2aecc3d102ULL, 0x777ed32aecc3be51ULL }, /* bd_m_body_m | bd_m_body_s | bd_m_body_n (1) */
    { 0x77ea4877247bd4b2ULL, 0x595304664f7d570cULL, 0x595319664f7d7abbULL }, /* am_f_9710 | am_f_9710_s | am_f_9710_n (2) */
    { 0x79c82f0ee20fa979ULL, 0x99bef2f569cb4f53ULL, 0x99bf0df569cb7d34ULL }, /* am_m_9379_am_m_9372 | am_m_9379_am_m_9372_s | am_m_9379_am_m_9372_n (4) */
    { 0x7a69b8dd6ebd0693ULL, 0x30af4d2266e55155ULL, 0x30af402266e53b3eULL }, /* lg_f_9510_lg_m_9510 | lg_f_9510_lg_m_9510_s | lg_f_9510_lg_m_9510_n (10) */
    { 0x7a9526b2752f264fULL, 0x6bf2cddbdf6d68d9ULL, 0x6bf2e8dbdf6d96baULL }, /* bd_m_9349_bd_m_9340_1 | bd_m_9349_bd_m_9340_1_s | bd_m_9349_bd_m_9340_1_n (10) */
    { 0x7a9527b2752f2802ULL, 0x76a7fedbe612a71cULL, 0x76a7f3dbe612946bULL }, /* bd_m_9349_bd_m_9340_2 | bd_m_9349_bd_m_9340_2_s | bd_m_9349_bd_m_9340_2_n (8) */
    { 0x7aeac84a763d8b0fULL, 0x973a7a4099028699ULL, 0x973a95409902b47aULL }, /* lg_m_9521_lg_f_9521 | lg_m_9521_lg_f_9521_s | lg_m_9521_lg_f_9521_n (8) */
    { 0x7c9fea1866014c5fULL, 0xfea95f5e1499ef69ULL, 0xfea95a5e1499e6eaULL }, /* hd_m_9620_hd_f_9620 | hd_m_9620_hd_f_9620_s | hd_m_9620_hd_f_9620_n (20) */
    { 0x7dade5b2b832dc07ULL, 0xf7c0365a52381581ULL, 0xf7c0215a5237f1d2ULL }, /* hd_f_9430_hd_m_9430 | hd_f_9430_hd_m_9430_s | hd_f_9430_hd_m_9430_n (8) */
    { 0x7db6905ddfb4674fULL, 0x3b3c8ece9967d1d9ULL, 0x3b3ca9ce9967ffbaULL }, /* am_m_9521_am_f_9521 | am_m_9521_am_f_9521_s | am_m_9521_am_f_9521_n (4) */
    { 0x7f3ce5772840d750ULL, 0x860444489f3201caULL, 0x860449489f320a49ULL }, /* am_f_9460 | am_f_9460_s | am_f_9460_n (4) */
    { 0x7f46f77728494a6bULL, 0x50e7266104f0b1ddULL, 0x50e7296104f0b6f6ULL }, /* am_f_9450 | am_f_9450_s | am_f_9450_n (9) */
    { 0x7f4a7d77284c63f4ULL, 0x05d48469f7f32106ULL, 0x05d4a169f7f3524dULL }, /* am_f_9420 | am_f_9420_s | am_f_9420_n (2) */
    { 0x7f6c79772869438eULL, 0x477ae0bd57a07bf0ULL, 0x477addbd57a076d7ULL }, /* am_f_9480 | am_f_9480_s | am_f_9480_n (4) */
    { 0x7fdc146665d68f10ULL, 0xf4296760af0e1d8aULL, 0xf4296c60af0e2609ULL }, /* hd_f_9620 | hd_f_9620_s | hd_f_9620_n (20) */
    { 0x8031551d990f5bf4ULL, 0x22228594d914d906ULL, 0x2222a294d9150a4dULL }, /* bd_m_9340_1 | bd_m_9340_1_s | bd_m_9340_1_n (10) */
    { 0x8031581d990f610dULL, 0x3e010894e965f8dfULL, 0x3e011b94e9661928ULL }, /* bd_m_9340_2 | bd_m_9340_2_s | bd_m_9340_2_n (8) */
    { 0x810ec6def8cca077ULL, 0xbbbb04ec8b186531ULL, 0xbbbb0fec8b1877e2ULL }, /* lg_f_9470_lg_m_9470 | lg_f_9470_lg_m_9470_s | lg_f_9470_lg_m_9470_n (6) */
    { 0x83518d9889f13c47ULL, 0x460886a1781bd1c1ULL, 0x460871a1781bae12ULL }, /* hd_f_2540_hd_m_2540 | hd_f_2540_hd_m_2540_s | hd_f_2540_hd_m_2540_n (4) */
    { 0x83ef99eede2c7e6bULL, 0xffa0bd9c057005ddULL, 0xffa0c09c05700af6ULL }, /* bd_f_9230_1 | bd_f_9230_1_s | bd_f_9230_1_n (20) */
    { 0x83ef9aeede2c801eULL, 0x097bce9c0b5b6a00ULL, 0x097beb9c0b5b9b47ULL }, /* bd_f_9230_2 | bd_f_9230_2_s | bd_f_9230_2_n (10) */
    { 0x8583f5a2a2bd8e20ULL, 0x827a5dd542b455daULL, 0x827a42d542b427f9ULL }, /* hd_a_5320 | hd_a_5320_s | hd_a_5320_n (4) */
    { 0x859513a2a2cc344dULL, 0xb5841bff8d04d31fULL, 0xb5842eff8d04f368ULL }, /* hd_a_5370 | hd_a_5370_s | hd_a_5370_n (4) */
    { 0x859b7fa2a2d1577fULL, 0xb570600e6207d149ULL, 0xb5705b0e6207c8caULL }, /* hd_a_5350 | hd_a_5350_s | hd_a_5350_n (4) */
    { 0x85d561c9a40789e4ULL, 0x068730cd07cbc676ULL, 0x06872dcd07cbc15dULL }, /* bd_a_2870_2 | bd_a_2870_2_s | bd_a_2870_2_n (4) */
    { 0x85d564c9a4078efdULL, 0x1fd9b3cd15f3610fULL, 0x1fd9a6cd15f34af8ULL }, /* bd_a_2870_1 | bd_a_2870_1_s | bd_a_2870_1_n (6) */
    { 0x886aaa772d99047aULL, 0xc740e0b704782244ULL, 0xc740e5b704782ac3ULL }, /* am_f_9530 | am_f_9530_s | am_f_9530_n (6) */
    { 0x886e0f772d9be5f0ULL, 0x5ed60dbf55ffa62aULL, 0x5ed612bf55ffaea9ULL }, /* am_f_9521 | am_f_9521_s | am_f_9521_n (4) */
    { 0x886e10772d9be7a3ULL, 0x698b26bf5ca4bba5ULL, 0x698b39bf5ca4dbeeULL }, /* am_f_9520 | am_f_9520_s | am_f_9520_n (4) */
    { 0x886e58df3aa8e613ULL, 0x62df1d15f5da9cd5ULL, 0x62df1015f5da86beULL }, /* bd_a_9509_bd_a_9500_1 | bd_a_9509_bd_a_9500_1_s | bd_a_9509_bd_a_9500_1_n (12) */
    { 0x886e59df3aa8e7c6ULL, 0x6a2d4e15f99aff18ULL, 0x6a2d5b15f99b152fULL }, /* bd_a_9509_bd_a_9500_2 | bd_a_9509_bd_a_9500_2_s | bd_a_9509_bd_a_9500_2_n (6) */
    { 0x8879b7e787b1c74bULL, 0x63ed5bcaed26e4fdULL, 0x63ed5ecaed26ea16ULL }, /* bd_m_9510_1 | bd_m_9510_1_s | bd_m_9510_1_n (10) */
    { 0x8879b8e787b1c8feULL, 0x6c166ccaf1a267a0ULL, 0x6c1689caf1a298e7ULL }, /* bd_m_9510_2 | bd_m_9510_2_s | bd_m_9510_2_n (10) */
    { 0x88f555666b1d32e4ULL, 0x5f38c79c711ed776ULL, 0x5f38c49c711ed25dULL }, /* hd_f_9710 | hd_f_9710_s | hd_f_9710_n (10) */
    { 0x8a0c6e1d9efacdafULL, 0x55ba97ac76a67ef9ULL, 0x55bab2ac76a6acdaULL }, /* bd_m_9341_1 | bd_m_9341_1_s | bd_m_9341_1_n (10) */
    { 0x8a0c6f1d9efacf62ULL, 0x5d09c8ac7a68943cULL, 0x5d09bdac7a68818bULL }, /* bd_m_9341_2 | bd_m_9341_2_s | bd_m_9341_2_n (8) */
    { 0x8d02b76fec2e11acULL, 0xfdf6fab5d130d2aeULL, 0xfdf6e7b5d130b265ULL }, /* hd_a_3322 | hd_a_3322_s | hd_a_3322_n (4) */
    { 0x8d02b96fec2e1512ULL, 0x0f2144b5dade59acULL, 0x0f2159b5dade7d5bULL }, /* hd_a_3320 | hd_a_3320_s | hd_a_3320_n (2) */
    { 0x8d02ba6fec2e16c5ULL, 0x16707db5dea07c87ULL, 0x167060b5dea04b40ULL }, /* hd_a_3321 | hd_a_3321_s | hd_a_3321_n (2) */
    { 0x8e6a17d299fe378cULL, 0x037a464384ddc9ceULL, 0x037a334384dda985ULL }, /* bd_f_9370_1 | bd_f_9370_1_s | bd_f_9370_1_n (6) */
    { 0x8e6a1ad299fe3ca5ULL, 0x1da5c94393bd5527ULL, 0x1da5ac4393bd23e0ULL }, /* bd_f_9370_2 | bd_f_9370_2_s | bd_f_9370_2_n (8) */
    { 0x9187d17732e364f7ULL, 0x287e26fd91fb5db1ULL, 0x287e31fd91fb7062ULL }, /* am_f_9230 | am_f_9230_s | am_f_9230_n (2) */
    { 0x9262d3d015865e9fULL, 0x61460d76560956a9ULL, 0x6146087656094e2aULL }, /* hd_m_9521_hd_f_9521 | hd_m_9521_hd_f_9521_s | hd_m_9521_hd_f_9521_n (2) */
    { 0x954d342d0b1293bfULL, 0xe986d52d66d9a589ULL, 0xe986d02d66d99d0aULL }, /* bd_m_9360_2 | bd_m_9360_2_s | bd_m_9360_2_n (4) */
    { 0x954d352d0b129572ULL, 0xf0d5062d6a9a07ccULL, 0xf0d51b2d6a9a2b7bULL }, /* bd_m_9360_1 | bd_m_9360_1_s | bd_m_9360_1_n (4) */
    { 0x95d31e61b35415ffULL, 0x3101595172e3d3c9ULL, 0x3101545172e3cb4aULL }, /* bd_m_9620_bd_f_9620_1 | bd_m_9620_bd_f_9620_1_s | bd_m_9620_bd_f_9620_1_n (8) */
    { 0x95d31f61b35417b2ULL, 0x3bb68a517989120cULL, 0x3bb69f51798935bbULL }, /* bd_m_9620_bd_f_9620_2 | bd_m_9620_bd_f_9620_2_s | bd_m_9620_bd_f_9620_2_n (6) */
    { 0x961b6043913f919fULL, 0x1a97ea1cc4eb81a9ULL, 0x1a97e51cc4eb792aULL }, /* hd_m_9480_hd_f_9480 | hd_m_9480_hd_f_9480_s | hd_m_9480_hd_f_9480_n (4) */
    { 0x9695bc8ba713a8b0ULL, 0x2d5ee0ab82d084eaULL, 0x2d5ee5ab82d08d69ULL }, /* bd_a_9700_1 | bd_a_9700_1_s | bd_a_9700_1_n (6) */
    { 0x9695bf8ba713adc9ULL, 0x45d743ab903e4563ULL, 0x45d73eab903e3ce4ULL }, /* bd_a_9700_2 | bd_a_9700_2_s | bd_a_9700_2_n (4) */
    { 0x983b0471227566f0ULL, 0xba3cd7976c384f2aULL, 0xba3cdc976c3857a9ULL }, /* lg_m_2540 | lg_m_2540_s | lg_m_2540_n (6) */
    { 0x983e8a7122788079ULL, 0x6f2a35a05f3abe53ULL, 0x6f2a50a05f3aec34ULL }, /* lg_m_2550 | lg_m_2550_s | lg_m_2550_n (8) */
    { 0x98421071227b9a02ULL, 0x26a3b3a95466e91cULL, 0x26a3a8a95466d66bULL }, /* lg_m_2560 | lg_m_2560_s | lg_m_2560_n (8) */
    { 0x98451671227dda0bULL, 0x86d2b9afd36893bdULL, 0x86d2bcafd36898d6ULL }, /* lg_m_2570 | lg_m_2570_s | lg_m_2570_n (10) */
    { 0x9845d0d29feab927ULL, 0x9f4bf85e31fc9961ULL, 0x9f4be35e31fc75b2ULL }, /* bd_f_9371_1 | bd_f_9371_1_s | bd_f_9371_1_n (6) */
    { 0x9845d1d29feabadaULL, 0xa5c1295e35050ae4ULL, 0xa5c12e5e35051363ULL }, /* bd_f_9371_2 | bd_f_9371_2_s | bd_f_9371_2_n (8) */
    { 0x984f88712286f046ULL, 0x8f8ff3ca0eb3db98ULL, 0x8f9000ca0eb3f1afULL }, /* lg_m_2520 | lg_m_2520_s | lg_m_2520_n (10) */
    { 0x993b4c50cd8084f3ULL, 0x7aacf52ae891ad75ULL, 0x7aace82ae891975eULL }, /* lg_m_9289_lg_m_9280 | lg_m_9289_lg_m_9280_s | lg_m_9289_lg_m_9280_n (6) */
    { 0x99463dc71a3909a9ULL, 0x28701895dae47703ULL, 0x28701395dae46e84ULL }, /* hd_m_9590 | hd_m_9590_s | hd_m_9590_n (2) */
    { 0x99614dc71a4fec91ULL, 0x12dfb8d7ef64539bULL, 0x12dfa3d7ef642fecULL }, /* hd_m_9510 | hd_m_9510_s | hd_m_9510_n (2) */
    { 0x996839c71a55e943ULL, 0x66b07ce937aeb685ULL, 0x66b08fe937aed6ceULL }, /* hd_m_9530 | hd_m_9530_s | hd_m_9530_n (2) */
    { 0x996b3fc71a58294cULL, 0xc6df7aefb6b0538eULL, 0xc6df67efb6b03345ULL }, /* hd_m_9540 | hd_m_9540_s | hd_m_9540_n (4) */
    { 0x9a786a773807993fULL, 0x2bdba6d5e8f80709ULL, 0x2bdba1d5e8f7fe8aULL }, /* am_f_9380 | am_f_9380_s | am_f_9380_n (6) */
    { 0x9a78f43faeeda737ULL, 0x8d5e772b0e5127f1ULL, 0x8d5e822b0e513aa2ULL }, /* lg_f_9220_lg_m_9220 | lg_f_9220_lg_m_9220_s | lg_f_9220_lg_m_9220_n (8) */
    { 0x9a7bf077380ab2c8ULL, 0xdfef24dedb4108d2ULL, 0xdfef39dedb412c81ULL }, /* am_f_9370 | am_f_9370_s | am_f_9370_n (2) */
    { 0x9a7bf177380ab47bULL, 0xe8175ddedfbb1c6dULL, 0xe81740dedfbaeb26ULL }, /* am_f_9371 | am_f_9371_s | am_f_9371_n (2) */
    { 0x9a7bf277380ab62eULL, 0xf2cc6edee6602450ULL, 0xf2cc6bdee6601f37ULL }, /* am_f_9372 | am_f_9372_s | am_f_9372_n (2) */
    { 0x9a7f7677380dcc51ULL, 0x94dba2e7ce41fb5bULL, 0x94db8de7ce41d7acULL }, /* am_f_9360 | am_f_9360_s | am_f_9360_n (4) */
    { 0x9a82fc773810e5daULL, 0x49c820f0c142ede4ULL, 0x49c825f0c142f663ULL }, /* am_f_9350 | am_f_9350_s | am_f_9350_n (4) */
    { 0x9a8661773813c750ULL, 0xe15d4df912ca71caULL, 0xe15d52f912ca7a49ULL }, /* am_f_9341 | am_f_9341_s | am_f_9341_n (4) */
    { 0x9a8662773813c903ULL, 0xec1266f9196f8745ULL, 0xec1279f9196fa78eULL }, /* am_f_9340 | am_f_9340_s | am_f_9340_n (4) */
    { 0x9a8a4a23afa558e7ULL, 0xdfac66c17096b421ULL, 0xdfac51c170969072ULL }, /* lg_f_1000_lg_m_1000 | lg_f_1000_lg_m_1000_s | lg_f_1000_lg_m_1000_n (6) */
    { 0x9a8cee7738192295ULL, 0x012ee3088b73c9d7ULL, 0x012ee6088b73cef0ULL }, /* am_f_9320 | am_f_9320_s | am_f_9320_n (2) */
    { 0x9a93fa77381f55a7ULL, 0x6be1c71a722f29e1ULL, 0x6be1b21a722f0632ULL }, /* am_f_9300 | am_f_9300_s | am_f_9300_n (2) */
    { 0x9b60df64935b24cfULL, 0x45a48f7bb424ab59ULL, 0x45a4aa7bb424d93aULL }, /* lg_f_9700_lg_m_9700 | lg_f_9700_lg_m_9700_s | lg_f_9700_lg_m_9700_n (6) */
    { 0x9b83b7ca76c4ad77ULL, 0x881b6746c5597a31ULL, 0x881b7246c5598ce2ULL }, /* bd_a_9500_2 | bd_a_9500_2_s | bd_a_9500_2_n (6) */
    { 0x9b83b8ca76c4af2aULL, 0x92d09846cbfeb874ULL, 0x92d07d46cbfe8a93ULL }, /* bd_a_9500_1 | bd_a_9500_1_s | bd_a_9500_1_n (12) */
    { 0x9f9464d2a3abc3a3ULL, 0xa9daaf350d42f7a5ULL, 0xa9dac2350d4317eeULL }, /* bd_f_9372_2 | bd_f_9372_2_s | bd_f_9372_2_n (8) */
    { 0x9f9465d2a3abc556ULL, 0xb04fe035104b6928ULL, 0xb04fcd35104b48dfULL }, /* bd_f_9372_1 | bd_f_9372_1_s | bd_f_9372_1_n (6) */
    { 0x9fd774a7187dad67ULL, 0xa9ec8d75b729bca1ULL, 0xa9ec7875b72998f2ULL }, /* lg_f_2870_lg_m_2870 | lg_f_2870_lg_m_2870_s | lg_f_2870_lg_m_2870_n (4) */
    { 0xa093c7e5f18478d3ULL, 0x836347051632ef95ULL, 0x83633a051632d97eULL }, /* lg_f_9210_lg_m_9210 | lg_f_9210_lg_m_9210_s | lg_f_9210_lg_m_9210_n (8) */
    { 0xa0b6f0c71e172f38ULL, 0xa173d6c0bf8dac82ULL, 0xa173cbc0bf8d99d1ULL }, /* hd_m_9430 | hd_m_9430_s | hd_m_9430_n (8) */
    { 0xa0ba76c71e1a48c1ULL, 0x558654c9b1d4fb4bULL, 0x55865fc9b1d50dfcULL }, /* hd_m_9420 | hd_m_9420_s | hd_m_9420_n (4) */
    { 0xa0bd7cc71e1c88caULL, 0xb5b652d030d84b54ULL, 0xb5b637d030d81d73ULL }, /* hd_m_9410 | hd_m_9410_s | hd_m_9410_n (4) */
    { 0xa0c0e2c71e1f6bf3ULL, 0x557398d886d9ac75ULL, 0x55738bd886d9965eULL }, /* hd_m_9400 | hd_m_9400_s | hd_m_9400_n (2) */
    { 0xa0c7eec71e259f05ULL, 0xc1d994ea6f06c9c7ULL, 0xc1d977ea6f069880ULL }, /* hd_m_9460 | hd_m_9460_s | hd_m_9460_n (4) */
    { 0xa0cb74c71e28b88eULL, 0x76c6f2f3620938f0ULL, 0x76c6eff3620933d7ULL }, /* hd_m_9450 | hd_m_9450_s | hd_m_9450_n (4) */
    { 0xa0ce7ac71e2af897ULL, 0xd6f5f8f9e10ae391ULL, 0xd6f603f9e10af642ULL }, /* hd_m_9440 | hd_m_9440_s | hd_m_9440_n (8) */
    { 0xa16c5fbeda623ed3ULL, 0x934ec0fa9449a595ULL, 0x934eb3fa94498f7eULL }, /* lg_m_9509_lg_m_9500 | lg_m_9509_lg_m_9500_s | lg_m_9509_lg_m_9500_n (14) */
    { 0xa38696e20c027f37ULL, 0x331835df222fbff1ULL, 0x331840df222fd2a2ULL }, /* bd_f_9350_2 | bd_f_9350_2_s | bd_f_9350_2_n (6) */
    { 0xa38697e20c0280eaULL, 0x3a6766df25f1d534ULL, 0x3a674bdf25f1a753ULL }, /* bd_f_9350_1 | bd_f_9350_1_s | bd_f_9350_1_n (8) */
    { 0xa3b81f44877a45e7ULL, 0x43af580fa9d7a921ULL, 0x43af430fa9d78572ULL }, /* am_f_9270_am_m_9270 | am_f_9270_am_m_9270_s | am_f_9270_am_m_9270_n (8) */
    { 0xa44177e4b210d904ULL, 0x8e40d4a72824d3d6ULL, 0x8e40d1a72824cebdULL }, /* bd_a_9570_2 | bd_a_9570_2_s | bd_a_9570_2_n (10) */
    { 0xa4417ae4b210de1dULL, 0xa79357a7364c6e6fULL, 0xa7934aa7364c5858ULL }, /* bd_a_9570_1 | bd_a_9570_1_s | bd_a_9570_1_n (8) */
    { 0xa513a04cf04141f9ULL, 0xdcfd38dedaf63bd3ULL, 0xdcfd53dedaf669b4ULL }, /* am_a_9390 | am_a_9390_s | am_a_9390_n (6) */
    { 0xa52eb04cf05824e1ULL, 0xc845d920f02e092bULL, 0xc845e420f02e1bdcULL }, /* am_a_9310 | am_a_9310_s | am_a_9310_n (4) */
    { 0xa5643037356df9dcULL, 0x722e9b1405e2a89eULL, 0x722ea81405e2beb5ULL }, /* bd_m_9220_2 | bd_m_9220_2_s | bd_m_9220_2_n (4) */
    { 0xa5643337356dfef5ULL, 0x8c5a1e1414c233f7ULL, 0x8c5a211414c23910ULL }, /* bd_m_9220_1 | bd_m_9220_1_s | bd_m_9220_1_n (4) */
    { 0xa5cc62ed437043b7ULL, 0x161544ace101b871ULL, 0x16154face101cb22ULL }, /* lg_f_9570_lg_m_9570 | lg_f_9570_lg_m_9570_s | lg_f_9570_lg_m_9570_n (16) */
    { 0xa6bd8ea3c6142a97ULL, 0x70822006f0b9e591ULL, 0x70822b06f0b9f842ULL }, /* am_f_9260_am_m_9260 | am_f_9260_am_m_9260_s | am_f_9260_am_m_9260_n (4) */
    { 0xa9341412a109554fULL, 0x265d70d9bb0befd9ULL, 0x265d8bd9bb0c1dbaULL }, /* bd_m_9430_2 | bd_m_9430_2_s | bd_m_9430_2_n (6) */
    { 0xa9341512a1095702ULL, 0x3112a1d9c1b12e1cULL, 0x311296d9c1b11b6bULL }, /* bd_m_9430_1 | bd_m_9430_1_s | bd_m_9430_1_n (4) */
    { 0xa95babb7c7f857b3ULL, 0xbb2e81b0ec951c35ULL, 0xbb2e74b0ec95061eULL }, /* lg_f_9610_lg_m_9610 | lg_f_9610_lg_m_9610_s | lg_f_9610_lg_m_9610_n (10) */
    { 0xa9ae1d7d6bff4197ULL, 0x45914ad826c59491ULL, 0x459155d826c5a742ULL }, /* bd_f_9450_1 | bd_f_9450_1_s | bd_f_9450_1_n (8) */
    { 0xa9ae1e7d6bff434aULL, 0x4ce07bd82a87a9d4ULL, 0x4ce060d82a877bf3ULL }, /* bd_f_9450_2 | bd_f_9450_2_s | bd_f_9450_2_n (8) */
    { 0xa9c23e57f9ecae0fULL, 0x4277658d42182199ULL, 0x4277808d42184f7aULL }, /* am_m_9349_am_m_9340 | am_m_9349_am_m_9340_s | am_m_9349_am_m_9340_n (4) */
    { 0xa9c9a5c72358797aULL, 0xf68cf2ed0ee0df44ULL, 0xf68cf7ed0ee0e7c3ULL }, /* hd_m_9740 | hd_m_9740_s | hd_m_9740_n (2) */
    { 0xa9d091c7235e762cULL, 0x4dc3b6fe5a0e6b2eULL, 0x4dc3a3fe5a0e4ae5ULL }, /* hd_m_9720 | hd_m_9720_s | hd_m_9720_n (4) */
    { 0xaac48db0d4bb712bULL, 0xc210b26b2a87949dULL, 0xc210b56b2a8799b6ULL }, /* lg_f_9250_lg_m_9250 | lg_f_9250_lg_m_9250_s | lg_f_9250_lg_m_9250_n (12) */
    { 0xaafceb0117c8726bULL, 0xb10255d470c619ddULL, 0xb10258d470c61ef6ULL }, /* bd_f_5320_bd_m_5320_1 | bd_f_5320_bd_m_5320_1_s | bd_f_5320_bd_m_5320_1_n (8) */
    { 0xaafcec0117c8741eULL, 0xbadd66d476b17e00ULL, 0xbadd83d476b1af47ULL }, /* bd_f_5320_bd_m_5320_2 | bd_f_5320_bd_m_5320_2_s | bd_f_5320_bd_m_5320_2_n (4) */
    { 0xac00cacd0380dadfULL, 0xbc1dbef2866341e9ULL, 0xbc1db9f28663396aULL }, /* am_m_9710_am_f_9710 | am_m_9710_am_f_9710_s | am_m_9710_am_f_9710_n (2) */
    { 0xaca4051d12230cdbULL, 0x3d90480314bdf70dULL, 0x3d902b0314bdc5c6ULL }, /* hd_f_2520_hd_m_2520 | hd_f_2520_hd_m_2520_s | hd_f_2520_hd_m_2520_n (6) */
    { 0xae22cf4cf57f72b2ULL, 0x808fd7023a2ba50cULL, 0x808fec023a2bc8bbULL }, /* am_a_9290 | am_a_9290_s | am_a_9290_n (2) */
    { 0xae3ad94cf5941591ULL, 0x0ad0793dcfa9e49bULL, 0x0ad0643dcfa9c0ecULL }, /* am_a_9220 | am_a_9220_s | am_a_9220_n (4) */
    { 0xae3e5f4cf5972f1aULL, 0xbfbcf746c2aad724ULL, 0xbfbcfc46c2aadfa3ULL }, /* am_a_9210 | am_a_9210_s | am_a_9210_n (6) */
    { 0xae41c54cf59a1243ULL, 0x5ea13d4f17f44785ULL, 0x5ea1504f17f467ceULL }, /* am_a_9200 | am_a_9200_s | am_a_9200_n (2) */
    { 0xae4bd74cf5a2855eULL, 0x28aa17677cf94640ULL, 0x28aa34677cf97787ULL }, /* am_a_9250 | am_a_9250_s | am_a_9250_n (6) */
    { 0xae4f5d4cf5a59ee7ULL, 0xde709d7070b3ea21ULL, 0xde70887070b3c672ULL }, /* am_a_9240 | am_a_9240_s | am_a_9240_n (4) */
    { 0xaefc8d9acc322556ULL, 0x7d150c3f74f0c928ULL, 0x7d14f93f74f0a8dfULL }, /* lg_m_9280 | lg_m_9280_s | lg_m_9280_n (6) */
    { 0xaf00139acc353edfULL, 0x3202724867f345e9ULL, 0x32026d4867f33d6aULL }, /* lg_m_9290 | lg_m_9290_s | lg_m_9290_n (6) */
    { 0xaf03999acc385868ULL, 0xe7c7f0515bac2932ULL, 0xe7c805515bac4ce1ULL }, /* lg_m_9260 | lg_m_9260_s | lg_m_9260_n (8) */
    { 0xaf069f9acc3a9871ULL, 0x47f7ee57daaf793bULL, 0x47f7d957daaf558cULL }, /* lg_m_9270 | lg_m_9270_s | lg_m_9270_n (4) */
    { 0xaf0a259acc3db1faULL, 0xfce46c60cdb06bc4ULL, 0xfce47160cdb07443ULL }, /* lg_m_9240 | lg_m_9240_s | lg_m_9240_n (8) */
    { 0xaf0d8b9acc409523ULL, 0x9f2eb26925dd0525ULL, 0x9f2ec56925dd256eULL }, /* lg_m_9250 | lg_m_9250_s | lg_m_9250_n (12) */
    { 0xaf11119acc43aeacULL, 0x541b307218ddf7aeULL, 0x541b1d7218ddd765ULL }, /* lg_m_9220 | lg_m_9220_s | lg_m_9220_n (8) */
    { 0xaf179d9acc49083eULL, 0x69378c818ae203e0ULL, 0x6937a9818ae23527ULL }, /* lg_m_9200 | lg_m_9200_s | lg_m_9200_n (6) */
    { 0xaf1b239acc4c21c7ULL, 0x1d4b128a7d2b1341ULL, 0x1d4afd8a7d2aef92ULL }, /* lg_m_9210 | lg_m_9210_s | lg_m_9210_n (8) */
    { 0xafaa56fc4a31d3c4ULL, 0x3082ab95b7d2ce96ULL, 0x3082a895b7d2c97dULL }, /* bd_f_9320_2 | bd_f_9320_2_s | bd_f_9320_2_n (12) */
    { 0xafaa59fc4a31d8ddULL, 0x49d52e95c5fa692fULL, 0x49d52195c5fa5318ULL }, /* bd_f_9320_1 | bd_f_9320_1_s | bd_f_9320_1_n (12) */
    { 0xb071cd90214bf5fbULL, 0xeb6c87339b4319edULL, 0xeb6c6a339b42e8a6ULL }, /* hd_a_9509_hd_a_9500 | hd_a_9509_hd_a_9500_s | hd_a_9509_hd_a_9500_n (2) */
    { 0xb0fd870815ca3207ULL, 0x5c7e7a322cb6db81ULL, 0x5c7e65322cb6b7d2ULL }, /* hd_m_9710_hd_f_9710 | hd_m_9710_hd_f_9710_s | hd_m_9710_hd_f_9710_n (10) */
    { 0xb249c766f5ae25e7ULL, 0x5c9ea9b30fc68921ULL, 0x5c9e94b30fc66572ULL }, /* hd_f_9440_hd_m_9440 | hd_f_9440_hd_m_9440_s | hd_f_9440_hd_m_9440_n (8) */
    { 0xb27d32c72848eb40ULL, 0xcead962ff268783aULL, 0xcead7b2ff2684a59ULL }, /* hd_m_9610 | hd_m_9610_s | hd_m_9610_n (4) */
    { 0xb2aab3a4e9f436dbULL, 0x53305fc5717bb10dULL, 0x533042c5717b7fc6ULL }, /* hd_f_9510_hd_m_9510 | hd_f_9510_hd_m_9510_s | hd_f_9510_hd_m_9510_n (2) */
    { 0xb31ea758adcafbc0ULL, 0x001c55e4c9de9cbaULL, 0x001c3ae4c9de6ed9ULL }, /* lg_m_1000 | lg_m_1000_s | lg_m_1000_n (6) */
    { 0xb5b54d6ad2c850d1ULL, 0x3a474794029eb3dbULL, 0x3a473294029e902cULL }, /* hd_m_2520 | hd_m_2520_s | hd_m_2520_n (6) */
    { 0xb5c64b6ad2d6c09eULL, 0x5b86e5bdb2d13e80ULL, 0x5b8702bdb2d16fc7ULL }, /* hd_m_2550 | hd_m_2550_s | hd_m_2550_n (2) */
    { 0xb5c9d16ad2d9da27ULL, 0x114d6bc6a68be261ULL, 0x114d56c6a68bbeb2ULL }, /* hd_m_2540 | hd_m_2540_s | hd_m_2540_n (4) */
    { 0xb7bdb29ad12e23c0ULL, 0xd5050fa3b13804baULL, 0xd504f4a3b137d6d9ULL }, /* lg_m_9310 | lg_m_9310_s | lg_m_9310_n (4) */
    { 0xb7c1389ad1313d49ULL, 0x89f16daca438c0e3ULL, 0x89f168aca438b864ULL }, /* lg_m_9300 | lg_m_9300_s | lg_m_9300_n (6) */
    { 0xb7c8449ad137705bULL, 0xf3cb71be8a3c668dULL, 0xf3cb54be8a3c3546ULL }, /* lg_m_9320 | lg_m_9320_s | lg_m_9320_n (4) */
    { 0xb7cb4a9ad139b064ULL, 0x54d44fc509f770f6ULL, 0x54d44cc509f76bddULL }, /* lg_m_9350 | lg_m_9350_s | lg_m_9350_n (4) */
    { 0xb7cecf9ad13cc83aULL, 0x0272b4cdf9382a04ULL, 0x0272b9cdf9383283ULL }, /* lg_m_9341 | lg_m_9341_s | lg_m_9341_n (10) */
    { 0xb7ced09ad13cc9edULL, 0x09c0cdcdfcf8637fULL, 0x09c0e0cdfcf883c8ULL }, /* lg_m_9340 | lg_m_9340_s | lg_m_9340_n (10) */
    { 0xb7d2349ad13fa9b0ULL, 0x9a07e1d64abfadeaULL, 0x9a07e6d64abfb669ULL }, /* lg_m_9372 | lg_m_9372_s | lg_m_9372_n (2) */
    { 0xb7d2369ad13fad16ULL, 0xab322bd6546d34e8ULL, 0xab3218d6546d149fULL }, /* lg_m_9370 | lg_m_9370_s | lg_m_9370_n (2) */
    { 0xb7d2379ad13faec9ULL, 0xb28044d6582d6e63ULL, 0xb2803fd6582d65e4ULL }, /* lg_m_9371 | lg_m_9371_s | lg_m_9371_n (2) */
    { 0xb7d5bc9ad142c69fULL, 0x601e91df476dfea9ULL, 0x601e8cdf476df62aULL }, /* lg_m_9360 | lg_m_9360_s | lg_m_9360_n (10) */
    { 0xb7d9429ad145e028ULL, 0x15e50fe83b2894f2ULL, 0x15e524e83b28b8a1ULL }, /* lg_m_9390 | lg_m_9390_s | lg_m_9390_n (8) */
    { 0xb7dc489ad1482031ULL, 0x76140deeba2a31fbULL, 0x7613f8eeba2a0e4cULL }, /* lg_m_9380 | lg_m_9380_s | lg_m_9380_n (6) */
    { 0xb948233b185b3f62ULL, 0x032ab6c64e2a843cULL, 0x032aabc64e2a718bULL }, /* am_m_9372_l | am_m_9372_l_s | am_m_9372_l_n (2) */
    { 0xbab181e9c9f207e7ULL, 0xd6af3367040bbb21ULL, 0xd6af1e67040b9772ULL }, /* lg_f_9430_lg_m_9430 | lg_f_9430_lg_m_9430_s | lg_f_9430_lg_m_9430_n (6) */
    { 0xbabb38ff9be42fcfULL, 0xd72972f3acd86e59ULL, 0xd7298df3acd89c3aULL }, /* lg_m_9480_lg_f_9480 | lg_m_9480_lg_f_9480_s | lg_m_9480_lg_f_9480_n (20) */
    { 0xbbea5af4263f4fdbULL, 0xc551e682e2adb20dULL, 0xc551c982e2ad80c6ULL }, /* bd_a_9550_1 | bd_a_9550_1_s | bd_a_9550_1_n (12) */
    { 0xbbea5bf4263f518eULL, 0xd006f782e952b9f0ULL, 0xd006f482e952b4d7ULL }, /* bd_a_9550_2 | bd_a_9550_2_s | bd_a_9550_2_n (6) */
    { 0xbc04a67254c8a378ULL, 0x63b12e0ec31b9cc2ULL, 0x63b1230ec31b8a11ULL }, /* lg_f_9529_lg_f_9521 | lg_f_9529_lg_f_9521_s | lg_f_9529_lg_f_9521_n (8) */
    { 0xbc1e9bedae01dcf7ULL, 0xd0e899603e4495b1ULL, 0xd0e8a4603e44a862ULL }, /* bd_a_9210_2 | bd_a_9210_2_s | bd_a_9210_2_n (4) */
    { 0xbc1e9cedae01deaaULL, 0xdb9dca6044e9d3f4ULL, 0xdb9daf6044e9a613ULL }, /* bd_a_9210_1 | bd_a_9210_1_s | bd_a_9210_1_n (4) */
    { 0xbc34fbfdc55e147bULL, 0xd05c776330357c6dULL, 0xd05c5a6330354b26ULL }, /* am_f_9430_am_m_9430 | am_f_9430_am_m_9430_s | am_f_9430_am_m_9430_n (2) */
    { 0xbceba30811c8639bULL, 0x7c69f2a0511789cdULL, 0x7c69d5a051175886ULL }, /* bd_m_2520_1 | bd_m_2520_1_s | bd_m_2520_1_n (11) */
    { 0xbceba40811c8654eULL, 0x83b803a054d7b5b0ULL, 0x83b800a054d7b097ULL }, /* bd_m_2520_2 | bd_m_2520_2_s | bd_m_2520_2_n (12) */
    { 0xbd68967f6e0519c3ULL, 0x953a700157befb05ULL, 0x953a830157bf1b4eULL }, /* hd_a_1000 | hd_a_1000_s | hd_a_1000_n (2) */
    { 0xbe4ff822130c9599ULL, 0x19178d72615dceb3ULL, 0x1917a872615dfc94ULL }, /* bd_m_9450_2 | bd_m_9450_2_s | bd_m_9450_2_n (6) */
    { 0xc07d3e3c05753d1fULL, 0xccddae5e4f967929ULL, 0xccdda95e4f9670aaULL }, /* bd_m_9309_bd_m_9300_1 | bd_m_9309_bd_m_9300_1_s | bd_m_9309_bd_m_9300_1_n (6) */
    { 0xc07d3f3c05753ed2ULL, 0xd42cdf5e53588e6cULL, 0xd42cf45e5358b21bULL }, /* bd_m_9309_bd_m_9300_2 | bd_m_9309_bd_m_9300_2_s | bd_m_9309_bd_m_9300_2_n (10) */
    { 0xc0c9db9ad66a1470ULL, 0x15dbafc08f4098aaULL, 0x15dbb4c08f40a129ULL }, /* lg_m_9440 | lg_m_9440_s | lg_m_9440_n (6) */
    { 0xc0cd619ad66d2df9ULL, 0xcac90dc9824307d3ULL, 0xcac928c9824335b4ULL }, /* lg_m_9450 | lg_m_9450_s | lg_m_9450_n (8) */
    { 0xc0d0e79ad6704782ULL, 0x82428bd2776f329cULL, 0x824280d2776f1febULL }, /* lg_m_9460 | lg_m_9460_s | lg_m_9460_n (8) */
    { 0xc0d3ed9ad672878bULL, 0xe27191d8f670dd3dULL, 0xe27194d8f670e256ULL }, /* lg_m_9470 | lg_m_9470_s | lg_m_9470_n (6) */
    { 0xc0d7739ad675a114ULL, 0x975defe1e9719966ULL, 0x975e0ce1e971caadULL }, /* lg_m_9400 | lg_m_9400_s | lg_m_9400_n (18) */
    { 0xc0daf99ad678ba9dULL, 0x4c4b6deadc743eefULL, 0x4c4b60eadc7428d8ULL }, /* lg_m_9410 | lg_m_9410_s | lg_m_9410_n (2) */
    { 0xc0de5f9ad67b9dc6ULL, 0xeb2ecbf331bc2518ULL, 0xeb2ed8f331bc3b2fULL }, /* lg_m_9420 | lg_m_9420_s | lg_m_9420_n (6) */
    { 0xc0e1e59ad67eb74fULL, 0xa01c31fc24bea1d9ULL, 0xa01c4cfc24becfbaULL }, /* lg_m_9430 | lg_m_9430_s | lg_m_9430_n (6) */
    { 0xc0f5e99ad68f6725ULL, 0x2264ee2c54aca3a7ULL, 0x2264d12c54ac7260ULL }, /* lg_m_9490 | lg_m_9490_s | lg_m_9490_n (8) */
    { 0xc20d14e20a49ae3bULL, 0x3e0866e65837172dULL, 0x3e0849e65836e5e6ULL }, /* lg_f_9720_lg_m_9720 | lg_f_9720_lg_m_9720_s | lg_f_9720_lg_m_9720_n (7) */
    { 0xc3ed3a0bbb7d219bULL, 0x704eb51b3f83f7cdULL, 0x704e981b3f83c686ULL }, /* bd_f_9300_1 | bd_f_9300_1_s | bd_f_9300_1_n (8) */
    { 0xc3ed3b0bbb7d234eULL, 0x779cc61b434423b0ULL, 0x779cc31b43441e97ULL }, /* bd_f_9300_2 | bd_f_9300_2_s | bd_f_9300_2_n (6) */
    { 0xc564f10f5dab3be4ULL, 0xee0769db4dd94876ULL, 0xee0766db4dd9435dULL }, /* bd_m_9270_1 | bd_m_9270_1_s | bd_m_9270_1_n (10) */
    { 0xc564f40f5dab40fdULL, 0x0759ecdb5c00e30fULL, 0x0759dfdb5c00ccf8ULL }, /* bd_m_9270_2 | bd_m_9270_2_s | bd_m_9270_2_n (2) */
    { 0xc5c0d538f58f532bULL, 0x69021dfc11dcc69dULL, 0x690220fc11dccbb6ULL }, /* lg_m_9349_lg_m_9340 | lg_m_9349_lg_m_9340_s | lg_m_9349_lg_m_9340_n (10) */
    { 0xc6b900f89a72d7c3ULL, 0x2a2b48aa40cc6905ULL, 0x2a2b5baa40cc894eULL }, /* hd_f_9740_hd_m_9740 | hd_f_9740_hd_m_9740_s | hd_f_9740_hd_m_9740_n (2) */
    { 0xc6f75a3d3031f6efULL, 0xb37688ecf31f1f39ULL, 0xb376a3ecf31f4d1aULL }, /* bd_f_9620_2 | bd_f_9620_2_s | bd_f_9620_2_n (6) */
    { 0xc6f75b3d3031f8a2ULL, 0xbe2ab9ecf9c2aa7cULL, 0xbe2aaeecf9c297cbULL }, /* bd_f_9620_1 | bd_f_9620_1_s | bd_f_9620_1_n (8) */
    { 0xc764564d0398a0e4ULL, 0x466d96b72b6d7576ULL, 0x466d93b72b6d705dULL }, /* am_a_9700 | am_a_9700_s | am_a_9700_n (4) */
    { 0xc76ec84d03a1b71fULL, 0x51b7d8d168e40329ULL, 0x51b7d3d168e3faaaULL }, /* am_a_9730 | am_a_9730_s | am_a_9730_n (2) */
    { 0xc7a82714f101c647ULL, 0x69f53912b81febc1ULL, 0x69f52412b81fc812ULL }, /* am_f_9510_am_m_9510 | am_f_9510_am_m_9510_s | am_f_9510_am_m_9510_n (6) */
    { 0xc7fa0f99b16f311bULL, 0xc382b5d16606f34dULL, 0xc38298d16606c206ULL }, /* lg_f_9630_lg_m_9630 | lg_f_9630_lg_m_9630_s | lg_f_9630_lg_m_9630_n (6) */
    { 0xc9d6049adba60520ULL, 0x593f4fdd6f7464daULL, 0x593f34dd6f7436f9ULL }, /* lg_m_9570 | lg_m_9570_s | lg_m_9570_n (16) */
    { 0xc9d98a9adba91ea9ULL, 0x0e2cade66276d403ULL, 0x0e2ca8e66276cb84ULL }, /* lg_m_9560 | lg_m_9560_s | lg_m_9560_n (10) */
    { 0xc9dc909adbab5eb2ULL, 0x6e5babece178710cULL, 0x6e5bc0ece17894bbULL }, /* lg_m_9550 | lg_m_9550_s | lg_m_9550_n (10) */
    { 0xc9e0169adbae783bULL, 0x234831f5d479712dULL, 0x234814f5d4793fe6ULL }, /* lg_m_9540 | lg_m_9540_s | lg_m_9540_n (6) */
    { 0xc9e39c9adbb191c4ULL, 0xd75b8ffec6c23c96ULL, 0xd75b8cfec6c2377dULL }, /* lg_m_9530 | lg_m_9530_s | lg_m_9530_n (8) */
    { 0xc9ea889adbb78e76ULL, 0x2c066c100fc66c08ULL, 0x2c0659100fc64bbfULL }, /* lg_m_9510 | lg_m_9510_s | lg_m_9510_n (10) */
    { 0xc9ed8e9adbb9ce7fULL, 0x8c3552168ec7e049ULL, 0x8c354d168ec7d7caULL }, /* lg_m_9500 | lg_m_9500_s | lg_m_9500_n (14) */
    { 0xca05989adbce715eULL, 0x1675ec5224461240ULL, 0x1676095224464387ULL }, /* lg_m_9590 | lg_m_9590_s | lg_m_9590_n (4) */
    { 0xca70de700f3312fbULL, 0xcf8aa6d2ddfdbeedULL, 0xcf8a89d2ddfd8da6ULL }, /* hd_a_3460 | hd_a_3460_s | hd_a_3460_n (10) */
    { 0xca73b73c513be873ULL, 0x0d80f228f1cf1cf5ULL, 0x0d80e528f1cf06deULL }, /* bd_m_9460_1 | bd_m_9460_1_s | bd_m_9460_1_n (16) */
    { 0xca73b83c513bea26ULL, 0x18362328f8745b38ULL, 0x18363028f874714fULL }, /* bd_m_9460_2 | bd_m_9460_2_s | bd_m_9460_2_n (6) */
    { 0xcbf539c736908262ULL, 0xaaabce6ae51b3f3cULL, 0xaaabc36ae51b2c8bULL }, /* hd_m_9380 | hd_m_9380_s | hd_m_9380_n (4) */
    { 0xcc09bdc736a20bb8ULL, 0x80d8f29d88507d02ULL, 0x80d8e79d88506a51ULL }, /* hd_m_9320 | hd_m_9320_s | hd_m_9320_n (6) */
    { 0xcc1049c736a7654aULL, 0x951b6eacf99b1bd4ULL, 0x951b53acf99aedf3ULL }, /* hd_m_9300 | hd_m_9300_s | hd_m_9300_n (10) */
    { 0xcc1ab9c736b0781fULL, 0x8f3b46c72d63ec29ULL, 0x8f3b41c72d63e3aaULL }, /* hd_m_9372 | hd_m_9372_s | hd_m_9372_n (4) */
    { 0xcc1abac736b079d2ULL, 0x968a77c73126016cULL, 0x968a8cc73126251bULL }, /* hd_m_9371 | hd_m_9371_s | hd_m_9371_n (4) */
    { 0xcc1abbc736b07b85ULL, 0xa13eb0c737c99a47ULL, 0xa13e93c737c96900ULL }, /* hd_m_9370 | hd_m_9370_s | hd_m_9370_n (4) */
    { 0xcc1e41c736b3950eULL, 0x562c0ed02acc0970ULL, 0x562c0bd02acc0457ULL }, /* hd_m_9340 | hd_m_9340_s | hd_m_9340_n (14) */
    { 0xcc1e42c736b396c1ULL, 0x5d7a47d02e8c794bULL, 0x5d7a52d02e8c8bfcULL }, /* hd_m_9341 | hd_m_9341_s | hd_m_9341_n (14) */
    { 0xcc2147c736b5d517ULL, 0xb65b14d6a9cdb411ULL, 0xb65b1fd6a9cdc6c2ULL }, /* hd_m_9350 | hd_m_9350_s | hd_m_9350_n (12) */
    { 0xce955114048166f5ULL, 0xe1dc50364415dbf7ULL, 0xe1dc53364415e110ULL }, /* am_a_2870 | am_a_2870_s | am_a_2870_n (2) */
    { 0xcf248799ae81063cULL, 0x1bfedd5b6819ebbeULL, 0x1bfeea5b681a01d5ULL }, /* hd_a_4150 | hd_a_4150_s | hd_a_4150_n (4) */
    { 0xcf2e9999ae897957ULL, 0xe52ebf73cc670751ULL, 0xe52eca73cc671a02ULL }, /* hd_a_4160 | hd_a_4160_s | hd_a_4160_n (8) */
    { 0xcf6e2339c3593fb8ULL, 0x5e4fbb58eb8dc682ULL, 0x777ed32aecc3be51ULL }, /* bd_m_body | bd_m_body_m_s | bd_m_body_n (1) */
    { 0xcf6e2339c3593fb8ULL, 0x724f5fe11b342205ULL, 0x724f72e11b34424eULL }, /* bd_m_body | bd_f_body_s | bd_f_body_n (15) */
    { 0xcffbfbab98106bdfULL, 0x84311972c0f47ae9ULL, 0x84311472c0f4726aULL }, /* bd_a_9290_2 | bd_a_9290_2_s | bd_a_9290_2_n (2) */
    { 0xcffbfcab98106d92ULL, 0x8ee54a72c798062cULL, 0x8ee55f72c79829dbULL }, /* bd_a_9290_1 | bd_a_9290_1_s | bd_a_9290_1_n (6) */
    { 0xd0707f4d08d49194ULL, 0x88f736d40ae79de6ULL, 0x88f753d40ae7cf2dULL }, /* am_a_9630 | am_a_9630_s | am_a_9630_n (2) */
    { 0xd14dc39adf8a5dc1ULL, 0x3b42ea1a3967584bULL, 0x3b42f51a39676afcULL }, /* lg_m_9630 | lg_m_9630_s | lg_m_9630_n (6) */
    { 0xd1542f9adf8f80f3ULL, 0x3b302e290e6c0975ULL, 0x3b3021290e6bf35eULL }, /* lg_m_9610 | lg_m_9610_s | lg_m_9610_n (10) */
    { 0xd25a6f29f798635fULL, 0x9fd466758d319e69ULL, 0x9fd461758d3195eaULL }, /* hd_m_9289_hd_m_9280 | hd_m_9289_hd_m_9280_s | hd_m_9289_hd_m_9280_n (6) */
    { 0xd29e02376425e887ULL, 0xb102c3f8793a9601ULL, 0xb102aef8793a7252ULL }, /* lg_f_9490_lg_m_9490 | lg_f_9490_lg_m_9490_s | lg_f_9490_lg_m_9490_n (8) */
    { 0xd2eaf44f41f1727aULL, 0x90cb770d8c2bc044ULL, 0x90cb7c0d8c2bc8c3ULL }, /* am_m_5320 | am_m_5320_s | am_m_5320_n (4) */
    { 0xd2fbf24f41ffe247ULL, 0xb1321d373ba667c1ULL, 0xb13208373ba64412ULL }, /* am_m_5370 | am_m_5370_s | am_m_5370_n (2) */
    { 0xd4ee9b0aeabbca11ULL, 0x154cdb5bb1134d1bULL, 0x154cc65bb113296cULL }, /* am_a_1000 | am_a_1000_s | am_a_1000_n (4) */
    { 0xd50b74c73bd4e62dULL, 0xb81850a02a5417bfULL, 0xb81863a02a543808ULL }, /* hd_m_9280 | hd_m_9280_s | hd_m_9280_n (6) */
    { 0xd51c72c73be355faULL, 0xd5f20ec9d7a3afc4ULL, 0xd5f213c9d7a3b843ULL }, /* hd_m_9270 | hd_m_9270_s | hd_m_9270_n (2) */
    { 0xd51fd8c73be63923ULL, 0x783c54d22fd04925ULL, 0x783c67d22fd0696eULL }, /* hd_m_9260 | hd_m_9260_s | hd_m_9260_n (8) */
    { 0xd5b2eb64418ce267ULL, 0x7ff49f48309839a1ULL, 0x7ff48a48309815f2ULL }, /* lg_m_9230_lg_f_9230 | lg_m_9230_lg_f_9230_s | lg_m_9230_lg_f_9230_n (10) */
    { 0xd627f3f4a8cc5b8fULL, 0x21a5c874f5c36b19ULL, 0x21a5e374f5c398faULL }, /* hd_f_9270_hd_m_9270 | hd_f_9270_hd_m_9270_s | hd_f_9270_hd_m_9270_n (2) */
    { 0xd6a102f9fb68de57ULL, 0x8df09dc8fcbd3451ULL, 0x8df0a8c8fcbd4702ULL }, /* am_f_5320_am_m_5320 | am_f_5320_am_m_5320_s | am_f_5320_am_m_5320_n (2) */
    { 0xd6f415806ac0d0a8ULL, 0x4d50a0bbb41a9972ULL, 0x4d50b5bbb41abd21ULL }, /* bd_m_9380_1 | bd_m_9380_1_s | bd_m_9380_1_n (8) */
    { 0xd6f418806ac0d5c1ULL, 0x65c923bbc188904bULL, 0x65c92ebbc188a2fcULL }, /* bd_m_9380_2 | bd_m_9380_2_s | bd_m_9380_2_n (10) */
    { 0xd8635319798fb323ULL, 0xbcc253fe4b22d325ULL, 0xbcc266fe4b22f36eULL }, /* bd_f_9720_bd_m_9720_1 | bd_f_9720_bd_m_9720_1_s | bd_f_9720_bd_m_9720_1_n (6) */
    { 0xd8635419798fb4d6ULL, 0xc33784fe4e2b44a8ULL, 0xc33771fe4e2b245fULL }, /* bd_f_9720_bd_m_9720_2 | bd_f_9720_bd_m_9720_2_s | bd_f_9720_bd_m_9720_2_n (8) */
    { 0xd8a419c9a6447ac3ULL, 0x2167b7434ee88405ULL, 0x2167ca434ee8a44eULL }, /* bd_f_9380_1 | bd_f_9380_1_s | bd_f_9380_1_n (6) */
    { 0xd8a41ac9a6447c76ULL, 0x298fe84353628a08ULL, 0x298fd543536269bfULL }, /* bd_f_9380_2 | bd_f_9380_2_s | bd_f_9380_2_n (4) */
    { 0xd96acb900cd5d71bULL, 0xd54cfa1f5aa9894dULL, 0xd54cdd1f5aa95806ULL }, /* hd_f_9341_hd_m_9341 | hd_f_9341_hd_m_9341_s | hd_f_9341_hd_m_9341_n (12) */
    { 0xd9912c4d0e220b9aULL, 0x9f2213238b6da7a4ULL, 0x9f2218238b6db023ULL }, /* am_a_9500 | am_a_9500_s | am_a_9500_n (8) */
    { 0xd997984d0e272eccULL, 0x9e3557325fb8b50eULL, 0x9e3544325fb894c5ULL }, /* am_a_9560 | am_a_9560_s | am_a_9560_n (4) */
    { 0xd99b1e4d0e2a4855ULL, 0x5321d53b52b9a797ULL, 0x5321d83b52b9acb0ULL }, /* am_a_9570 | am_a_9570_s | am_a_9570_n (8) */
    { 0xd9a22a4d0e307b67ULL, 0xbdd5b94d3976baa1ULL, 0xbdd5a44d397696f2ULL }, /* am_a_9550 | am_a_9550_s | am_a_9550_n (8) */
    { 0xda16d87d6a3ecda0ULL, 0x50ab40c950b6015aULL, 0x50ab25c950b5d379ULL }, /* bd_a_9630_1 | bd_a_9630_1_s | bd_a_9630_1_n (2) */
    { 0xda16db7d6a3ed2b9ULL, 0x69fda3c95edd6593ULL, 0x69fdbec95edd9374ULL }, /* bd_a_9630_2 | bd_a_9630_2_s | bd_a_9630_2_n (2) */
    { 0xda596c9ae4c574f1ULL, 0x275d0a34a37249bbULL, 0x275cf534a372260cULL }, /* lg_m_9740 | lg_m_9740_s | lg_m_9740_n (8) */
    { 0xda67649ae4d1a4b5ULL, 0xe86dca57d4a36db7ULL, 0xe86dcd57d4a372d0ULL }, /* lg_m_9700 | lg_m_9700_s | lg_m_9700_n (6) */
    { 0xda6a6a9ae4d3e4beULL, 0x489ca85e53a4d460ULL, 0x489cc55e53a505a7ULL }, /* lg_m_9730 | lg_m_9730_s | lg_m_9730_n (12) */
    { 0xda6df09ae4d6fe47ULL, 0xfcb02e6745ede3c1ULL, 0xfcb0196745edc012ULL }, /* lg_m_9720 | lg_m_9720_s | lg_m_9720_n (8) */
    { 0xdabfd391db70b923ULL, 0xded269d28c7ec925ULL, 0xded27cd28c7ee96eULL }, /* lg_f_9290_lg_m_9290 | lg_f_9290_lg_m_9290_s | lg_f_9290_lg_m_9290_n (6) */
    { 0xdb115b85c7bdf940ULL, 0xcd9a63066391b63aULL, 0xcd9a480663918859ULL }, /* bd_a_9390_2 | bd_a_9390_2_s | bd_a_9390_2_n (6) */
    { 0xdb115e85c7bdfe59ULL, 0xe6ecc60671b91a73ULL, 0xe6ece10671b94854ULL }, /* bd_a_9390_1 | bd_a_9390_1_s | bd_a_9390_1_n (6) */
    { 0xdb8157e7541550a7ULL, 0xf9a25ec600255ce1ULL, 0xf9a249c600253932ULL }, /* bd_a_9470_2 | bd_a_9470_2_s | bd_a_9470_2_n (8) */
    { 0xdb8158e75415525aULL, 0x00178fc6032dce64ULL, 0x001794c6032dd6e3ULL }, /* bd_a_9470_1 | bd_a_9470_1_s | bd_a_9470_1_n (6) */
    { 0xdc14f680f1dea601ULL, 0x6c5537dc76ea188bULL, 0x6c5542dc76ea2b3cULL }, /* bd_a_underwear | bd_a_underwear_s | bd_a_underwear_n (40) */
    { 0xdc7cfa17944ad947ULL, 0x73bda1dad0acf6c1ULL, 0x73bd8cdad0acd312ULL }, /* hd_f_9540_hd_m_9540 | hd_f_9540_hd_m_9540_s | hd_f_9540_hd_m_9540_n (4) */
    { 0xdc853f175d7c7f5bULL, 0x0e1f189c5b98cd8dULL, 0x0e1efb9c5b989c46ULL }, /* bd_a_9240_1 | bd_a_9240_1_s | bd_a_9240_1_n (6) */
    { 0xdc8540175d7c810eULL, 0x18d4299c623dd570ULL, 0x18d4269c623dd057ULL }, /* bd_a_9240_2 | bd_a_9240_2_s | bd_a_9240_2_n (2) */
    { 0xdcd0edef5f5e2c73ULL, 0x866d96a31f9800f5ULL, 0x866d89a31f97eadeULL }, /* hd_f_9280_hd_m_9280 | hd_f_9280_hd_m_9280_s | hd_f_9280_hd_m_9280_n (6) */
    { 0xdd18bb6504cfd684ULL, 0x22171311be83ed56ULL, 0x22171011be83e83dULL }, /* bd_f_9460_1 | bd_f_9460_1_s | bd_f_9460_1_n (12) */
    { 0xdd18be6504cfdb9dULL, 0x3b699611ccab87efULL, 0x3b698911ccab71d8ULL }, /* bd_f_9460_2 | bd_f_9460_2_s | bd_f_9460_2_n (10) */
    { 0xde195cc354113eb3ULL, 0x64ae31f63f671b35ULL, 0x64ae24f63f67051eULL }, /* hd_a_9101 | hd_a_9101_s | hd_a_9101_n (2) */
    { 0xde2067c354177012ULL, 0xc65ef50820eeecacULL, 0xc65f0a0820ef105bULL }, /* hd_a_9120 | hd_a_9120_s | hd_a_9120_n (2) */
    { 0xde2068c3541771c5ULL, 0xcdae2e0824b10f87ULL, 0xcdae110824b0de40ULL }, /* hd_a_9121 | hd_a_9121_s | hd_a_9121_n (2) */
    { 0xde23edc3541a899bULL, 0x7b4c7b1113f19fcdULL, 0x7b4c5e1113f16e86ULL }, /* hd_a_9130 | hd_a_9130_s | hd_a_9130_n (4) */
    { 0xdf1bddc5a4372580ULL, 0x975b98b27f27477aULL, 0x975b7db27f271999ULL }, /* am_f_9529_am_f_9521 | am_f_9529_am_f_9521_s | am_f_9529_am_f_9521_n (4) */
    { 0xdfb93e8aa05fa267ULL, 0x957fb427c098f9a1ULL, 0x957f9f27c098d5f2ULL }, /* bd_f_9270_bd_m_9270_1 | bd_f_9270_bd_m_9270_1_s | bd_f_9270_bd_m_9270_1_n (10) */
    { 0xdfb93f8aa05fa41aULL, 0x9f5ae527c6849424ULL, 0x9f5aea27c6849ca3ULL }, /* bd_f_9270_bd_m_9270_2 | bd_f_9270_bd_m_9270_2_s | bd_f_9270_bd_m_9270_2_n (4) */
    { 0xdfd1a25f97ec8fe3ULL, 0xf0c56fdf37cdffe5ULL, 0xf0c582df37ce202eULL }, /* am_m_9620_am_f_9620 | am_m_9620_am_f_9620_s | am_m_9620_am_f_9620_n (8) */
    { 0xe030541caa12c21bULL, 0x2abfc28b63cc2c4dULL, 0x2abfa58b63cbfb06ULL }, /* bd_f_9510_bd_m_9510_1 | bd_f_9510_bd_m_9510_1_s | bd_f_9510_bd_m_9510_1_n (10) */
    { 0xe030551caa12c3ceULL, 0x320dd38b678c5830ULL, 0x320dd08b678c5317ULL }, /* bd_f_9510_bd_m_9510_2 | bd_f_9510_bd_m_9510_2_s | bd_f_9510_bd_m_9510_2_n (10) */
    { 0xe0d5d14d11dade74ULL, 0x0c7672e2ab643f86ULL, 0x0c768fe2ab6470cdULL }, /* am_a_9490 | am_a_9490_s | am_a_9490_n (4) */
    { 0xe0e9d54d11eb8e4aULL, 0x8d0c2f12d9e0acd4ULL, 0x8d0c1412d9e07ef3ULL }, /* am_a_9470 | am_a_9470_s | am_a_9470_n (2) */
    { 0xe3a4938a923837abULL, 0x996c3049b486df1dULL, 0x996c3349b486e436ULL }, /* bd_m_9280_1 | bd_m_9280_1_s | bd_m_9280_1_n (6) */
    { 0xe3a4948a9238395eULL, 0x9fe14149b78f1a40ULL, 0x9fe15e49b78f4b87ULL }, /* bd_m_9280_2 | bd_m_9280_2_s | bd_m_9280_2_n (8) */
    { 0xe4ac6851fb18e073ULL, 0xda96d7fa061ad4f5ULL, 0xda96cafa061abedeULL }, /* lg_m_9620_lg_f_9620 | lg_m_9620_lg_f_9620_s | lg_m_9620_lg_f_9620_n (4) */
    { 0xe5da0ced77f9d97bULL, 0x5b2e1e5e74d7096dULL, 0x5b2e015e74d6d826ULL }, /* bd_f_2550_bd_m_2550 | bd_f_2550_bd_m_2550_s | bd_f_2550_bd_m_2550_n (6) */
    { 0xe65ad75b3b8dc757ULL, 0xf29e5d86cf418551ULL, 0xf29e6886cf419802ULL }, /* bd_f_9740_bd_m_9740_1 | bd_f_9740_bd_m_9740_1_s | bd_f_9740_bd_m_9740_1_n (12) */
    { 0xe65ad85b3b8dc90aULL, 0xfd528e86d5e51094ULL, 0xfd527386d5e4e2b3ULL }, /* bd_f_9740_bd_m_9740_2 | bd_f_9740_bd_m_9740_2_s | bd_f_9740_bd_m_9740_2_n (8) */
    { 0xe66554841c252387ULL, 0xa1046967b86c0901ULL, 0xa1045467b86be552ULL }, /* bd_m_9720_2 | bd_m_9720_2_s | bd_m_9720_2_n (8) */
    { 0xe66555841c25253aULL, 0xa92c9a67bce60f04ULL, 0xa92c9f67bce61783ULL }, /* bd_m_9720_1 | bd_m_9720_1_s | bd_m_9720_1_n (6) */
    { 0xe78f050e212497f0ULL, 0x4cfc23eff0ba282aULL, 0x4cfc28eff0ba30a9ULL }, /* hd_f_9529_hd_f_9521 | hd_f_9529_hd_f_9521_s | hd_f_9529_hd_f_9521_n (2) */
    { 0xe92ecaafe44b865fULL, 0x14fca93193eb3969ULL, 0x14fca43193eb30eaULL }, /* hd_m_9349_hd_m_9340 | hd_m_9349_hd_m_9340_s | hd_m_9349_hd_m_9340_n (14) */
    { 0xeb92e77308960c4bULL, 0xb9b3ae3584e4f1fdULL, 0xb9b3b13584e4f716ULL }, /* lg_f_9270_lg_m_9270 | lg_f_9270_lg_m_9270_s | lg_f_9270_lg_m_9270_n (6) */
    { 0xec51fc12ba6a0641ULL, 0xd494f21feaa2d4cbULL, 0xd494fd1feaa2e77cULL }, /* hd_m_9379_hd_m_9372 | hd_m_9379_hd_m_9372_s | hd_m_9379_hd_m_9372_n (4) */
    { 0xee9996d91900afc8ULL, 0xe9c956f1fbfd8dd2ULL, 0xe9c96bf1fbfdb181ULL }, /* bd_f_9360_2 | bd_f_9360_2_s | bd_f_9360_2_n (8) */
    { 0xee9999d91900b4e1ULL, 0x03f4d9f20add192bULL, 0x03f4e4f20add2bdcULL }, /* bd_f_9360_1 | bd_f_9360_1_s | bd_f_9360_1_n (4) */
    { 0xef0e08ecb5031a25ULL, 0x58fd4770f2ed4ea7ULL, 0x58fd2a70f2ed1d60ULL }, /* bd_a_9600_fur | bd_a_9600_fur_s | bd_a_9600_fur_n (2) */
    { 0xef5478dbbf0f0b48ULL, 0x4d1a2eba066cb552ULL, 0x4d1a43ba066cd901ULL }, /* bd_a_9560_1 | bd_a_9560_1_s | bd_a_9560_1_n (10) */
    { 0xef547bdbbf0f1061ULL, 0x6745b1ba154c40abULL, 0x6745bcba154c535cULL }, /* bd_a_9560_2 | bd_a_9560_2_s | bd_a_9560_2_n (2) */
    { 0xf04937c35e9ce7bfULL, 0x13b65b69187d1989ULL, 0x13b65669187d110aULL }, /* hd_a_9310 | hd_a_9310_s | hd_a_9310_n (10) */
    { 0xf064c7c35eb4a427ULL, 0x53bc7bada1b43c61ULL, 0x53bc66ada1b418b2ULL }, /* hd_a_9390 | hd_a_9390_s | hd_a_9390_n (4) */
    { 0xf139182bfed7a0eaULL, 0x3f136ab44573f534ULL, 0x3f134fb44573c753ULL }, /* am_m_9720 | am_m_9720_s | am_m_9720_n (4) */
    { 0xf140042bfedd9d9cULL, 0x95702ec58fe7dd5eULL, 0x95703bc58fe7f375ULL }, /* am_m_9740 | am_m_9740_s | am_m_9740_n (2) */
    { 0xf1f3afcbba7eaf07ULL, 0xbc65c5fc9275e081ULL, 0xbc65b0fc9275bcd2ULL }, /* bd_f_9430_bd_m_9430_1 | bd_f_9430_bd_m_9430_1_s | bd_f_9430_bd_m_9430_1_n (6) */
    { 0xf1f3b0cbba7eb0baULL, 0xc48df6fc96efe684ULL, 0xc48dfbfc96efef03ULL }, /* bd_f_9430_bd_m_9430_2 | bd_f_9430_bd_m_9430_2_s | bd_f_9430_bd_m_9430_2_n (8) */
    { 0xf2acaf349a8e2eefULL, 0x8e175c95318c1739ULL, 0x8e177795318c451aULL }, /* bd_a_9600 | bd_a_9600_s | bd_a_9600_n (2) */
    { 0xf2fb3b2f5bc514cbULL, 0xea04bb7f9cf6ce7dULL, 0xea04be7f9cf6d396ULL }, /* am_f_9440_am_m_9440 | am_f_9440_am_m_9440_s | am_f_9440_am_m_9440_n (4) */
    { 0xf30bd64ba4716c93ULL, 0x903f28d0c33ba755ULL, 0x903f1bd0c33b913eULL }, /* bd_f_2560_bd_m_2560_1 | bd_f_2560_bd_m_2560_1_s | bd_f_2560_bd_m_2560_1_n (6) */
    { 0xf30bd74ba4716e46ULL, 0x978d59d0c6fc0998ULL, 0x978d66d0c6fc1fafULL }, /* bd_f_2560_bd_m_2560_2 | bd_f_2560_bd_m_2560_2_s | bd_f_2560_bd_m_2560_2_n (2) */
    { 0xf3340d384a73f990ULL, 0x587527f0b6b3ac0aULL, 0x58752cf0b6b3b489ULL }, /* bd_m_9379_bd_m_9372_2 | bd_m_9379_bd_m_9372_2_s | bd_m_9379_bd_m_9372_2_n (4) */
    { 0xf33410384a73fea9ULL, 0x72a18af0c594b403ULL, 0x72a185f0c594ab84ULL }, /* bd_m_9379_bd_m_9372_1 | bd_m_9379_bd_m_9372_1_s | bd_m_9379_bd_m_9372_1_n (8) */
    { 0xf5209409aec05020ULL, 0xdf1f3402252b67daULL, 0xdf1f1902252b39f9ULL }, /* bd_m_9400_2 | bd_m_9400_2_s | bd_m_9400_2_n (8) */
    { 0xf5209709aec05539ULL, 0xf87197023352cc13ULL, 0xf871b2023352f9f4ULL }, /* bd_m_9400_1 | bd_m_9400_1_s | bd_m_9400_1_n (4) */
    { 0xf72058f27c6c5131ULL, 0x11adbf029c690afbULL, 0x11adaa029c68e74cULL }, /* bd_a_1000 | bd_a_1000_s | bd_a_1000_n (8) */
    { 0xf78ad6c362537a90ULL, 0x228df521bae2550aULL, 0x228dfa21bae25d89ULL }, /* hd_a_9290 | hd_a_9290_s | hd_a_9290_n (2) */
    { 0xf7a5e6c3626a5d78ULL, 0x0b4a7563cdf066c2ULL, 0x0b4a6a63cdf05411ULL }, /* hd_a_9210 | hd_a_9210_s | hd_a_9210_n (6) */
    { 0xf7a96cc3626d7701ULL, 0xc2c3f36cc31c918bULL, 0xc2c3fe6cc31ca43cULL }, /* hd_a_9200 | hd_a_9200_s | hd_a_9200_n (2) */
    { 0xf7afd8c362729a33ULL, 0xc2b1377b982142b5ULL, 0xc2b12a7b98212c9eULL }, /* hd_a_9220 | hd_a_9220_s | hd_a_9220_n (2) */
    { 0xf7b35ec36275b3bcULL, 0x779db5848b22353eULL, 0x779dc2848b224b55ULL }, /* hd_a_9250 | hd_a_9250_s | hd_a_9250_n (8) */
    { 0xf7b6e4c36278cd45ULL, 0x2bb1338d7d6b3707ULL, 0x2bb1168d7d6b05c0ULL }, /* hd_a_9240 | hd_a_9240_s | hd_a_9240_n (4) */
    { 0xf9edb191d913acadULL, 0x0c67f0ff9065623fULL, 0x0c6803ff90658288ULL }, /* lg_m_6200 | lg_m_6200_s | lg_m_6200_n (4) */
    { 0xfa230eceec3d100dULL, 0xc8d32e181be3ffdfULL, 0xc8d341181be42028ULL }, /* hd_m_0001_m | hd_m_0001_m_s | hd_m_0001_m_n (2) */
    { 0xfa354e2355b684e7ULL, 0x3a2307169b5ac021ULL, 0x3a22f2169b5a9c72ULL }, /* lg_f_9280_lg_m_9280 | lg_f_9280_lg_m_9280_s | lg_f_9280_lg_m_9280_n (6) */
    { 0xfa45412c0413919aULL, 0x819d0ad124ee1da4ULL, 0x819d0fd124ee2623ULL }, /* am_m_9610 | am_m_9610_s | am_m_9610_n (2) */
    { 0xfd796866f3c7a5f3ULL, 0x8dd965365e38f675ULL, 0x8dd958365e38e05eULL }, /* bd_f_9400_bd_m_9400_1 | bd_f_9400_bd_m_9400_1_s | bd_f_9400_bd_m_9400_1_n (4) */
    { 0xfd796966f3c7a7a6ULL, 0x988e963664de34b8ULL, 0x988ea33664de4acfULL }, /* bd_f_9400_bd_m_9400_2 | bd_f_9400_bd_m_9400_2_s | bd_f_9400_bd_m_9400_2_n (8) */
};
static const struct pair_hash k_diffuse_pairs[] = {
    { 0x00178fc6032dce64ULL, 0xdb8158e75415525aULL }, /* BD_A_9470_1_s + BD_A_9470_1 (6) */
    { 0x001c55e4c9de9cbaULL, 0xb31ea758adcafbc0ULL }, /* LG_M_1000_s + LG_M_1000 (6) */
    { 0x012ee3088b73c9d7ULL, 0x9a8cee7738192295ULL }, /* AM_F_9320_s + AM_F_9320 (2) */
    { 0x0251a82fe613a8d0ULL, 0x66d0ad6657eb26aeULL }, /* HD_F_9370_s + HD_F_9370 (4) */
    { 0x025aebf96526d296ULL, 0x6fbada665d0a37c4ULL }, /* HD_F_9480_s + HD_F_9480 (4) */
    { 0x0269092d7cf08daeULL, 0x4211fe35f99a54acULL }, /* BD_M_9371_1_s + BD_M_9371_1 (6) */
    { 0x0272b4cdf9382a04ULL, 0xb7cecf9ad13cc83aULL }, /* LG_M_9341_s + LG_M_9341 (10) */
    { 0x0342b74ca7a33af1ULL, 0x4d79947a15bc8237ULL }, /* LG_M_5320_s + LG_M_5320 (2) */
    { 0x037a464384ddc9ceULL, 0x8e6a17d299fe378cULL }, /* BD_F_9370_1_s + BD_F_9370_1 (6) */
    { 0x05d48469f7f32106ULL, 0x7f4a7d77284c63f4ULL }, /* AM_F_9420_s + AM_F_9420 (2) */
    { 0x068730cd07cbc676ULL, 0x85d561c9a40789e4ULL }, /* BD_A_2870_2_s + BD_A_2870_2 (4) */
    { 0x0759ecdb5c00e30fULL, 0xc564f40f5dab40fdULL }, /* BD_M_9270_2_s + BD_M_9270_2 (2) */
    { 0x084daa418db0bbf6ULL, 0x136b382c12150364ULL }, /* AM_M_9300_s + AM_M_9300 (2) */
    { 0x097bce9c0b5b6a00ULL, 0x83ef9aeede2c801eULL }, /* BD_F_9230_2_s + BD_F_9230_2 (4) */
    { 0x097ca7119f625055ULL, 0x11f63c978b88ed93ULL }, /* BD_F_2540_BD_M_2540_s + BD_F_2540_BD_M_2540 (4) */
    { 0x099fe12fe9d418abULL, 0x66d0ae6657eb2861ULL }, /* HD_F_9371_s + HD_F_9371 (4) */
    { 0x09a4eb186711c351ULL, 0x0bd3ac6f2051d557ULL }, /* LG_F_9740_LG_M_9740_s + LG_F_9740_LG_M_9740 (4) */
    { 0x09c0cdcdfcf8637fULL, 0xb7ced09ad13cc9edULL }, /* LG_M_9340_s + LG_M_9340 (10) */
    { 0x0ad0793dcfa9e49bULL, 0xae3ad94cf5941591ULL }, /* AM_A_9220_s + AM_A_9220 (4) */
    { 0x0b4a7563cdf066c2ULL, 0xf7a5e6c3626a5d78ULL }, /* HD_A_9210_s + HD_A_9210 (6) */
    { 0x0c67f0ff9065623fULL, 0xf9edb191d913acadULL }, /* LG_M_6200_s + LG_M_6200 (4) */
    { 0x0c7672e2ab643f86ULL, 0xe0d5d14d11dade74ULL }, /* AM_A_9490_s + AM_A_9490 (4) */
    { 0x0cf5e2fc7d0969b1ULL, 0x273c74a0fa2e90f7ULL }, /* BD_M_9610_2_s + BD_M_9610_2 (6) */
    { 0x0d80f228f1cf1cf5ULL, 0xca73b73c513be873ULL }, /* BD_M_9460_1_s + BD_M_9460_1 (16) */
    { 0x0e1f189c5b98cd8dULL, 0xdc853f175d7c7f5bULL }, /* BD_A_9240_1_s + BD_A_9240_1 (6) */
    { 0x0e2cade66276d403ULL, 0xc9d98a9adba91ea9ULL }, /* LG_M_9560_s + LG_M_9560 (8) */
    { 0x0f2144b5dade59acULL, 0x8d02b96fec2e1512ULL }, /* HD_A_3320_s + HD_A_3320 (2) */
    { 0x106d3e131750c0f8ULL, 0x68dc76d1d2c857e6ULL }, /* LG_F_9340_s + LG_F_9340 (10) */
    { 0x114d6bc6a68be261ULL, 0xb5c9d16ad2d9da27ULL }, /* HD_M_2540_s + HD_M_2540 (3) */
    { 0x1159e20443058ac6ULL, 0x68d60ad1d2c334b4ULL }, /* LG_F_9360_s + LG_F_9360 (4) */
    { 0x11adbf029c690afbULL, 0xf72058f27c6c5131ULL }, /* BD_A_1000_s + BD_A_1000 (8) */
    { 0x1254640ca084b4b3ULL, 0x3d78abd1ba2f0b99ULL }, /* LG_F_9420_s + LG_F_9420 (4) */
    { 0x12c8f95affd447f1ULL, 0x3dfbed368cdbc737ULL }, /* HD_F_9720_HD_M_9720_s + HD_F_9720_HD_M_9720 (2) */
    { 0x12dfb8d7ef64539bULL, 0x99614dc71a4fec91ULL }, /* HD_M_9510_s + HD_M_9510 (2) */
    { 0x13b65b69187d1989ULL, 0xf04937c35e9ce7bfULL }, /* HD_A_9310_s + HD_A_9310 (10) */
    { 0x14fca93193eb3969ULL, 0xe92ecaafe44b865fULL }, /* HD_M_9349_HD_M_9340_s + HD_M_9349_HD_M_9340 (14) */
    { 0x154cdb5bb1134d1bULL, 0xd4ee9b0aeabbca11ULL }, /* AM_A_1000_s + AM_A_1000 (4) */
    { 0x15dbafc08f4098aaULL, 0xc0c9db9ad66a1470ULL }, /* LG_M_9440_s + LG_M_9440 (6) */
    { 0x15e50fe83b2894f2ULL, 0xb7d9429ad145e028ULL }, /* LG_M_9390_s + LG_M_9390 (8) */
    { 0x161544ace101b871ULL, 0xa5cc62ed437043b7ULL }, /* LG_F_9570_LG_M_9570_s + LG_F_9570_LG_M_9570 (16) */
    { 0x1675ec5224461240ULL, 0xca05989adbce715eULL }, /* LG_M_9590_s + LG_M_9590 (4) */
    { 0x1797c0ff6bea0d41ULL, 0x57c29b8c697c8bc7ULL }, /* AM_M_9520_AM_F_9520_s + AM_M_9520_AM_F_9520 (4) */
    { 0x17ab13fc83aea7f4ULL, 0x273c75a0fa2e92aaULL }, /* BD_M_9610_1_s + BD_M_9610_1 (6) */
    { 0x18362328f8745b38ULL, 0xca73b83c513bea26ULL }, /* BD_M_9460_2_s + BD_M_9460_2 (6) */
    { 0x18d4299c623dd570ULL, 0xdc8540175d7c810eULL }, /* BD_A_9240_2_s + BD_A_9240_2 (2) */
    { 0x19178d72615dceb3ULL, 0xbe4ff822130c9599ULL }, /* BD_M_9450_2_s + BD_M_9450_2 (4) */
    { 0x1a4857131d3c32b3ULL, 0x68dc77d1d2c85999ULL }, /* LG_F_9341_s + LG_F_9341 (10) */
    { 0x1a97ea1cc4eb81a9ULL, 0x961b6043913f919fULL }, /* HD_M_9480_HD_F_9480_s + HD_M_9480_HD_F_9480 (4) */
    { 0x1ae28c2d8a603787ULL, 0x42120135f99a59c5ULL }, /* BD_M_9371_2_s + BD_M_9371_2 (4) */
    { 0x1b1c082c9fd191feULL, 0x772ef36660eb407cULL }, /* HD_F_9530_s + HD_F_9530 (2) */
    { 0x1b8e3a14a8c3ed3dULL, 0x48188ef2121f178bULL }, /* HD_F_9340_HD_M_9340_s + HD_F_9340_HD_M_9340 (22) */
    { 0x1bfedd5b6819ebbeULL, 0xcf248799ae81063cULL }, /* HD_A_4150_s + HD_A_4150 (4) */
    { 0x1d0d8a27245f6e46ULL, 0x0a5f8f2c0cd9ec34ULL }, /* AM_M_9450_s + AM_M_9450 (4) */
    { 0x1d4b128a7d2b1341ULL, 0xaf1b239acc4c21c7ULL }, /* LG_M_9210_s + LG_M_9210 (6) */
    { 0x1d799da08b7361e5ULL, 0x2de3889f704521e3ULL }, /* AM_F_9400_AM_M_9400_s + AM_F_9400_AM_M_9400 (8) */
    { 0x1da5c94393bd5527ULL, 0x8e6a1ad299fe3ca5ULL }, /* BD_F_9370_2_s + BD_F_9370_2 (8) */
    { 0x1edebea35346fe2dULL, 0x2385515e48963d3bULL }, /* LG_F_8200_LG_M_8200_s + LG_F_8200_LG_M_8200 (6) */
    { 0x1f719c10fce44af5ULL, 0x4daea8c9fe2b6673ULL }, /* HD_F_2550_HD_M_2550_s + HD_F_2550_HD_M_2550 (2) */
    { 0x1fd9b3cd15f3610fULL, 0x85d564c9a4078efdULL }, /* BD_A_2870_1_s + BD_A_2870_1 (6) */
    { 0x2167b7434ee88405ULL, 0xd8a419c9a6447ac3ULL }, /* BD_F_9380_1_s + BD_F_9380_1 (6) */
    { 0x21a5c874f5c36b19ULL, 0xd627f3f4a8cc5b8fULL }, /* HD_F_9270_HD_M_9270_s + HD_F_9270_HD_M_9270 (2) */
    { 0x22171311be83ed56ULL, 0xdd18bb6504cfd684ULL }, /* BD_F_9460_1_s + BD_F_9460_1 (12) */
    { 0x22228594d914d906ULL, 0x8031551d990f5bf4ULL }, /* BD_M_9340_1_s + BD_M_9340_1 (10) */
    { 0x2264ee2c54aca3a7ULL, 0xc0f5e99ad68f6725ULL }, /* LG_M_9490_s + LG_M_9490 (8) */
    { 0x228df521bae2550aULL, 0xf78ad6c362537a90ULL }, /* HD_A_9290_s + HD_A_9290 (2) */
    { 0x22e69c169b88d731ULL, 0x28a0e5c74a89c277ULL }, /* LG_F_9200_LG_M_9200_s + LG_F_9200_LG_M_9200 (2) */
    { 0x234831f5d479712dULL, 0xc9e0169adbae783bULL }, /* LG_M_9540_s + LG_M_9540 (6) */
    { 0x239baa23155b469bULL, 0x6fcbd8665d18a791ULL }, /* HD_F_9450_s + HD_F_9450 (2) */
    { 0x23c5fe569e517ee9ULL, 0x061b7a5a0cc9cfdfULL }, /* BD_F_9710_2_s + BD_F_9710_2 (4) */
    { 0x23f7d18ba4b7c318ULL, 0x09b72cc36cdc0bc6ULL }, /* HD_A_9490_s + HD_A_9490 (4) */
    { 0x254fbaf3a67ccd46ULL, 0x1ea054d5a1af3334ULL }, /* BD_M_9530_2_s + BD_M_9530_2 (8) */
    { 0x264559a796e7a569ULL, 0x698683d4473e125fULL }, /* BD_M_9289_BD_M_9280_1_s + BD_M_9289_BD_M_9280_1 (6) */
    { 0x26a3b3a95466e91cULL, 0x98421071227b9a02ULL }, /* LG_M_2560_s + LG_M_2560 (6) */
    { 0x26ed8f3b6adff809ULL, 0x5643edb7f587223fULL }, /* BD_F_5370_BD_M_5370_1_s + BD_F_5370_BD_M_5370_1 (4) */
    { 0x275d0a34a37249bbULL, 0xda596c9ae4c574f1ULL }, /* LG_M_9740_s + LG_M_9740 (4) */
    { 0x27f36b2ec87a2ac9ULL, 0x07cffd6b2cb194ffULL }, /* BD_M_9521_BD_F_9521_1_s + BD_M_9521_BD_F_9521_1 (14) */
    { 0x28701895dae47703ULL, 0x99463dc71a3909a9ULL }, /* HD_M_9590_s + HD_M_9590 (2) */
    { 0x287e26fd91fb5db1ULL, 0x9187d17732e364f7ULL }, /* AM_F_9230_s + AM_F_9230 (2) */
    { 0x287f285c210a95a9ULL, 0x469958d1bf7c859fULL }, /* LG_F_9710_s + LG_F_9710 (14) */
    { 0x28aa17677cf94640ULL, 0xae4bd74cf5a2855eULL }, /* AM_A_9250_s + AM_A_9250 (6) */
    { 0x298fe84353628a08ULL, 0xd8a41ac9a6447c76ULL }, /* BD_F_9380_2_s + BD_F_9380_2 (4) */
    { 0x2a9fac1b3e38aacdULL, 0x10565b76fc8d9c9bULL }, /* BD_F_2520_BD_M_2520_1_s + BD_F_2520_BD_M_2520_1 (8) */
    { 0x2abfc28b63cc2c4dULL, 0xe030541caa12c21bULL }, /* BD_F_9510_BD_M_9510_1_s + BD_F_9510_BD_M_9510_1 (10) */
    { 0x2b1fff78c65fda6dULL, 0x517372f5f80f427bULL }, /* BD_M_5370_1_s + BD_M_5370_1 (4) */
    { 0x2bb1338d7d6b3707ULL, 0xf7b6e4c36278cd45ULL }, /* HD_A_9240_s + HD_A_9240 (4) */
    { 0x2bdba6d5e8f80709ULL, 0x9a786a773807993fULL }, /* AM_F_9380_s + AM_F_9380 (6) */
    { 0x2c066c100fc66c08ULL, 0xc9ea889adbb78e76ULL }, /* LG_M_9510_s + LG_M_9510 (6) */
    { 0x2d2ba462c9a286b1ULL, 0x708aeaadbe0765f7ULL }, /* HD_F_9600_HD_A_9600_s + HD_F_9600_HD_A_9600 (4) */
    { 0x2d5ee0ab82d084eaULL, 0x9695bc8ba713a8b0ULL }, /* BD_A_9700_1_s + BD_A_9700_1 (6) */
    { 0x2e3bc03b6ea05a4cULL, 0x5643eeb7f58723f2ULL }, /* BD_F_5370_BD_M_5370_2_s + BD_F_5370_BD_M_5370_2 (8) */
    { 0x2e3f9b9f3a90518dULL, 0x37f4c8c358cc635bULL }, /* LG_F_9730_LG_M_9730_s + LG_F_9730_LG_M_9730 (12) */
    { 0x2e7a2f56a4f50a2cULL, 0x061b7b5a0cc9d192ULL }, /* BD_F_9710_1_s + BD_F_9710_1 (6) */
    { 0x2fa39eee8e58abf1ULL, 0x75a15d8af4cd8b37ULL }, /* AM_M_9309_AM_M_9300_s + AM_M_9309_AM_M_9300 (2) */
    { 0x303ee55804b18a26ULL, 0x648220b53f2826d4ULL }, /* BD_F_9529_BD_F_9521_1_s + BD_F_9529_BD_F_9521_1 (10) */
    { 0x3082ab95b7d2ce96ULL, 0xafaa56fc4a31d3c4ULL }, /* BD_F_9320_2_s + BD_F_9320_2 (6) */
    { 0x30af4d2266e55155ULL, 0x7a69b8dd6ebd0693ULL }, /* LG_F_9510_LG_M_9510_s + LG_F_9510_LG_M_9510 (10) */
    { 0x30f98aa79d8b30acULL, 0x698684d4473e1412ULL }, /* BD_M_9289_BD_M_9280_2_s + BD_M_9289_BD_M_9280_2 (6) */
    { 0x3101595172e3d3c9ULL, 0x95d31e61b35415ffULL }, /* BD_M_9620_BD_F_9620_1_s + BD_M_9620_BD_F_9620_1 (8) */
    { 0x310822364e8df078ULL, 0x3d89a9d1ba3d7b66ULL }, /* LG_F_9450_s + LG_F_9450 (4) */
    { 0x31ae32f8d4b95e93ULL, 0x127bd7c371db23b9ULL }, /* HD_A_9730_s + HD_A_9730 (2) */
    { 0x31edbd1b41f8d6b0ULL, 0x10565c76fc8d9e4eULL }, /* BD_F_2520_BD_M_2520_2_s + BD_F_2520_BD_M_2520_2 (10) */
    { 0x3202724867f345e9ULL, 0xaf00139acc353edfULL }, /* LG_M_9290_s + LG_M_9290 (6) */
    { 0x320dd38b678c5830ULL, 0xe030551caa12c3ceULL }, /* BD_F_9510_BD_M_9510_2_s + BD_F_9510_BD_M_9510_2 (10) */
    { 0x324baa857930856eULL, 0x1386a82c122c896cULL }, /* AM_M_9380_s + AM_M_9380 (2) */
    { 0x32a89c2ecf1f690cULL, 0x07cffe6b2cb196b2ULL }, /* BD_M_9521_BD_F_9521_2_s + BD_M_9521_BD_F_9521_2 (6) */
    { 0x331835df222fbff1ULL, 0xa38696e20c027f37ULL }, /* BD_F_9350_2_s + BD_F_9350_2 (4) */
    { 0x3445bea0b45c4a35ULL, 0x7589af03e449d5b3ULL }, /* HD_M_9520_HD_F_9520_s + HD_M_9520_HD_F_9520 (2) */
    { 0x34c029c89b5cf97fULL, 0x5da60e6652956fedULL }, /* HD_F_9230_s + HD_F_9230 (4) */
    { 0x35049e9740b6b968ULL, 0x6047304991c61d96ULL }, /* HD_M_6200_s + HD_M_6200 (2) */
    { 0x35d51078cd04e250ULL, 0x517373f5f80f442eULL }, /* BD_M_5370_2_s + BD_M_5370_2 (8) */
    { 0x36facc2cc1ec7a6dULL, 0x36268ed1b66ae27bULL }, /* LG_F_9530_s + LG_F_9530 (6) */
    { 0x376ece1780550655ULL, 0x66c69b6657e2b393ULL }, /* HD_F_9320_s + HD_F_9320 (6) */
    { 0x378f5ad12670fb90ULL, 0x565df5503c43b96eULL }, /* BD_M_9320_1_s + BD_M_9320_1 (6) */
    { 0x38421f4ac1647de5ULL, 0x07fa0359694cdde3ULL }, /* AM_F_5370_AM_M_5370_s + AM_F_5370_AM_M_5370 (2) */
    { 0x3868b404d2878351ULL, 0x03662a75ea919557ULL }, /* LG_F_9260_LG_M_9260_s + LG_F_9260_LG_M_9260 (8) */
    { 0x389099953254eecdULL, 0x724f71e11b34409bULL }, /* BD_F_body_M_s + BD_F_body_M (3) */
    { 0x3914359b16bbdce2ULL, 0x09bdb8c36ce16558ULL }, /* HD_A_9470_s + HD_A_9470 (2) */
    { 0x398585d3191eb301ULL, 0x1638132e0d51bd87ULL }, /* LG_F_2540_LG_M_2540_s + LG_F_2540_LG_M_2540 (6) */
    { 0x3a2307169b5ac021ULL, 0xfa354e2355b684e7ULL }, /* LG_F_9280_LG_M_9280_s + LG_F_9280_LG_M_9280 (6) */
    { 0x3a474794029eb3dbULL, 0xb5b54d6ad2c850d1ULL }, /* HD_M_2520_s + HD_M_2520 (6) */
    { 0x3a6766df25f1d534ULL, 0xa38697e20c0280eaULL }, /* BD_F_9350_1_s + BD_F_9350_1 (6) */
    { 0x3b302e290e6c0975ULL, 0xd1542f9adf8f80f3ULL }, /* LG_M_9610_s + LG_M_9610 (6) */
    { 0x3b3c8ece9967d1d9ULL, 0x7db6905ddfb4674fULL }, /* AM_M_9521_AM_F_9521_s + AM_M_9521_AM_F_9521 (4) */
    { 0x3b42ea1a3967584bULL, 0xd14dc39adf8a5dc1ULL }, /* LG_M_9630_s + LG_M_9630 (6) */
    { 0x3b699611ccab87efULL, 0xdd18be6504cfdb9dULL }, /* BD_F_9460_2_s + BD_F_9460_2 (10) */
    { 0x3bb68a517989120cULL, 0x95d31f61b35417b2ULL }, /* BD_M_9620_BD_F_9620_2_s + BD_M_9620_BD_F_9620_2 (6) */
    { 0x3d6ac82927f20be3ULL, 0x1361262c120c9049ULL }, /* AM_M_9350_s + AM_M_9350 (8) */
    { 0x3d90480314bdf70dULL, 0xaca4051d12230cdbULL }, /* HD_F_2520_HD_M_2520_s + HD_F_2520_HD_M_2520 (6) */
    { 0x3dc93df3b3ec771fULL, 0x1ea057d5a1af384dULL }, /* BD_M_9530_1_s + BD_M_9530_1 (12) */
    { 0x3e010894e965f8dfULL, 0x8031581d990f610dULL }, /* BD_M_9340_2_s + BD_M_9340_2 (8) */
    { 0x3e0866e65837172dULL, 0xc20d14e20a49ae3bULL }, /* LG_F_9720_LG_M_9720_s + LG_F_9720_LG_M_9720 (7) */
    { 0x3e5fd05f72c503beULL, 0x733cf42b060e5e3cULL }, /* BD_M_9440_2_s + BD_M_9440_2 (15) */
    { 0x3ec3eacca19b760aULL, 0x50e7276104f0b390ULL }, /* AM_F_9450_L_s + AM_F_9450_L (3) */
    { 0x3f136ab44573f534ULL, 0xf139182bfed7a0eaULL }, /* AM_M_9720_s + AM_M_9720 (2) */
    { 0x3fb82cbabe695df1ULL, 0x1c9ce32c1770ed37ULL }, /* AM_M_9280_s + AM_M_9280 (2) */
    { 0x4175484cc2aaa840ULL, 0x6fdcd6665d27175eULL }, /* HD_F_9420_s + HD_F_9420 (8) */
    { 0x420bc8f8d2f4ec4dULL, 0x38e1c5853bcd821bULL }, /* LG_M_9520_LG_F_9520_s + LG_M_9520_LG_F_9520 (10) */
    { 0x4277658d42182199ULL, 0xa9c23e57f9ecae0fULL }, /* AM_M_9349_AM_M_9340_s + AM_M_9349_AM_M_9340 (4) */
    { 0x431b5187baf21bd1ULL, 0x1604dc6d4b86f9d7ULL }, /* LG_F_9240_LG_M_9240_s + LG_F_9240_LG_M_9240 (8) */
    { 0x43af580fa9d7a921ULL, 0xa3b81f44877a45e7ULL }, /* AM_F_9270_AM_M_9270_s + AM_F_9270_AM_M_9270 (2) */
    { 0x45914ad826c59491ULL, 0xa9ae1d7d6bff4197ULL }, /* BD_F_9450_1_s + BD_F_9450_1 (6) */
    { 0x45a48f7bb424ab59ULL, 0x9b60df64935b24cfULL }, /* LG_F_9700_LG_M_9700_s + LG_F_9700_LG_M_9700 (4) */
    { 0x45d743ab903e4563ULL, 0x9695bf8ba713adc9ULL }, /* BD_A_9700_2_s + BD_A_9700_2 (4) */
    { 0x460886a1781bd1c1ULL, 0x83518d9889f13c47ULL }, /* HD_F_2540_HD_M_2540_s + HD_F_2540_HD_M_2540 (4) */
    { 0x466d96b72b6d7576ULL, 0xc764564d0398a0e4ULL }, /* AM_A_9700_s + AM_A_9700 (4) */
    { 0x46a6a127fc2001c5ULL, 0x023e86468b8e3f83ULL }, /* BD_M_9710_BD_F_9710_1_s + BD_M_9710_BD_F_9710_1 (6) */
    { 0x477ae0bd57a07bf0ULL, 0x7f6c79772869438eULL }, /* AM_F_9480_s + AM_F_9480 (4) */
    { 0x47f7ee57daaf793bULL, 0xaf069f9acc3a9871ULL }, /* LG_M_9270_s + LG_M_9270 (2) */
    { 0x489ca85e53a4d460ULL, 0xda6a6a9ae4d3e4beULL }, /* LG_M_9730_s + LG_M_9730 (12) */
    { 0x49c820f0c142ede4ULL, 0x9a82fc773810e5daULL }, /* AM_F_9350_s + AM_F_9350 (4) */
    { 0x49d52e95c5fa692fULL, 0xafaa59fc4a31d8ddULL }, /* BD_F_9320_1_s + BD_F_9320_1 (8) */
    { 0x4a6a68581391157fULL, 0x648223b53f282bedULL }, /* BD_F_9529_BD_F_9521_2_s + BD_F_9529_BD_F_9521_2 (8) */
    { 0x4a75371a99c1ddc1ULL, 0x4a3a3735fe146847ULL }, /* BD_M_9370_1_s + BD_M_9370_1 (6) */
    { 0x4c4b6deadc743eefULL, 0xc0daf99ad678ba9dULL }, /* LG_M_9410_s + LG_M_9410 (2) */
    { 0x4c662b815c5a7dcaULL, 0x351e34268c10f350ULL }, /* BD_M_9350_2_s + BD_M_9350_2 (4) */
    { 0x4ca05252b95dc251ULL, 0x5a812828a954bc57ULL }, /* BD_F_9280_BD_M_9280_1_s + BD_F_9280_BD_M_9280_1 (8) */
    { 0x4ce07bd82a87a9d4ULL, 0xa9ae1e7d6bff434aULL }, /* BD_F_9450_2_s + BD_F_9450_2 (6) */
    { 0x4cfc23eff0ba282aULL, 0xe78f050e212497f0ULL }, /* HD_F_9529_HD_F_9521_s + HD_F_9529_HD_F_9521 (2) */
    { 0x4d1a2eba066cb552ULL, 0xef5478dbbf0f0b48ULL }, /* BD_A_9560_1_s + BD_A_9560_1 (10) */
    { 0x4d50a0bbb41a9972ULL, 0xd6f415806ac0d0a8ULL }, /* BD_M_9380_1_s + BD_M_9380_1 (8) */
    { 0x4dadb1d07fa1e601ULL, 0x43dbb22a240cb887ULL }, /* BD_F_9610_BD_M_9610_1_s + BD_F_9610_BD_M_9610_1 (6) */
    { 0x4dc3b6fe5a0e6b2eULL, 0xa9d091c7235e762cULL }, /* HD_M_9720_s + HD_M_9720 (4) */
    { 0x4eced228009a07c8ULL, 0x023e87468b8e4136ULL }, /* BD_M_9710_BD_F_9710_2_s + BD_M_9710_BD_F_9710_2 (4) */
    { 0x4f37222ff41683f6ULL, 0x0c5a84de310d8b64ULL }, /* BD_F_9521_1_s + BD_F_9521_1 (10) */
    { 0x50ab40c950b6015aULL, 0xda16d87d6a3ecda0ULL }, /* BD_A_9630_1_s + BD_A_9630_1 (2) */
    { 0x50e7266104f0b1ddULL, 0x7f46f77728494a6bULL }, /* AM_F_9450_s + AM_F_9450 (9) */
    { 0x51b7d8d168e40329ULL, 0xc76ec84d03a1b71fULL }, /* AM_A_9730_s + AM_A_9730 (2) */
    { 0x5248c85ffec23bb5ULL, 0x3d9aa7d1ba4beb33ULL }, /* LG_F_9480_s + LG_F_9480 (20) */
    { 0x529d681a9e3be3c4ULL, 0x4a3a3835fe1469faULL }, /* BD_M_9370_2_s + BD_M_9370_2 (4) */
    { 0x52f7de2ff6cc9c68ULL, 0x71e89fd1d8044896ULL }, /* LG_F_9230_s + LG_F_9230 (9) */
    { 0x5321d53b52b9a797ULL, 0xd99b1e4d0e2a4855ULL }, /* AM_A_9570_s + AM_A_9570 (8) */
    { 0x53305fc5717bb10dULL, 0xb2aab3a4e9f436dbULL }, /* HD_F_9510_HD_M_9510_s + HD_F_9510_HD_M_9510 (2) */
    { 0x53bc7bada1b43c61ULL, 0xf064c7c35eb4a427ULL }, /* HD_A_9390_s + HD_A_9390 (4) */
    { 0x541b307218ddf7aeULL, 0xaf11119acc43aeacULL }, /* LG_M_9220_s + LG_M_9220 (6) */
    { 0x54acd536e1489895ULL, 0x0284be4d34f2f9d3ULL }, /* LG_F_9560_LG_M_9560_s + LG_F_9560_LG_M_9560 (8) */
    { 0x54d44fc509f770f6ULL, 0xb7cb4a9ad139b064ULL }, /* LG_M_9350_s + LG_M_9350 (4) */
    { 0x557398d886d9ac75ULL, 0xa0c0e2c71e1f6bf3ULL }, /* HD_M_9400_s + HD_M_9400 (2) */
    { 0x558654c9b1d4fb4bULL, 0xa0ba76c71e1a48c1ULL }, /* HD_M_9420_s + HD_M_9420 (4) */
    { 0x55ba97ac76a67ef9ULL, 0x8a0c6e1d9efacdafULL }, /* BD_M_9341_1_s + BD_M_9341_1 (10) */
    { 0x55d5e2d0841bec04ULL, 0x43dbb32a240cba3aULL }, /* BD_F_9610_BD_M_9610_2_s + BD_F_9610_BD_M_9610_2 (6) */
    { 0x562ae6d10acf19d9ULL, 0x095c0a0aaf526f4fULL }, /* HD_M_9389_HD_M_9380_s + HD_M_9389_HD_M_9380 (4) */
    { 0x562c0ed02acc0970ULL, 0xcc1e41c736b3950eULL }, /* HD_M_9340_s + HD_M_9340 (14) */
    { 0x5668fbabf853167dULL, 0x4fef1f8581501ccbULL }, /* AM_M_9230_AM_F_9230_s + AM_M_9230_AM_F_9230 (2) */
    { 0x56d8535f8032fa97ULL, 0x733cf72b060e6355ULL }, /* BD_M_9440_1_s + BD_M_9440_1 (18) */
    { 0x57548352c0014d94ULL, 0x5a812928a954be0aULL }, /* BD_F_9280_BD_M_9280_2_s + BD_F_9280_BD_M_9280_2 (6) */
    { 0x587527f0b6b3ac0aULL, 0xf3340d384a73f990ULL }, /* BD_M_9379_BD_M_9372_2_s + BD_M_9379_BD_M_9372_2 (2) */
    { 0x58fd4770f2ed4ea7ULL, 0xef0e08ecb5031a25ULL }, /* BD_A_9600_fur_s + BD_A_9600_fur (2) */
    { 0x593f4fdd6f7464daULL, 0xc9d6049adba60520ULL }, /* LG_M_9570_s + LG_M_9570 (16) */
    { 0x595304664f7d570cULL, 0x77ea4877247bd4b2ULL }, /* AM_F_9710_s + AM_F_9710 (2) */
    { 0x5975809273d455ddULL, 0x084f157c8ac84e6bULL }, /* LG_M_9389_LG_M_9380_s + LG_M_9389_LG_M_9380 (6) */
    { 0x5a191d6065847ec1ULL, 0x54f966c6e41f2147ULL }, /* LG_F_9500_LG_M_9500_s + LG_F_9500_LG_M_9500 (8) */
    { 0x5a8a26467db32021ULL, 0x380b94672fb7e4e7ULL }, /* HD_A_2870_s + HD_A_2870 (4) */
    { 0x5b2e1e5e74d7096dULL, 0xe5da0ced77f9d97bULL }, /* BD_F_2550_BD_M_2550_s + BD_F_2550_BD_M_2550 (6) */
    { 0x5b86e5bdb2d13e80ULL, 0xb5c64b6ad2d6c09eULL }, /* HD_M_2550_s + HD_M_2550 (2) */
    { 0x5c7e7a322cb6db81ULL, 0xb0fd870815ca3207ULL }, /* HD_M_9710_HD_F_9710_s + HD_M_9710_HD_F_9710 (8) */
    { 0x5c9ea9b30fc68921ULL, 0xb249c766f5ae25e7ULL }, /* HD_F_9440_HD_M_9440_s + HD_F_9440_HD_M_9440 (6) */
    { 0x5d09c8ac7a68943cULL, 0x8a0c6f1d9efacf62ULL }, /* BD_M_9341_2_s + BD_M_9341_2 (8) */
    { 0x5d7a47d02e8c794bULL, 0xcc1e42c736b396c1ULL }, /* HD_M_9341_s + HD_M_9341 (14) */
    { 0x5e4fbb58eb8dc682ULL, 0x724f71e11b34409bULL }, /* BD_M_body_M_s + BD_F_body_M (1) */
    { 0x5e4fbb58eb8dc682ULL, 0x777ed02aecc3b938ULL }, /* BD_M_body_M_s + BD_M_body_M (224) */
    { 0x5e4fbb58eb8dc682ULL, 0xcf6e2339c3593fb8ULL }, /* BD_M_body_M_s + BD_M_body (1) */
    { 0x5ea13d4f17f44785ULL, 0xae41c54cf59a1243ULL }, /* AM_A_9200_s + AM_A_9200 (2) */
    { 0x5ed60dbf55ffa62aULL, 0x886e0f772d9be5f0ULL }, /* AM_F_9521_s + AM_F_9521 (4) */
    { 0x5f38c79c711ed776ULL, 0x88f555666b1d32e4ULL }, /* HD_F_9710_s + HD_F_9710 (8) */
    { 0x601e91df476dfea9ULL, 0xb7d5bc9ad142c69fULL }, /* LG_M_9360_s + LG_M_9360 (6) */
    { 0x604ed3e664fcb889ULL, 0x5b025ef6034a6ebfULL }, /* BD_A_9200_1_s + BD_A_9200_1 (4) */
    { 0x61460d76560956a9ULL, 0x9262d3d015865e9fULL }, /* HD_M_9521_HD_F_9521_s + HD_M_9521_HD_F_9521 (2) */
    { 0x6226ebb480dc9400ULL, 0x707b05468a039a1eULL }, /* AM_A_8200_s + AM_A_8200 (2) */
    { 0x62df1d15f5da9cd5ULL, 0x886e58df3aa8e613ULL }, /* BD_A_9509_BD_A_9500_1_s + BD_A_9509_BD_A_9500_1 (10) */
    { 0x63b12e0ec31b9cc2ULL, 0xbc04a67254c8a378ULL }, /* LG_F_9529_LG_F_9521_s + LG_F_9529_LG_F_9521 (8) */
    { 0x63ca919020287584ULL, 0x00b89bc367aba7baULL }, /* HD_A_9560_s + HD_A_9560 (4) */
    { 0x63d3b2f08633eb22ULL, 0x3b0f959565284b98ULL }, /* BD_M_9740_1_s + BD_M_9740_1 (4) */
    { 0x63e83dd6d2982095ULL, 0x07177b1cc47f41d3ULL }, /* BD_A_8200_2_s + BD_A_8200_2 (4) */
    { 0x63ec826ab0b90576ULL, 0x3bfa0f55c609b0e4ULL }, /* AM_M_6200_s + AM_M_6200 (4) */
    { 0x63ed5bcaed26e4fdULL, 0x8879b7e787b1c74bULL }, /* BD_M_9510_1_s + BD_M_9510_1 (10) */
    { 0x65c923bbc188904bULL, 0xd6f418806ac0d5c1ULL }, /* BD_M_9380_2_s + BD_M_9380_2 (10) */
    { 0x65fab7c4a421443aULL, 0x5e9afb865f00d740ULL }, /* BD_F_9420_1_s + BD_F_9420_1 (6) */
    { 0x6670b97e9fa04cadULL, 0x66c2653c9c8167bbULL }, /* LG_M_9710_LG_F_9710_s + LG_M_9710_LG_F_9710 (14) */
    { 0x66928e816b3b85c3ULL, 0x351e37268c10f869ULL }, /* BD_M_9350_1_s + BD_M_9350_1 (6) */
    { 0x66b07ce937aeb685ULL, 0x996839c71a55e943ULL }, /* HD_M_9530_s + HD_M_9530 (2) */
    { 0x6745b1ba154c40abULL, 0xef547bdbbf0f1061ULL }, /* BD_A_9560_2_s + BD_A_9560_2 (2) */
    { 0x679d04e668bd1accULL, 0x5b025ff6034a7072ULL }, /* BD_A_9200_2_s + BD_A_9200_2 (2) */
    { 0x6889a530023e1e8fULL, 0x0c5a87de310d907dULL }, /* BD_F_9521_2_s + BD_F_9521_2 (8) */
    { 0x69021dfc11dcc69dULL, 0xc5c0d538f58f532bULL }, /* LG_M_9349_LG_M_9340_s + LG_M_9349_LG_M_9340 (10) */
    { 0x69378c818ae203e0ULL, 0xaf179d9acc49083eULL }, /* LG_M_9200_s + LG_M_9200 (2) */
    { 0x6960c70abd8d4a94ULL, 0x68d90fd1d2c5730aULL }, /* LG_F_9371_s + LG_F_9371 (2) */
    { 0x696c2a47c19558eaULL, 0x01b3953ef3405cb0ULL }, /* BD_M_9300_1_s + BD_M_9300_1 (6) */
    { 0x698b26bf5ca4bba5ULL, 0x886e10772d9be7a3ULL }, /* AM_F_9520_s + AM_F_9520 (4) */
    { 0x69f53912b81febc1ULL, 0xc7a82714f101c647ULL }, /* AM_F_9510_AM_M_9510_s + AM_F_9510_AM_M_9510 (2) */
    { 0x69fda3c95edd6593ULL, 0xda16db7d6a3ed2b9ULL }, /* BD_A_9630_2_s + BD_A_9630_2 (2) */
    { 0x6a2d4e15f99aff18ULL, 0x886e59df3aa8e7c6ULL }, /* BD_A_9509_BD_A_9500_2_s + BD_A_9509_BD_A_9500_2 (4) */
    { 0x6b1e3e74986c50ddULL, 0x43b9e4c41792d16bULL }, /* AM_F_9610_AM_M_9610_s + AM_F_9610_AM_M_9610 (2) */
    { 0x6bb40771faf9563dULL, 0x6c4c821a8d67588bULL }, /* LG_F_2560_LG_M_2560_s + LG_F_2560_LG_M_2560 (6) */
    { 0x6be1c71a722f29e1ULL, 0x9a93fa77381f55a7ULL }, /* AM_F_9300_s + AM_F_9300 (2) */
    { 0x6bf2cddbdf6d68d9ULL, 0x7a9526b2752f264fULL }, /* BD_M_9349_BD_M_9340_1_s + BD_M_9349_BD_M_9340_1 (10) */
    { 0x6c166ccaf1a267a0ULL, 0x8879b8e787b1c8feULL }, /* BD_M_9510_2_s + BD_M_9510_2 (10) */
    { 0x6c5537dc76ea188bULL, 0xdc14f680f1dea601ULL }, /* BD_A_underwear_s + BD_A_underwear (40) */
    { 0x6d39e44be574189cULL, 0x6ede9f771f40bd82ULL }, /* AM_F_9620_s + AM_F_9620 (8) */
    { 0x6e5babece178710cULL, 0xc9dc909adbab5eb2ULL }, /* LG_M_9550_s + LG_M_9550 (6) */
    { 0x6e9d6ed6d93d5ed8ULL, 0x07177c1cc47f4386ULL }, /* BD_A_8200_1_s + BD_A_8200_1 (12) */
    { 0x6f2a35a05f3abe53ULL, 0x983e8a7122788079ULL }, /* LG_M_2550_s + LG_M_2550 (2) */
    { 0x6f8726c0736122d4ULL, 0x019dea2c07dd144aULL }, /* AM_M_9540_s + AM_M_9540 (2) */
    { 0x704eb51b3f83f7cdULL, 0xc3ed3a0bbb7d219bULL }, /* BD_F_9300_1_s + BD_F_9300_1 (6) */
    { 0x70705176e5ade1e1ULL, 0x38f5e87918f74da7ULL }, /* AM_M_2520_s + AM_M_2520 (4) */
    { 0x70822006f0b9e591ULL, 0xa6bd8ea3c6142a97ULL }, /* AM_F_9260_AM_M_9260_s + AM_F_9260_AM_M_9260 (4) */
    { 0x7189e00ac208dacfULL, 0x68d910d1d2c574bdULL }, /* LG_F_9370_s + LG_F_9370 (2) */
    { 0x722e9b1405e2a89eULL, 0xa5643037356df9dcULL }, /* BD_M_9220_2_s + BD_M_9220_2 (4) */
    { 0x724f5fe11b342205ULL, 0xcf6e2339c3593fb8ULL }, /* BD_F_body_s + BD_M_body (15) */
    { 0x724f5fe11b342205ULL, 0xe17fd5d9c574e8c3ULL }, /* BD_F_body_s + BD_F_body (96) */
    { 0x72a18af0c594b403ULL, 0xf33410384a73fea9ULL }, /* BD_M_9379_BD_M_9372_1_s + BD_M_9379_BD_M_9372_1 (2) */
    { 0x73bda1dad0acf6c1ULL, 0xdc7cfa17944ad947ULL }, /* HD_F_9540_HD_M_9540_s + HD_F_9540_HD_M_9540 (2) */
    { 0x73f2d61cfbe22522ULL, 0x1b17fbca50f5f598ULL }, /* BD_A_9310_2_s + BD_A_9310_2 (10) */
    { 0x74eb0131ceaee915ULL, 0x5cb9f89ff0f55653ULL }, /* LG_F_2520_LG_M_2520_s + LG_F_2520_LG_M_2520 (10) */
    { 0x75ea2244fcfc2c79ULL, 0x6fa5e965f3823f2fULL }, /* AM_F_9720_AM_M_9720_s + AM_F_9720_AM_M_9720 (2) */
    { 0x76140deeba2a31fbULL, 0xb7dc489ad1482031ULL }, /* LG_M_9380_s + LG_M_9380 (6) */
    { 0x7615a43cc15e9332ULL, 0x1d060f67201eb268ULL }, /* HD_A_2570_s + HD_A_2570 (2) */
    { 0x76a7fedbe612a71cULL, 0x7a9527b2752f2802ULL }, /* BD_M_9349_BD_M_9340_2_s + BD_M_9349_BD_M_9340_2 (8) */
    { 0x76c6f2f3620938f0ULL, 0xa0cb74c71e28b88eULL }, /* HD_M_9450_s + HD_M_9450 (4) */
    { 0x773e6dc6c5ba5e79ULL, 0x2f69bf742897212fULL }, /* LG_F_9390_LG_M_9390_s + LG_F_9390_LG_M_9390 (8) */
    { 0x777ede2aecc3d102ULL, 0x777ed02aecc3b938ULL }, /* BD_M_body_s + BD_M_body_M (1) */
    { 0x777ede2aecc3d102ULL, 0xcf6e2339c3593fb8ULL }, /* BD_M_body_s + BD_M_body (110) */
    { 0x779cc61b434423b0ULL, 0xc3ed3b0bbb7d234eULL }, /* BD_F_9300_2_s + BD_F_9300_2 (6) */
    { 0x779db5848b22353eULL, 0xf7b35ec36275b3bcULL }, /* HD_A_9250_s + HD_A_9250 (8) */
    { 0x783c54d22fd04925ULL, 0xd51fd8c73be63923ULL }, /* HD_M_9260_s + HD_M_9260 (8) */
    { 0x78d0189c0bde70d5ULL, 0x6623900562fc9a13ULL }, /* LG_F_2550_LG_M_2550_s + LG_F_2550_LG_M_2550 (2) */
    { 0x7aacf52ae891ad75ULL, 0x993b4c50cd8084f3ULL }, /* LG_M_9289_LG_M_9280_s + LG_M_9289_LG_M_9280 (6) */
    { 0x7b4c7b1113f19fcdULL, 0xde23edc3541a899bULL }, /* HD_A_9130_s + HD_A_9130 (4) */
    { 0x7baa68dab18fa147ULL, 0x01a85c2c07e62a85ULL }, /* AM_M_9530_s + AM_M_9530 (2) */
    { 0x7c69f2a0511789cdULL, 0xbceba30811c8639bULL }, /* BD_M_2520_1_s + BD_M_2520_1 (9) */
    { 0x7d061d98509daab9ULL, 0x3e5fdb5f72c5166fULL }, /* BD_M_9440_2_L_s + BD_M_9440_2_L (5) */
    { 0x7d150c3f74f0c928ULL, 0xaefc8d9acc322556ULL }, /* LG_M_9280_s + LG_M_9280 (6) */
    { 0x7d2ed7cce732c74dULL, 0x01fa980ab8aae51bULL }, /* LG_F_9550_LG_M_9550_s + LG_F_9550_LG_M_9550 (6) */
    { 0x7d3d882da362be4fULL, 0x0a62952c0cdc2c3dULL }, /* AM_M_9440_s + AM_M_9440 (4) */
    { 0x7d98a0c74dd924baULL, 0x07e098b4b30a43c0ULL }, /* BD_A_9490_1_s + BD_A_9490_1 (8) */
    { 0x7dff35f09513767bULL, 0x3b0f9895652850b1ULL }, /* BD_M_9740_2_s + BD_M_9740_2 (2) */
    { 0x7e42684605fc52d3ULL, 0x1c6d4f2c174880f9ULL }, /* AM_M_9260_s + AM_M_9260 (4) */
    { 0x7e7cda7bb7d54c61ULL, 0x673a14bdd77f3427ULL }, /* BD_M_9540_2_s + BD_M_9540_2 (2) */
    { 0x7f4d1ac4b248a873ULL, 0x5e9afe865f00dc59ULL }, /* BD_F_9420_2_s + BD_F_9420_2 (8) */
    { 0x7ff49f48309839a1ULL, 0xd5b2eb64418ce267ULL }, /* LG_M_9230_LG_F_9230_s + LG_M_9230_LG_F_9230 (10) */
    { 0x808fd7023a2ba50cULL, 0xae22cf4cf57f72b2ULL }, /* AM_A_9290_s + AM_A_9290 (2) */
    { 0x80d8f29d88507d02ULL, 0xcc09bdc736a20bb8ULL }, /* HD_M_9320_s + HD_M_9320 (6) */
    { 0x812cbd62e44e9da1ULL, 0x04efcec4cd60a667ULL }, /* LG_F_2570_LG_M_2570_s + LG_F_2570_LG_M_2570 (9) */
    { 0x819d0ad124ee1da4ULL, 0xfa45412c0413919aULL }, /* AM_M_9610_s + AM_M_9610 (2) */
    { 0x81e48d47cf031963ULL, 0x01b3983ef34061c9ULL }, /* BD_M_9300_2_s + BD_M_9300_2 (10) */
    { 0x820d4623cee9c74cULL, 0x362308d1b667c8f2ULL }, /* LG_F_9520_s + LG_F_9520 (10) */
    { 0x82428bd2776f329cULL, 0xc0d0e79ad6704782ULL }, /* LG_M_9460_s + LG_M_9460 (8) */
    { 0x827a5dd542b455daULL, 0x8583f5a2a2bd8e20ULL }, /* HD_A_5320_s + HD_A_5320 (4) */
    { 0x836347051632ef95ULL, 0xa093c7e5f18478d3ULL }, /* LG_F_9210_LG_M_9210_s + LG_F_9210_LG_M_9210 (6) */
    { 0x83b803a054d7b5b0ULL, 0xbceba40811c8654eULL }, /* BD_M_2520_2_s + BD_M_2520_2 (10) */
    { 0x84311972c0f47ae9ULL, 0xcffbfbab98106bdfULL }, /* BD_A_9290_2_s + BD_A_9290_2 (2) */
    { 0x84f20b7bbaddbde4ULL, 0x673a15bdd77f35daULL }, /* BD_M_9540_1_s + BD_M_9540_1 (16) */
    { 0x860444489f3201caULL, 0x7f3ce5772840d750ULL }, /* AM_F_9460_s + AM_F_9460 (4) */
    { 0x866d96a31f9800f5ULL, 0xdcd0edef5f5e2c73ULL }, /* HD_F_9280_HD_M_9280_s + HD_F_9280_HD_M_9280 (6) */
    { 0x86d2b9afd36893bdULL, 0x98451671227dda0bULL }, /* LG_M_2570_s + LG_M_2570 (10) */
    { 0x876842cef0491032ULL, 0x5dc4319cde49e768ULL }, /* BD_M_2550_s + BD_M_2550 (5) */
    { 0x881b6746c5597a31ULL, 0x9b83b7ca76c4ad77ULL }, /* BD_A_9500_2_s + BD_A_9500_2 (4) */
    { 0x887e6a2034f14fbaULL, 0x135da02c120976c0ULL }, /* AM_M_9340_s + AM_M_9340 (4) */
    { 0x88d25c2218f9f4a9ULL, 0x25bec542f7d3cc9fULL }, /* AM_A_9509_AM_A_9500_s + AM_A_9509_AM_A_9500 (4) */
    { 0x88f736d40ae79de6ULL, 0xd0707f4d08d49194ULL }, /* AM_A_9630_s + AM_A_9630 (2) */
    { 0x896236ef80a9d251ULL, 0x300118e112524c57ULL }, /* AM_M_9389_AM_M_9380_s + AM_M_9389_AM_M_9380 (2) */
    { 0x89f16daca438c0e3ULL, 0xb7c1389ad1313d49ULL }, /* LG_M_9300_s + LG_M_9300 (6) */
    { 0x8bbfe48e545ac0d6ULL, 0x3b1bc0ea5d89fe04ULL }, /* BD_F_9341_1_s + BD_F_9341_1 (8) */
    { 0x8c3552168ec7e049ULL, 0xc9ed8e9adbb9ce7fULL }, /* LG_M_9500_s + LG_M_9500 (8) */
    { 0x8c5a1e1414c233f7ULL, 0xa5643337356dfef5ULL }, /* BD_M_9220_1_s + BD_M_9220_1 (4) */
    { 0x8cc27f23d58f1327ULL, 0x362309d1b667caa5ULL }, /* LG_F_9521_s + LG_F_9521 (8) */
    { 0x8d0c2f12d9e0acd4ULL, 0xe0e9d54d11eb8e4aULL }, /* AM_A_9470_s + AM_A_9470 (2) */
    { 0x8d5e772b0e5127f1ULL, 0x9a78f43faeeda737ULL }, /* LG_F_9220_LG_M_9220_s + LG_F_9220_LG_M_9220 (6) */
    { 0x8dd965365e38f675ULL, 0xfd796866f3c7a5f3ULL }, /* BD_F_9400_BD_M_9400_1_s + BD_F_9400_BD_M_9400_1 (4) */
    { 0x8e1e591d0ac1b07bULL, 0x1b17feca50f5fab1ULL }, /* BD_A_9310_1_s + BD_A_9310_1 (8) */
    { 0x8e40d4a72824d3d6ULL, 0xa44177e4b210d904ULL }, /* BD_A_9570_2_s + BD_A_9570_2 (8) */
    { 0x8ee54a72c798062cULL, 0xcffbfcab98106d92ULL }, /* BD_A_9290_1_s + BD_A_9290_1 (6) */
    { 0x8f8ff3ca0eb3db98ULL, 0x984f88712286f046ULL }, /* LG_M_2520_s + LG_M_2520 (10) */
    { 0x8fc3c198bc4b2ab9ULL, 0x1ad91da62036966fULL }, /* LG_F_9310_LG_M_9310_s + LG_F_9310_LG_M_9310 (4) */
    { 0x903f28d0c33ba755ULL, 0xf30bd64ba4716c93ULL }, /* BD_F_2560_BD_M_2560_1_s + BD_F_2560_BD_M_2560_1 (6) */
    { 0x90a68320396b2cf5ULL, 0x135da12c12097873ULL }, /* AM_M_9341_s + AM_M_9341 (4) */
    { 0x90c6ccea2393bb11ULL, 0x01aee82c07eb8417ULL }, /* AM_M_9510_s + AM_M_9510 (2) */
    { 0x918aa1e2ea461e8aULL, 0x68c872d1d2b7a810ULL }, /* LG_F_9320_s + LG_F_9320 (6) */
    { 0x91ef9e347181f814ULL, 0x68ea0ed1d2d3e48aULL }, /* LG_F_9380_s + LG_F_9380 (6) */
    { 0x9224c42df9460a4fULL, 0x3d8643d1ba3a983dULL }, /* LG_F_9460_s + LG_F_9460 (6) */
    { 0x92d09846cbfeb874ULL, 0x9b83b8ca76c4af2aULL }, /* BD_A_9500_1_s + BD_A_9500_1 (10) */
    { 0x92eb913f623cc2c8ULL, 0x1b98fec377258436ULL }, /* HD_A_9630_s + HD_A_9630 (2) */
    { 0x934ec0fa9449a595ULL, 0xa16c5fbeda623ed3ULL }, /* LG_M_9509_LG_M_9500_s + LG_M_9509_LG_M_9500 (8) */
    { 0x937f9e7fa5199621ULL, 0x711e60a2571f6ae7ULL }, /* LG_F_9400_LG_M_9400_s + LG_F_9400_LG_M_9400 (19) */
    { 0x951b6eacf99b1bd4ULL, 0xcc1049c736a7654aULL }, /* HD_M_9300_s + HD_M_9300 (10) */
    { 0x953a700157befb05ULL, 0xbd68967f6e0519c3ULL }, /* HD_A_1000_s + HD_A_1000 (2) */
    { 0x957fb427c098f9a1ULL, 0xdfb93e8aa05fa267ULL }, /* BD_F_9270_BD_M_9270_1_s + BD_F_9270_BD_M_9270_1 (2) */
    { 0x968a77c73126016cULL, 0xcc1abac736b079d2ULL }, /* HD_M_9371_s + HD_M_9371 (4) */
    { 0x96eb03c75c0088f3ULL, 0x07e09bb4b30a48d9ULL }, /* BD_A_9490_2_s + BD_A_9490_2 (6) */
    { 0x9727d668c7886330ULL, 0x6ee71413ce32d6ceULL }, /* AM_A_2570_s + AM_A_2570 (2) */
    { 0x973a7a4099028699ULL, 0x7aeac84a763d8b0fULL }, /* LG_M_9521_LG_F_9521_s + LG_M_9521_LG_F_9521 (8) */
    { 0x97554b803782d30dULL, 0x48495098059888dbULL }, /* AM_F_9280_AM_M_9280_s + AM_F_9280_AM_M_9280 (2) */
    { 0x975b98b27f27477aULL, 0xdf1bddc5a4372580ULL }, /* AM_F_9529_AM_F_9521_s + AM_F_9529_AM_F_9521 (4) */
    { 0x975defe1e9719966ULL, 0xc0d7739ad675a114ULL }, /* LG_M_9400_s + LG_M_9400 (18) */
    { 0x978d59d0c6fc0998ULL, 0xf30bd74ba4716e46ULL }, /* BD_F_2560_BD_M_2560_2_s + BD_F_2560_BD_M_2560_2 (2) */
    { 0x988e963664de34b8ULL, 0xfd796966f3c7a7a6ULL }, /* BD_F_9400_BD_M_9400_2_s + BD_F_9400_BD_M_9400_2 (8) */
    { 0x996a2d4441c0b35bULL, 0x38e1647918e5c451ULL }, /* AM_M_2540_s + AM_M_2540 (4) */
    { 0x996c3049b486df1dULL, 0xe3a4938a923837abULL }, /* BD_M_9280_1_s + BD_M_9280_1 (6) */
    { 0x9a7b63af95907b96ULL, 0x364ef4baf9acb8c4ULL }, /* HD_A_8200_s + HD_A_8200 (2) */
    { 0x9d3e4a05cba0020aULL, 0x0a51f72c0cce5f90ULL }, /* AM_M_9410_s + AM_M_9410 (2) */
    { 0x9e3557325fb8b50eULL, 0xd997984d0e272eccULL }, /* AM_A_9560_s + AM_A_9560 (4) */
    { 0x9e8f19d9bd3de750ULL, 0x513db3312cadc12eULL }, /* BD_F_9480_s + BD_F_9480 (6) */
    { 0x9f2213238b6da7a4ULL, 0xd9912c4d0e220b9aULL }, /* AM_A_9500_s + AM_A_9500 (4) */
    { 0x9f2eb26925dd0525ULL, 0xaf0d8b9acc409523ULL }, /* LG_M_9250_s + LG_M_9250 (10) */
    { 0x9f4bf85e31fc9961ULL, 0x9845d0d29feab927ULL }, /* BD_F_9371_1_s + BD_F_9371_1 (6) */
    { 0x9f5ae527c6849424ULL, 0xdfb93f8aa05fa41aULL }, /* BD_F_9270_BD_M_9270_2_s + BD_F_9270_BD_M_9270_2 (2) */
    { 0x9fd466758d319e69ULL, 0xd25a6f29f798635fULL }, /* HD_M_9289_HD_M_9280_s + HD_M_9289_HD_M_9280 (6) */
    { 0x9fe14149b78f1a40ULL, 0xe3a4948a9238395eULL }, /* BD_M_9280_2_s + BD_M_9280_2 (6) */
    { 0xa01c31fc24bea1d9ULL, 0xc0e1e59ad67eb74fULL }, /* LG_M_9430_s + LG_M_9430 (2) */
    { 0xa0cf933b0aed3272ULL, 0x1368312c1212c1a8ULL }, /* AM_M_9371_s + AM_M_9371 (4) */
    { 0xa1046967b86c0901ULL, 0xe66554841c252387ULL }, /* BD_M_9720_2_s + BD_M_9720_2 (8) */
    { 0xa13eb0c737c99a47ULL, 0xcc1abbc736b07b85ULL }, /* HD_M_9370_s + HD_M_9370 (4) */
    { 0xa173d6c0bf8dac82ULL, 0xa0b6f0c71e172f38ULL }, /* HD_M_9430_s + HD_M_9430 (4) */
    { 0xa1ff577f04bbd9d1ULL, 0x68f12290e72887d7ULL }, /* LG_F_9540_LG_M_9540_s + LG_F_9540_LG_M_9540 (4) */
    { 0xa2b1f01e51e81531ULL, 0x6bc75e00d244d077ULL }, /* LG_F_9440_LG_M_9440_s + LG_F_9440_LG_M_9440 (6) */
    { 0xa3de2737bd5707ddULL, 0x5e4fae58eb8db06bULL }, /* BD_M_body_M_L_s + BD_M_body_M_L (1) */
    { 0xa512678e62825b6fULL, 0x3b1bc3ea5d8a031dULL }, /* BD_F_9341_2_s + BD_F_9341_2 (6) */
    { 0xa53dd88b5de3439dULL, 0x1e1c0ee70776882bULL }, /* HD_F_9260_HD_M_9260_s + HD_F_9260_HD_M_9260 (8) */
    { 0xa5c1295e35050ae4ULL, 0x9845d1d29feabadaULL }, /* BD_F_9371_2_s + BD_F_9371_2 (8) */
    { 0xa744cc3b0df5b18dULL, 0x1368322c1212c35bULL }, /* AM_M_9370_s + AM_M_9370 (4) */
    { 0xa77ffdf25d021b7cULL, 0x68cefed1d2bd01a2ULL }, /* LG_F_9300_s + LG_F_9300 (6) */
    { 0xa79357a7364c6e6fULL, 0xa4417ae4b210de1dULL }, /* BD_A_9570_1_s + BD_A_9570_1 (6) */
    { 0xa92c9a67bce60f04ULL, 0xe66555841c25253aULL }, /* BD_M_9720_1_s + BD_M_9720_1 (6) */
    { 0xa9daaf350d42f7a5ULL, 0x9f9464d2a3abc3a3ULL }, /* BD_F_9372_2_s + BD_F_9372_2 (2) */
    { 0xaaabce6ae51b3f3cULL, 0xcbf539c736908262ULL }, /* HD_M_9380_s + HD_M_9380 (4) */
    { 0xaab31958cdb47005ULL, 0x24a930333bfa86c3ULL }, /* HD_M_9230_HD_F_9230_s + HD_M_9230_HD_F_9230 (4) */
    { 0xab0c3d763e2dbe39ULL, 0x3cae991857d67defULL }, /* HD_F_9610_HD_M_9610_s + HD_F_9610_HD_M_9610 (4) */
    { 0xab322bd6546d34e8ULL, 0xb7d2369ad13fad16ULL }, /* LG_M_9370_s + LG_M_9370 (2) */
    { 0xaeb095a18567b061ULL, 0x0be9f79f25cef827ULL }, /* HD_F_9400_HD_M_9400_s + HD_F_9400_HD_M_9400 (2) */
    { 0xaedd13872d25cffbULL, 0x00b515c367a88e31ULL }, /* HD_A_9550_s + HD_A_9550 (2) */
    { 0xb04fe035104b6928ULL, 0x9f9465d2a3abc556ULL }, /* BD_F_9372_1_s + BD_F_9372_1 (2) */
    { 0xb0b2a217ba5f0da7ULL, 0x2ef85f672a75c125ULL }, /* HD_A_2780_s + HD_A_2780 (8) */
    { 0xb10255d470c619ddULL, 0xaafceb0117c8726bULL }, /* BD_F_5320_BD_M_5320_1_s + BD_F_5320_BD_M_5320_1 (6) */
    { 0xb102c3f8793a9601ULL, 0xd29e02376425e887ULL }, /* LG_F_9490_LG_M_9490_s + LG_F_9490_LG_M_9490 (8) */
    { 0xb1321d373ba667c1ULL, 0xd2fbf24f41ffe247ULL }, /* AM_M_5370_s + AM_M_5370 (2) */
    { 0xb209306e811bf2d5ULL, 0x250bbc0e68504c13ULL }, /* BD_A_9250_2_s + BD_A_9250_2 (6) */
    { 0xb28044d6582d6e63ULL, 0xb7d2379ad13faec9ULL }, /* LG_M_9371_s + LG_M_9371 (2) */
    { 0xb3278e15a0727b96ULL, 0x4033b500bbbeb8c4ULL }, /* BD_M_9410_1_s + BD_M_9410_1 (4) */
    { 0xb37688ecf31f1f39ULL, 0xc6f75a3d3031f6efULL }, /* BD_F_9620_2_s + BD_F_9620_2 (6) */
    { 0xb570600e6207d149ULL, 0x859b7fa2a2d1577fULL }, /* HD_A_5350_s + HD_A_5350 (4) */
    { 0xb5841bff8d04d31fULL, 0x859513a2a2cc344dULL }, /* HD_A_5370_s + HD_A_5370 (4) */
    { 0xb5b652d030d84b54ULL, 0xa0bd7cc71e1c88caULL }, /* HD_M_9410_s + HD_M_9410 (4) */
    { 0xb603562f15e52bc5ULL, 0x396e69fc26095983ULL }, /* BD_A_2570_2_s + BD_A_2570_2 (6) */
    { 0xb65b14d6a9cdb411ULL, 0xcc2147c736b5d517ULL }, /* HD_M_9350_s + HD_M_9350 (12) */
    { 0xb73e2e38d914a8f1ULL, 0x66d4336657ee4037ULL }, /* HD_F_9360_s + HD_F_9360 (2) */
    { 0xb79b55a16872d86eULL, 0x00bf87c367b1a46cULL }, /* HD_A_9500_s + HD_A_9500 (2) */
    { 0xb81850a02a5417bfULL, 0xd50b74c73bd4e62dULL }, /* HD_M_9280_s + HD_M_9280 (6) */
    { 0xb8b087965e7b2195ULL, 0x6f05f27f35135ad3ULL }, /* AM_M_9480_AM_F_9480_s + AM_M_9480_AM_F_9480 (4) */
    { 0xb957616e84dc5518ULL, 0x250bbd0e68504dc6ULL }, /* BD_A_9250_1_s + BD_A_9250_1 (6) */
    { 0xb9b3ae3584e4f1fdULL, 0xeb92e77308960c4bULL }, /* LG_F_9270_LG_M_9270_s + LG_F_9270_LG_M_9270 (2) */
    { 0xb9c378bb7bbf97c9ULL, 0x45d059ea642e39ffULL }, /* BD_F_9340_1_s + BD_F_9340_1 (8) */
    { 0xba3cd7976c384f2aULL, 0x983b0471227566f0ULL }, /* LG_M_2540_s + LG_M_2540 (6) */
    { 0xbac660e426d3aa41ULL, 0x345d79c6f7b1e0c7ULL }, /* BD_M_9230_BD_F_9230_1_s + BD_M_9230_BD_F_9230_1 (4) */
    { 0xbadd66d476b17e00ULL, 0xaafcec0117c8741eULL }, /* BD_F_5320_BD_M_5320_2_s + BD_F_5320_BD_M_5320_2 (4) */
    { 0xbb2e81b0ec951c35ULL, 0xa95babb7c7f857b3ULL }, /* LG_F_9610_LG_M_9610_s + LG_F_9610_LG_M_9610 (6) */
    { 0xbba5a80bd1c50529ULL, 0x076811c4e36ce91fULL }, /* BD_F_9220_BD_M_9220_1_s + BD_F_9220_BD_M_9220_1 (4) */
    { 0xbbbb04ec8b186531ULL, 0x810ec6def8cca077ULL }, /* LG_F_9470_LG_M_9470_s + LG_F_9470_LG_M_9470 (6) */
    { 0xbc1dbef2866341e9ULL, 0xac00cacd0380dadfULL }, /* AM_M_9710_AM_F_9710_s + AM_M_9710_AM_F_9710 (2) */
    { 0xbcc253fe4b22d325ULL, 0xd8635319798fb323ULL }, /* BD_F_9720_BD_M_9720_1_s + BD_F_9720_BD_M_9720_1 (6) */
    { 0xbd22735ac9ad35e5ULL, 0x419bdda3abded5e3ULL }, /* LG_M_8200_s + LG_M_8200 (10) */
    { 0xbdd5b94d3976baa1ULL, 0xd9a22a4d0e307b67ULL }, /* AM_A_9550_s + AM_A_9550 (4) */
    { 0xbe2ab9ecf9c2aa7cULL, 0xc6f75b3d3031f8a2ULL }, /* BD_F_9620_1_s + BD_F_9620_1 (8) */
    { 0xbe2b872f1a5f31c8ULL, 0x396e6afc26095b36ULL }, /* BD_A_2570_1_s + BD_A_2570_1 (4) */
    { 0xbfbcf746c2aad724ULL, 0xae3e5f4cf5972f1aULL }, /* AM_A_9210_s + AM_A_9210 (6) */
    { 0xc06a770e702f0355ULL, 0x430a40ff28f16893ULL }, /* BD_M_9480_BD_F_9480_s + BD_M_9480_BD_F_9480 (8) */
    { 0xc07bc69cee80994dULL, 0x421425857266671bULL }, /* LG_M_9309_LG_M_9300_s + LG_M_9309_LG_M_9300 (6) */
    { 0xc07fefc4b542825dULL, 0x1ef9e6b184cb26ebULL }, /* BD_F_9540_BD_M_9540_1_s + BD_F_9540_BD_M_9540_1 (16) */
    { 0xc0da7a89296b6ebaULL, 0x25945015dcad7dc0ULL }, /* BD_M_9260_2_s + BD_M_9260_2 (6) */
    { 0xc1d994ea6f06c9c7ULL, 0xa0c7eec71e259f05ULL }, /* HD_M_9460_s + HD_M_9460 (4) */
    { 0xc210b26b2a87949dULL, 0xaac48db0d4bb712bULL }, /* LG_F_9250_LG_M_9250_s + LG_F_9250_LG_M_9250 (10) */
    { 0xc2b1377b982142b5ULL, 0xf7afd8c362729a33ULL }, /* HD_A_9220_s + HD_A_9220 (2) */
    { 0xc2c3f36cc31c918bULL, 0xf7a96cc3626d7701ULL }, /* HD_A_9200_s + HD_A_9200 (2) */
    { 0xc2ee91e42b4db044ULL, 0x345d7ac6f7b1e27aULL }, /* BD_M_9230_BD_F_9230_2_s + BD_M_9230_BD_F_9230_2 (6) */
    { 0xc2f4d90bd5871a6cULL, 0x076812c4e36cead2ULL }, /* BD_F_9220_BD_M_9220_2_s + BD_F_9220_BD_M_9220_2 (4) */
    { 0xc3161f1793254f9dULL, 0x003edc3c61d5b42bULL }, /* LG_F_5370_LG_M_5370_s + LG_F_5370_LG_M_5370 (10) */
    { 0xc33784fe4e2b44a8ULL, 0xd8635419798fb4d6ULL }, /* BD_F_9720_BD_M_9720_2_s + BD_F_9720_BD_M_9720_2 (8) */
    { 0xc382b5d16606f34dULL, 0xc7fa0f99b16f311bULL }, /* LG_F_9630_LG_M_9630_s + LG_F_9630_LG_M_9630 (6) */
    { 0xc478a9bb8264d60cULL, 0x45d05aea642e3bb2ULL }, /* BD_F_9340_2_s + BD_F_9340_2 (6) */
    { 0xc551e682e2adb20dULL, 0xbbea5af4263f4fdbULL }, /* BD_A_9550_1_s + BD_A_9550_1 (10) */
    { 0xc55aa41c0a533db9ULL, 0x68dffcd1d2cb716fULL }, /* LG_F_9350_s + LG_F_9350 (4) */
    { 0xc5deb6408d5329c9ULL, 0x3ecec2e6ba797bffULL }, /* BD_M_2560_1_s + BD_M_2560_1 (6) */
    { 0xc65ef50820eeecacULL, 0xde2067c354177012ULL }, /* HD_A_9120_s + HD_A_9120 (2) */
    { 0xc6df7aefb6b0538eULL, 0x996b3fc71a58294cULL }, /* HD_M_9540_s + HD_M_9540 (2) */
    { 0xc740e0b704782244ULL, 0x886aaa772d99047aULL }, /* AM_F_9530_s + AM_F_9530 (2) */
    { 0xc7e14d358e5a23ecULL, 0x7732786660ee5852ULL }, /* HD_F_9521_s + HD_F_9521 (2) */
    { 0xc845d920f02e092bULL, 0xa52eb04cf05824e1ULL }, /* AM_A_9310_s + AM_A_9310 (4) */
    { 0xc8d32e181be3ffdfULL, 0xfa230eceec3d100dULL }, /* HD_M_0001_M_s + HD_M_0001_M (2) */
    { 0xc9550a3d12f9e3aaULL, 0x1c69c92c17456770ULL }, /* AM_M_9270_s + AM_M_9270 (2) */
    { 0xc9909e059f19c24bULL, 0x3f5039673370b7c1ULL }, /* HD_A_2920_s + HD_A_2920 (6) */
    { 0xc9a488ff4a159e6aULL, 0x5d526d681648a630ULL }, /* BD_M_6200_1_s + BD_M_6200_1 (2) */
    { 0xca5b00c4bb2de680ULL, 0x1ef9e7b184cb289eULL }, /* BD_F_9540_BD_M_9540_2_s + BD_F_9540_BD_M_9540_2 (2) */
    { 0xcac90dc9824307d3ULL, 0xc0cd619ad66d2df9ULL }, /* LG_M_9450_s + LG_M_9450 (6) */
    { 0xcc7a1115ae9a162fULL, 0x4033b800bbbebdddULL }, /* BD_M_9410_2_s + BD_M_9410_2 (3) */
    { 0xccddae5e4f967929ULL, 0xc07d3e3c05753d1fULL }, /* BD_M_9309_BD_M_9300_1_s + BD_M_9309_BD_M_9300_1 (6) */
    { 0xccec595ecaa7fae8ULL, 0x56d8585f80330316ULL }, /* BD_M_9440_1_L_s + BD_M_9440_1_L (6) */
    { 0xcd9a63066391b63aULL, 0xdb115b85c7bdf940ULL }, /* BD_A_9390_2_s + BD_A_9390_2 (6) */
    { 0xcdae2e0824b10f87ULL, 0xde2068c3541771c5ULL }, /* HD_A_9121_s + HD_A_9121 (2) */
    { 0xcead962ff268783aULL, 0xb27d32c72848eb40ULL }, /* HD_M_9610_s + HD_M_9610 (4) */
    { 0xcec259deda6a4a85ULL, 0x47c246be5e5b5d43ULL }, /* AM_M_9289_AM_M_9280_s + AM_M_9289_AM_M_9280 (2) */
    { 0xd006f782e952b9f0ULL, 0xbbea5bf4263f518eULL }, /* BD_A_9550_2_s + BD_A_9550_2 (6) */
    { 0xd093e74093f8680cULL, 0x3ecec3e6ba797db2ULL }, /* BD_M_2560_2_s + BD_M_2560_2 (2) */
    { 0xd0e899603e4495b1ULL, 0xbc1e9bedae01dcf7ULL }, /* BD_A_9210_2_s + BD_A_9210_2 (4) */
    { 0xd10e4c3eebad2139ULL, 0x0a69812c0ce228efULL }, /* AM_M_9460_s + AM_M_9460 (4) */
    { 0xd12474868ed4cacdULL, 0x63ca100764ecbc9bULL }, /* BD_M_9520_BD_F_9520_1_s + BD_M_9520_BD_F_9520_1 (12) */
    { 0xd295863594fdbcc7ULL, 0x7732796660ee5a05ULL }, /* HD_F_9520_s + HD_F_9520 (2) */
    { 0xd42cdf5e53588e6cULL, 0xc07d3f3c05753ed2ULL }, /* BD_M_9309_BD_M_9300_2_s + BD_M_9309_BD_M_9300_2 (10) */
    { 0xd4d00899bb8d0755ULL, 0x4fb2f9d1c4c3cc93ULL }, /* LG_F_9620_s + LG_F_9620 (4) */
    { 0xd5050fa3b13804baULL, 0xb7bdb29ad12e23c0ULL }, /* LG_M_9310_s + LG_M_9310 (4) */
    { 0xd511b35130519e49ULL, 0x6dee124fc91b5c7fULL }, /* BD_F_9260_BD_M_9260_1_s + BD_F_9260_BD_M_9260_1 (8) */
    { 0xd54cfa1f5aa9894dULL, 0xd96acb900cd5d71bULL }, /* HD_F_9341_HD_M_9341_s + HD_F_9341_HD_M_9341 (12) */
    { 0xd5b8fca96fb86841ULL, 0x12d477ce8ff66ec7ULL }, /* AM_F_9540_AM_M_9540_s + AM_F_9540_AM_M_9540 (2) */
    { 0xd5f20ec9d7a3afc4ULL, 0xd51c72c73be355faULL }, /* HD_M_9270_s + HD_M_9270 (2) */
    { 0xd645a2434061e33bULL, 0x1d0915672020f271ULL }, /* HD_A_2560_s + HD_A_2560 (2) */
    { 0xd6af3367040bbb21ULL, 0xbab181e9c9f207e7ULL }, /* LG_F_9430_LG_M_9430_s + LG_F_9430_LG_M_9430 (2) */
    { 0xd6f5f8f9e10ae391ULL, 0xa0ce7ac71e2af897ULL }, /* HD_M_9440_s + HD_M_9440 (6) */
    { 0xd72972f3acd86e59ULL, 0xbabb38ff9be42fcfULL }, /* LG_M_9480_LG_F_9480_s + LG_M_9480_LG_F_9480 (20) */
    { 0xd75b8ffec6c23c96ULL, 0xc9e39c9adbb191c4ULL }, /* LG_M_9530_s + LG_M_9530 (6) */
    { 0xd87285869294f6b0ULL, 0x63ca110764ecbe4eULL }, /* BD_M_9520_BD_F_9520_2_s + BD_M_9520_BD_F_9520_2 (6) */
    { 0xd888282c085c3924ULL, 0x6fcf5e665d1bc11aULL }, /* HD_F_9460_s + HD_F_9460 (12) */
    { 0xd9002e095b3166e9ULL, 0x13a99dde34cf77dfULL }, /* BD_F_9520_1_s + BD_F_9520_1 (12) */
    { 0xda2cdd893792d2f3ULL, 0x25945315dcad82d9ULL }, /* BD_M_9260_1_s + BD_M_9260_1 (4) */
    { 0xda96d7fa061ad4f5ULL, 0xe4ac6851fb18e073ULL }, /* LG_M_9620_LG_F_9620_s + LG_M_9620_LG_F_9620 (4) */
    { 0xdb9dca6044e9d3f4ULL, 0xbc1e9cedae01deaaULL }, /* BD_A_9210_1_s + BD_A_9210_1 (4) */
    { 0xdbf6bd85d2f944d1ULL, 0x09631719200afad7ULL }, /* BD_M_9420_1_s + BD_M_9420_1 (6) */
    { 0xdcfd38dedaf63bd3ULL, 0xa513a04cf04141f9ULL }, /* AM_A_9390_s + AM_A_9390 (6) */
    { 0xde709d7070b3ea21ULL, 0xae4f5d4cf5a59ee7ULL }, /* AM_A_9240_s + AM_A_9240 (4) */
    { 0xded269d28c7ec925ULL, 0xdabfd391db70b923ULL }, /* LG_F_9290_LG_M_9290_s + LG_F_9290_LG_M_9290 (6) */
    { 0xdf1f3402252b67daULL, 0xf5209409aec05020ULL }, /* BD_M_9400_2_s + BD_M_9400_2 (8) */
    { 0xdf30d021a6a31882ULL, 0x5c3e0236087abb38ULL }, /* BD_M_9372_2_s + BD_M_9372_2 (2) */
    { 0xdfac66c17096b421ULL, 0x9a8a4a23afa558e7ULL }, /* LG_F_1000_LG_M_1000_s + LG_F_1000_LG_M_1000 (6) */
    { 0xdfc6e45136f6dc8cULL, 0x6dee134fc91b5e32ULL }, /* BD_F_9260_BD_M_9260_2_s + BD_F_9260_BD_M_9260_2 (8) */
    { 0xdfef24dedb4108d2ULL, 0x9a7bf077380ab2c8ULL }, /* AM_F_9370_s + AM_F_9370 (2) */
    { 0xe1110a0635df6b2bULL, 0x66bfaf6657dcb6e1ULL }, /* HD_F_9300_s + HD_F_9300 (12) */
    { 0xe15d4df912ca71caULL, 0x9a8661773813c750ULL }, /* AM_F_9341_s + AM_F_9341 (4) */
    { 0xe1dc50364415dbf7ULL, 0xce955114048166f5ULL }, /* AM_A_2870_s + AM_A_2870 (2) */
    { 0xe2031122f770a2b4ULL, 0x4d68967a15ae126aULL }, /* LG_M_5370_s + LG_M_5370 (12) */
    { 0xe21cebff57835ee3ULL, 0x5d5270681648ab49ULL }, /* BD_M_6200_2_s + BD_M_6200_2 (2) */
    { 0xe27191d8f670dd3dULL, 0xc0d3ed9ad672878bULL }, /* LG_M_9470_s + LG_M_9470 (6) */
    { 0xe32203b06fa9daf6ULL, 0x25cdc06725200a64ULL }, /* HD_A_2660_s + HD_A_2660 (4) */
    { 0xe3b45f0961d4f22cULL, 0x13a99ede34cf7992ULL }, /* BD_F_9520_2_s + BD_F_9520_2 (4) */
    { 0xe47daf3b4ebfc0d2ULL, 0x38ddde7918e2aac8ULL }, /* AM_M_2550_s + AM_M_2550 (4) */
    { 0xe52ebf73cc670751ULL, 0xcf2e9999ae897957ULL }, /* HD_A_4160_s + HD_A_4160 (4) */
    { 0xe5c1b101c702605cULL, 0x127f5dc371de3d42ULL }, /* HD_A_9700_s + HD_A_9700 (6) */
    { 0xe6aaee85d99cd014ULL, 0x09631819200afc8aULL }, /* BD_M_9420_2_s + BD_M_9420_2 (6) */
    { 0xe6ecc60671b91a73ULL, 0xdb115e85c7bdfe59ULL }, /* BD_A_9390_1_s + BD_A_9390_1 (6) */
    { 0xe79840d56f4c603bULL, 0x5dc7379cde4c2771ULL }, /* BD_M_2540_s + BD_M_2540 (4) */
    { 0xe7c7f0515bac2932ULL, 0xaf03999acc385868ULL }, /* LG_M_9260_s + LG_M_9260 (8) */
    { 0xe8175ddedfbb1c6dULL, 0x9a7bf177380ab47bULL }, /* AM_F_9371_s + AM_F_9371 (2) */
    { 0xe86dca57d4a36db7ULL, 0xda67649ae4d1a4b5ULL }, /* LG_M_9700_s + LG_M_9700 (4) */
    { 0xe8808ce2d21db065ULL, 0x4b6f7ea36f194c63ULL }, /* BD_A_9730_1_s + BD_A_9730_1 (2) */
    { 0xe986d52d66d9a589ULL, 0x954d342d0b1293bfULL }, /* BD_M_9360_2_s + BD_M_9360_2 (4) */
    { 0xe9c956f1fbfd8dd2ULL, 0xee9996d91900afc8ULL }, /* BD_F_9360_2_s + BD_F_9360_2 (6) */
    { 0xea04bb7f9cf6ce7dULL, 0xf2fb3b2f5bc514cbULL }, /* AM_F_9440_AM_M_9440_s + AM_F_9440_AM_M_9440 (4) */
    { 0xeb2ecbf331bc2518ULL, 0xc0de5f9ad67b9dc6ULL }, /* LG_M_9420_s + LG_M_9420 (4) */
    { 0xeb6c87339b4319edULL, 0xb071cd90214bf5fbULL }, /* HD_A_9509_HD_A_9500_s + HD_A_9509_HD_A_9500 (2) */
    { 0xebe211a986a3eac5ULL, 0x553c3a4eb9010083ULL }, /* BD_F_9440_BD_M_9440_1_s + BD_F_9440_BD_M_9440_1 (14) */
    { 0xec1266f9196f8745ULL, 0x9a8662773813c903ULL }, /* AM_F_9340_s + AM_F_9340 (4) */
    { 0xec5b4c207355f8deULL, 0x66ca216657e5cd1cULL }, /* HD_F_9350_s + HD_F_9350 (10) */
    { 0xecf28467c55b7f91ULL, 0x32bfcfcc4a063497ULL }, /* BD_M_5320_2_s + BD_M_5320_2 (2) */
    { 0xee0769db4dd94876ULL, 0xc564f10f5dab3be4ULL }, /* BD_M_9270_1_s + BD_M_9270_1 (2) */
    { 0xef745e2fdaf48d52ULL, 0x66d0ab6657eb2348ULL }, /* HD_F_9372_s + HD_F_9372 (4) */
    { 0xf0c56fdf37cdffe5ULL, 0xdfd1a25f97ec8fe3ULL }, /* AM_M_9620_AM_F_9620_s + AM_M_9620_AM_F_9620 (8) */
    { 0xf25846321af4b16cULL, 0x1364ac2c120fa9d2ULL }, /* AM_M_9360_s + AM_M_9360 (2) */
    { 0xf25cbde2d80afde8ULL, 0x4b6f7fa36f194e16ULL }, /* BD_A_9730_2_s + BD_A_9730_2 (4) */
    { 0xf29e5d86cf418551ULL, 0xe65ad75b3b8dc757ULL }, /* BD_F_9740_BD_M_9740_1_s + BD_F_9740_BD_M_9740_1 (4) */
    { 0xf32e33370c3b38dfULL, 0x1b9598c37722a10dULL }, /* HD_A_9600_s + HD_A_9600 (4) */
    { 0xf3cb71be8a3c668dULL, 0xb7c8449ad137705bULL }, /* LG_M_9320_s + LG_M_9320 (4) */
    { 0xf40a42a98b1df0c8ULL, 0x553c3b4eb9010236ULL }, /* BD_F_9440_BD_M_9440_2_s + BD_F_9440_BD_M_9440_2 (10) */
    { 0xf4296760af0e1d8aULL, 0x7fdc146665d68f10ULL }, /* HD_F_9620_s + HD_F_9620 (20) */
    { 0xf441b567c91d94d4ULL, 0x32bfd0cc4a06364aULL }, /* BD_M_5320_1_s + BD_M_5320_1 (6) */
    { 0xf4c212c504d548adULL, 0x086daa148a0a03bbULL }, /* LG_F_5320_LG_M_5320_s + LG_F_5320_LG_M_5320 (2) */
    { 0xf5c869c420a79dd3ULL, 0x66a49f6657c5d3f9ULL }, /* HD_F_9380_s + HD_F_9380 (4) */
    { 0xf757dc6f468bc0d1ULL, 0x6eea1a13ce3516d7ULL }, /* AM_A_2560_s + AM_A_2560 (2) */
    { 0xf7a95321b4110f5bULL, 0x5c3e0536087ac051ULL }, /* BD_M_9372_1_s + BD_M_9372_1 (2) */
    { 0xf7c0365a52381581ULL, 0x7dade5b2b832dc07ULL }, /* HD_F_9430_HD_M_9430_s + HD_F_9430_HD_M_9430 (4) */
    { 0xf8427380c35d1791ULL, 0x74c55ad5df860c97ULL }, /* BD_F_9530_2_s + BD_F_9530_2 (6) */
    { 0xf8707f133c9925f9ULL, 0x30d8ef7eae2c1cafULL }, /* BD_M_9389_BD_M_9380_1_s + BD_M_9389_BD_M_9380_1 (8) */
    { 0xf87197023352cc13ULL, 0xf5209709aec05539ULL }, /* BD_M_9400_1_s + BD_M_9400_1 (4) */
    { 0xf9a25ec600255ce1ULL, 0xdb8157e7541550a7ULL }, /* BD_A_9470_2_s + BD_A_9470_2 (8) */
    { 0xfcb02e6745ede3c1ULL, 0xda6df09ae4d6fe47ULL }, /* LG_M_9720_s + LG_M_9720 (8) */
    { 0xfce46c60cdb06bc4ULL, 0xaf0a259acc3db1faULL }, /* LG_M_9240_s + LG_M_9240 (8) */
    { 0xfd528e86d5e51094ULL, 0xe65ad85b3b8dc90aULL }, /* BD_F_9740_BD_M_9740_2_s + BD_F_9740_BD_M_9740_2 (2) */
    { 0xfd6d280c4aa168b3ULL, 0x0a54fd2c0cd09f99ULL }, /* AM_M_9400_s + AM_M_9400 (10) */
    { 0xfdf6fab5d130d2aeULL, 0x8d02b76fec2e11acULL }, /* HD_A_3322_s + HD_A_3322 (2) */
    { 0xfea95f5e1499ef69ULL, 0x7c9fea1866014c5fULL }, /* HD_M_9620_HD_F_9620_s + HD_M_9620_HD_F_9620 (20) */
    { 0xff91a480c71f2cd4ULL, 0x74c55bd5df860e4aULL }, /* BD_F_9530_1_s + BD_F_9530_1 (12) */
    { 0xffa0bd9c057005ddULL, 0x83ef99eede2c7e6bULL }, /* BD_F_9230_1_s + BD_F_9230_1 (2) */
    { 0xffbfb013405b3b3cULL, 0x30d8f07eae2c1e62ULL }, /* BD_M_9389_BD_M_9380_2_s + BD_M_9389_BD_M_9380_2 (10) */
    { 0xffc52a7253366a7aULL, 0xbe4ff522130c9080ULL }, /* BD_M_9450_1_s + BD_M_9450_1 (6) */
};
static const u64 k_spec_names[] = {
    0x00178fc6032dce64ULL, /* bd_a_9470_1_s */
    0x001c55e4c9de9cbaULL, /* lg_m_1000_s */
    0x012ee3088b73c9d7ULL, /* am_f_9320_s */
    0x0251a82fe613a8d0ULL, /* hd_f_9370_s */
    0x025aebf96526d296ULL, /* hd_f_9480_s */
    0x0269092d7cf08daeULL, /* bd_m_9371_1_s */
    0x0272b4cdf9382a04ULL, /* lg_m_9341_s */
    0x02add798757032e5ULL, /* hd_a_9570_s */
    0x032ab6c64e2a843cULL, /* am_m_9372_l_s */
    0x0342b74ca7a33af1ULL, /* lg_m_5320_s */
    0x037a464384ddc9ceULL, /* bd_f_9370_1_s */
    0x03f4d9f20add192bULL, /* bd_f_9360_1_s */
    0x048c5da97ac49a4aULL, /* wp_a_0805_s */
    0x05361495afc93576ULL, /* wp_a_0210_s */
    0x05d48469f7f32106ULL, /* am_f_9420_s */
    0x068730cd07cbc676ULL, /* bd_a_2870_2_s */
    0x06a53dafe754516eULL, /* wp_a_0221_s */
    0x0759ecdb5c00e30fULL, /* bd_m_9270_2_s */
    0x07738ae011a8e879ULL, /* wp_a_1520_s */
    0x07a3be6fd1bd475bULL, /* wp_a_1801_s */
    0x07dde9cc37875d84ULL, /* hr_m_0002_mspot_s */
    0x084daa418db0bbf6ULL, /* am_m_9300_s */
    0x097bce9c0b5b6a00ULL, /* bd_f_9230_2_s */
    0x097ca7119f625055ULL, /* bd_f_2540_bd_m_2540_s */
    0x099fe12fe9d418abULL, /* hd_f_9371_s */
    0x09a4eb186711c351ULL, /* lg_f_9740_lg_m_9740_s */
    0x09c0cdcdfcf8637fULL, /* lg_m_9340_s */
    0x0a8b90c8200b979eULL, /* wp_a_1518_s */
    0x0ab96515dd2c3db5ULL, /* hr_m_0011_short_s */
    0x0ad0793dcfa9e49bULL, /* am_a_9220_s */
    0x0afc436c92bb7553ULL, /* hr_f_0007_tail_s */
    0x0b4a7563cdf066c2ULL, /* hd_a_9210_s */
    0x0c67f0ff9065623fULL, /* lg_m_6200_s */
    0x0c7672e2ab643f86ULL, /* am_a_9490_s */
    0x0cf5e2fc7d0969b1ULL, /* bd_m_9610_2_s */
    0x0d80f228f1cf1cf5ULL, /* bd_m_9460_1_s */
    0x0dca8116374a6d6fULL, /* hr_m_0007_s */
    0x0e1f189c5b98cd8dULL, /* bd_a_9240_1_s */
    0x0e2cade66276d403ULL, /* lg_m_9560_s */
    0x0ec2bbe0156afdbcULL, /* wp_a_1523_s */
    0x0f114d95b5b4dd91ULL, /* wp_a_0211_s */
    0x0f2144b5dade59acULL, /* hd_a_3320_s */
    0x0f4176a98169afc5ULL, /* wp_a_0804_s */
    0x0fdb94a9744a5a6cULL, /* wp_a_0740_s */
    0x106d3e131750c0f8ULL, /* lg_f_9340_s */
    0x108056afed3fc329ULL, /* wp_a_0220_s */
    0x114d6bc6a68be261ULL, /* hd_m_2540_s */
    0x1159e20443058ac6ULL, /* lg_f_9360_s */
    0x11adbf029c690afbULL, /* bd_a_1000_s */
    0x1254640ca084b4b3ULL, /* lg_f_9420_s */
    0x12c8f95affd447f1ULL, /* hd_f_9720_hd_m_9720_s */
    0x12dfb8d7ef64539bULL, /* hd_m_9510_s */
    0x13b65b69187d1989ULL, /* hd_a_9310_s */
    0x14fca93193eb3969ULL, /* hd_m_9349_hd_m_9340_s */
    0x153fa9c826aefa19ULL, /* wp_a_1519_s */
    0x154cdb5bb1134d1bULL, /* am_a_1000_s */
    0x15b42e42016e457aULL, /* wp_a_0702_longspear_s */
    0x15dbafc08f4098aaULL, /* lg_m_9440_s */
    0x15e50fe83b2894f2ULL, /* lg_m_9390_s */
    0x161544ace101b871ULL, /* lg_f_9570_lg_m_9570_s */
    0x16607e95b976f2d4ULL, /* wp_a_0212_s */
    0x16707db5dea07c87ULL, /* hd_a_3321_s */
    0x1675ec5224461240ULL, /* lg_m_9590_s */
    0x171b03ad5f9455baULL, /* wp_a_1543_s */
    0x1797c0ff6bea0d41ULL, /* am_m_9520_am_f_9520_s */
    0x17ab13fc83aea7f4ULL, /* bd_m_9610_1_s */
    0x18362328f8745b38ULL, /* bd_m_9460_2_s */
    0x189df4e01b56a5d7ULL, /* wp_a_1522_s */
    0x18d4299c623dd570ULL, /* bd_a_9240_2_s */
    0x19178d72615dceb3ULL, /* bd_m_9450_2_s */
    0x1a4857131d3c32b3ULL, /* lg_f_9341_s */
    0x1a97ea1cc4eb81a9ULL, /* hd_m_9480_hd_f_9480_s */
    0x1ae28c2d8a603787ULL, /* bd_m_9371_2_s */
    0x1b1c082c9fd191feULL, /* hd_f_9530_s */
    0x1b8e3a14a8c3ed3dULL, /* hd_f_9340_hd_m_9340_s */
    0x1bfedd5b6819ebbeULL, /* hd_a_4150_s */
    0x1d0d8a27245f6e46ULL, /* am_m_9450_s */
    0x1d4b128a7d2b1341ULL, /* lg_m_9210_s */
    0x1d799da08b7361e5ULL, /* am_f_9400_am_m_9400_s */
    0x1da5c94393bd5527ULL, /* bd_f_9370_2_s */
    0x1e889795bdf0d00fULL, /* wp_a_0213_s */
    0x1edebea35346fe2dULL, /* lg_f_8200_lg_m_8200_s */
    0x1f431cad640e32f5ULL, /* wp_a_1542_s */
    0x1f719c10fce44af5ULL, /* hd_f_2550_hd_m_2550_s */
    0x1fd9b3cd15f3610fULL, /* bd_a_2870_1_s */
    0x20948ec4770bbbd3ULL, /* wp_a_1210_s */
    0x2167b7434ee88405ULL, /* bd_f_9380_1_s */
    0x21a5c874f5c36b19ULL, /* hd_f_9270_hd_m_9270_s */
    0x22171311be83ed56ULL, /* bd_f_9460_1_s */
    0x22228594d914d906ULL, /* bd_m_9340_1_s */
    0x2264ee2c54aca3a7ULL, /* lg_m_9490_s */
    0x228df521bae2550aULL, /* hd_a_9290_s */
    0x22e69c169b88d731ULL, /* lg_f_9200_lg_m_9200_s */
    0x234831f5d479712dULL, /* lg_m_9540_s */
    0x239baa23155b469bULL, /* hd_f_9450_s */
    0x23c5fe569e517ee9ULL, /* bd_f_9710_2_s */
    0x23f7d18ba4b7c318ULL, /* hd_a_9490_s */
    0x254fbaf3a67ccd46ULL, /* bd_m_9530_2_s */
    0x25d0f8b8e70664f6ULL, /* wp_a_0720_s */
    0x264559a796e7a569ULL, /* bd_m_9289_bd_m_9280_1_s */
    0x265d70d9bb0befd9ULL, /* bd_m_9430_2_s */
    0x26a3b3a95466e91cULL, /* lg_m_2560_s */
    0x26ed8f3b6adff809ULL, /* bd_f_5370_bd_m_5370_1_s */
    0x275d0a34a37249bbULL, /* lg_m_9740_s */
    0x27f36b2ec87a2ac9ULL, /* bd_m_9521_bd_f_9521_1_s */
    0x28701895dae47703ULL, /* hd_m_9590_s */
    0x287e26fd91fb5db1ULL, /* am_f_9230_s */
    0x287f285c210a95a9ULL, /* lg_f_9710_s */
    0x28aa17677cf94640ULL, /* am_a_9250_s */
    0x298fe84353628a08ULL, /* bd_f_9380_2_s */
    0x29f84dad6ab37138ULL, /* wp_a_1541_s */
    0x2a2b48aa40cc6905ULL, /* hd_f_9740_hd_m_9740_s */
    0x2a2ceaad49c45afaULL, /* wp_a_0452_s */
    0x2a7a5a94535a8743ULL, /* wp_a_0510_s */
    0x2a9fac1b3e38aacdULL, /* bd_f_2520_bd_m_2520_1_s */
    0x2abfc28b63cc2c4dULL, /* bd_f_9510_bd_m_9510_1_s */
    0x2b1fff78c65fda6dULL, /* bd_m_5370_1_s */
    0x2bb1338d7d6b3707ULL, /* hd_a_9240_s */
    0x2bdba6d5e8f80709ULL, /* am_f_9380_s */
    0x2c066c100fc66c08ULL, /* lg_m_9510_s */
    0x2c0ffe3d0b4a17fcULL, /* wp_a_0111_s */
    0x2cb0993ae1b59243ULL, /* hr_f_0010_tail_s */
    0x2d2ba462c9a286b1ULL, /* hd_f_9600_hd_a_9600_s */
    0x2d5ee0ab82d084eaULL, /* bd_a_9700_1_s */
    0x2e3bc03b6ea05a4cULL, /* bd_f_5370_bd_m_5370_2_s */
    0x2e3f9b9f3a90518dULL, /* lg_f_9730_lg_m_9730_s */
    0x2e7a2f56a4f50a2cULL, /* bd_f_9710_1_s */
    0x2f6c2cc835903872ULL, /* wp_a_1514_s */
    0x2fa39eee8e58abf1ULL, /* am_m_9309_am_m_9300_s */
    0x2fac31b8ecf20d11ULL, /* wp_a_0721_s */
    0x303ee55804b18a26ULL, /* bd_f_9529_bd_f_9521_1_s */
    0x304049d122af1cadULL, /* bd_m_9320_2_s */
    0x306d66ad6dbbb9f3ULL, /* wp_a_1540_s */
    0x3082ab95b7d2ce96ULL, /* bd_f_9320_2_s */
    0x30af4d2266e55155ULL, /* lg_f_9510_lg_m_9510_s */
    0x30f98aa79d8b30acULL, /* bd_m_9289_bd_m_9280_2_s */
    0x3101595172e3d3c9ULL, /* bd_m_9620_bd_f_9620_1_s */
    0x310822364e8df078ULL, /* lg_f_9450_s */
    0x3112a1d9c1b12e1cULL, /* bd_m_9430_1_s */
    0x311b3ad3247cf2a9ULL, /* wp_a_0710_s */
    0x31ae32f8d4b95e93ULL, /* hd_a_9730_s */
    0x31edbd1b41f8d6b0ULL, /* bd_f_2520_bd_m_2520_2_s */
    0x3202724867f345e9ULL, /* lg_m_9290_s */
    0x320dd38b678c5830ULL, /* bd_f_9510_bd_m_9510_2_s */
    0x324baa857930856eULL, /* am_m_9380_s */
    0x3285373d0e529717ULL, /* wp_a_0110_s */
    0x32a89c2ecf1f690cULL, /* bd_m_9521_bd_f_9521_2_s */
    0x331835df222fbff1ULL, /* bd_f_9350_2_s */
    0x3445bea0b45c4a35ULL, /* hd_m_9520_hd_f_9520_s */
    0x348514b815176e27ULL, /* lg_m_9379_lg_m_9372_s */
    0x34c029c89b5cf97fULL, /* hd_f_9230_s */
    0x35049e9740b6b968ULL, /* hd_m_6200_s */
    0x35d51078cd04e250ULL, /* bd_m_5370_2_s */
    0x35e165c83898b78dULL, /* wp_a_1515_s */
    0x36facc2cc1ec7a6dULL, /* lg_f_9530_s */
    0x36fb62b8f0b42254ULL, /* wp_a_0722_s */
    0x376ece1780550655ULL, /* hd_f_9320_s */
    0x378f5ad12670fb90ULL, /* bd_m_9320_1_s */
    0x38421f4ac1647de5ULL, /* am_f_5370_am_m_5370_s */
    0x3868b404d2878351ULL, /* lg_f_9260_lg_m_9260_s */
    0x390f2dd9a13470a9ULL, /* wp_a_0291_s */
    0x3914359b16bbdce2ULL, /* hd_a_9470_s */
    0x398585d3191eb301ULL, /* lg_f_2540_lg_m_2540_s */
    0x39a434ad52004d78ULL, /* wp_a_0450_s */
    0x3a2307169b5ac021ULL, /* lg_f_9280_lg_m_9280_s */
    0x3a474794029eb3dbULL, /* hd_m_2520_s */
    0x3a53327d4a0a8563ULL, /* wp_a_0240_s */
    0x3a6766df25f1d534ULL, /* bd_f_9350_1_s */
    0x3a6b3fe255698676ULL, /* lg_m_2870_s */
    0x3a9843a999f8751cULL, /* wp_a_0803_s */
    0x3af90c83c301c9e8ULL, /* wp_a_0920_s */
    0x3b229fad746105ceULL, /* wp_a_1547_s */
    0x3b302e290e6c0975ULL, /* lg_m_9610_s */
    0x3b3c8ece9967d1d9ULL, /* am_m_9521_am_f_9521_s */
    0x3b42ea1a3967584bULL, /* lg_m_9630_s */
    0x3b699611ccab87efULL, /* bd_f_9460_2_s */
    0x3bb68a517989120cULL, /* bd_m_9620_bd_f_9620_2_s */
    0x3d6ac82927f20be3ULL, /* am_m_9350_s */
    0x3d90480314bdf70dULL, /* hd_f_2520_hd_m_2520_s */
    0x3dc93df3b3ec771fULL, /* bd_m_9530_1_s */
    0x3e010894e965f8dfULL, /* bd_m_9340_2_s */
    0x3e0866e65837172dULL, /* lg_f_9720_lg_m_9720_s */
    0x3e5fd05f72c503beULL, /* bd_m_9440_2_s */
    0x3ec3eacca19b760aULL, /* am_f_9450_l_s */
    0x3f136ab44573f534ULL, /* am_m_9720_s */
    0x3fa9dad71389033aULL, /* wp_a_1532_s */
    0x3fb82cbabe695df1ULL, /* am_m_9280_s */
    0x409676c83f3dbf70ULL, /* wp_a_1516_s */
    0x4175484cc2aaa840ULL, /* hd_f_9420_s */
    0x418bfeff4d1b6ae3ULL, /* wp_a_0608_s */
    0x420bc8f8d2f4ec4dULL, /* lg_m_9520_lg_f_9520_s */
    0x42472583c6c20363ULL, /* wp_a_0921_s */
    0x4277658d42182199ULL, /* am_m_9349_am_m_9340_s */
    0x42c07ca99e7288b7ULL, /* wp_a_0802_s */
    0x431b5187baf21bd1ULL, /* lg_f_9240_lg_m_9240_s */
    0x434ab8ad78dae309ULL, /* wp_a_1546_s */
    0x437f4dad57ebbf33ULL, /* wp_a_0451_s */
    0x43af580fa9d7a921ULL, /* am_f_9270_am_m_9270_s */
    0x43f3a9e033e38f66ULL, /* wp_a_1529_s */
    0x45914ad826c59491ULL, /* bd_f_9450_1_s */
    0x45a48f7bb424ab59ULL, /* lg_f_9700_lg_m_9700_s */
    0x45d743ab903e4563ULL, /* bd_a_9700_2_s */
    0x460886a1781bd1c1ULL, /* hd_f_2540_hd_m_2540_s */
    0x466d96b72b6d7576ULL, /* am_a_9700_s */
    0x46a6a127fc2001c5ULL, /* bd_m_9710_bd_f_9710_1_s */
    0x477ae0bd57a07bf0ULL, /* am_f_9480_s */
    0x47e4afc842fe2f4bULL, /* wp_a_1517_s */
    0x47f7ee57daaf793bULL, /* lg_m_9270_s */
    0x489ca85e53a4d460ULL, /* lg_m_9730_s */
    0x49c820f0c142ede4ULL, /* am_f_9350_s */
    0x49d52e95c5fa692fULL, /* bd_f_9320_1_s */
    0x4a6a68581391157fULL, /* bd_f_9529_bd_f_9521_2_s */
    0x4a75371a99c1ddc1ULL, /* bd_m_9370_1_s */
    0x4a98e9ad7c9b454cULL, /* wp_a_1545_s */
    0x4b42e2e037a5b241ULL, /* wp_a_1528_s */
    0x4c4b6deadc743eefULL, /* lg_m_9410_s */
    0x4c662b815c5a7dcaULL, /* bd_m_9350_2_s */
    0x4ca05252b95dc251ULL, /* bd_f_9280_bd_m_9280_1_s */
    0x4ce07bd82a87a9d4ULL, /* bd_f_9450_2_s */
    0x4cfc23eff0ba282aULL, /* hd_f_9529_hd_f_9521_s */
    0x4d1a2eba066cb552ULL, /* bd_a_9560_1_s */
    0x4d50a0bbb41a9972ULL, /* bd_m_9380_1_s */
    0x4dadb1d07fa1e601ULL, /* bd_f_9610_bd_m_9610_1_s */
    0x4dc3b6fe5a0e6b2eULL, /* hd_m_9720_s */
    0x4eced228009a07c8ULL, /* bd_m_9710_bd_f_9710_2_s */
    0x4f37222ff41683f6ULL, /* bd_f_9521_1_s */
    0x500dc8c84779bf86ULL, /* wp_a_1510_s */
    0x50ab40c950b6015aULL, /* bd_a_9630_1_s */
    0x50e7266104f0b1ddULL, /* am_f_9450_s */
    0x51b7d8d168e40329ULL, /* am_a_9730_s */
    0x5248c85ffec23bb5ULL, /* lg_f_9480_s */
    0x528724d71ea81eb8ULL, /* wp_a_1530_s */
    0x529d681a9e3be3c4ULL, /* bd_m_9370_2_s */
    0x52f7de2ff6cc9c68ULL, /* lg_f_9230_s */
    0x5321d53b52b9a797ULL, /* am_a_9570_s */
    0x53305fc5717bb10dULL, /* hd_f_9510_hd_m_9510_s */
    0x53bc7bada1b43c61ULL, /* hd_a_9390_s */
    0x541b307218ddf7aeULL, /* lg_m_9220_s */
    0x54acd536e1489895ULL, /* lg_f_9560_lg_m_9560_s */
    0x54d44fc509f770f6ULL, /* lg_m_9350_s */
    0x554e22ad83409127ULL, /* wp_a_1544_s */
    0x557398d886d9ac75ULL, /* hd_m_9400_s */
    0x558654c9b1d4fb4bULL, /* hd_m_9420_s */
    0x55ba97ac76a67ef9ULL, /* bd_m_9341_1_s */
    0x55d5e2d0841bec04ULL, /* bd_f_9610_bd_m_9610_2_s */
    0x562ae6d10acf19d9ULL, /* hd_m_9389_hd_m_9380_s */
    0x562c0ed02acc0970ULL, /* hd_m_9340_s */
    0x5668fbabf853167dULL, /* am_m_9230_am_f_9230_s */
    0x56d8535f8032fa97ULL, /* bd_m_9440_1_s */
    0x57548352c0014d94ULL, /* bd_f_9280_bd_m_9280_2_s */
    0x5835c58f0597840aULL, /* wp_a_0269_s */
    0x587527f0b6b3ac0aULL, /* bd_m_9379_bd_m_9372_2_s */
    0x58fc3dd721b06773ULL, /* wp_a_1531_s */
    0x58fd4770f2ed4ea7ULL, /* bd_a_9600_fur_s */
    0x593f4fdd6f7464daULL, /* lg_m_9570_s */
    0x595304664f7d570cULL, /* am_f_9710_s */
    0x5975809273d455ddULL, /* lg_m_9389_lg_m_9380_s */
    0x5a191d6065847ec1ULL, /* lg_f_9500_lg_m_9500_s */
    0x5a8a26467db32021ULL, /* hd_a_2870_s */
    0x5ac201c84e1d5861ULL, /* wp_a_1511_s */
    0x5b2e1e5e74d7096dULL, /* bd_f_2550_bd_m_2550_s */
    0x5b86e5bdb2d13e80ULL, /* hd_m_2550_s */
    0x5b93f0a6fa3ef968ULL, /* wp_a_0230_s */
    0x5c7e7a322cb6db81ULL, /* hd_m_9710_hd_f_9710_s */
    0x5c9ea9b30fc68921ULL, /* hd_f_9440_hd_m_9440_s */
    0x5cf71d3409681d7aULL, /* wp_a_0104_s */
    0x5d09c8ac7a68943cULL, /* bd_m_9341_2_s */
    0x5d7a47d02e8c794bULL, /* hd_m_9341_s */
    0x5ea13d4f17f44785ULL, /* am_a_9200_s */
    0x5eab8652d8267fe8ULL, /* am_m_9320_s */
    0x5eac960ab6e9bf51ULL, /* lg_f_9372_s */
    0x5ed60dbf55ffa62aULL, /* am_f_9521_s */
    0x5f38c79c711ed776ULL, /* hd_f_9710_s */
    0x5f84de8f09597085ULL, /* wp_a_0268_s */
    0x601e91df476dfea9ULL, /* lg_m_9360_s */
    0x6044befa4ac632baULL, /* wp_a_1002_s */
    0x604ed3e664fcb889ULL, /* bd_a_9200_1_s */
    0x61460d76560956a9ULL, /* hd_m_9521_hd_f_9521_s */
    0x6226ebb480dc9400ULL, /* am_a_8200_s */
    0x62df1d15f5da9cd5ULL, /* bd_a_9509_bd_a_9500_1_s */
    0x62fc082f8959bf62ULL, /* wp_a_1103_s */
    0x63b12e0ec31b9cc2ULL, /* lg_f_9529_lg_f_9521_s */
    0x63b176d72855b34eULL, /* wp_a_1536_s */
    0x63ca919020287584ULL, /* hd_a_9560_s */
    0x63d3b2f08633eb22ULL, /* bd_m_9740_1_s */
    0x63e83dd6d2982095ULL, /* bd_a_8200_2_s */
    0x63ec826ab0b90576ULL, /* am_m_6200_s */
    0x63ed5bcaed26e4fdULL, /* bd_m_9510_1_s */
    0x644555ce9c04b58cULL, /* wp_a_1509_s */
    0x64ae31f63f671b35ULL, /* hd_a_9101_s */
    0x652036340de3adb5ULL, /* wp_a_0105_s */
    0x65c923bbc188904bULL, /* bd_m_9380_2_s */
    0x65fab7c4a421443aULL, /* bd_f_9420_1_s */
    0x6670b97e9fa04cadULL, /* lg_m_9710_lg_f_9710_s */
    0x66928e816b3b85c3ULL, /* bd_m_9350_1_s */
    0x66b07ce937aeb685ULL, /* hd_m_9530_s */
    0x66e2c87b4f3df9adULL, /* wp_a_0916_s */
    0x6745b1ba154c40abULL, /* bd_a_9560_2_s */
    0x679d04e668bd1accULL, /* bd_a_9200_2_s */
    0x68212c1e315eb21dULL, /* am_m_9420_s */
    0x686cd7fa4f400ff5ULL, /* wp_a_1003_s */
    0x6889a530023e1e8fULL, /* bd_f_9521_2_s */
    0x69021dfc11dcc69dULL, /* lg_m_9349_lg_m_9340_s */
    0x69378c818ae203e0ULL, /* lg_m_9200_s */
    0x6960c70abd8d4a94ULL, /* lg_f_9371_s */
    0x696c2a47c19558eaULL, /* bd_m_9300_1_s */
    0x698b26bf5ca4bba5ULL, /* am_f_9520_s */
    0x69f53912b81febc1ULL, /* am_f_9510_am_m_9510_s */
    0x69fda3c95edd6593ULL, /* bd_a_9630_2_s */
    0x6a2d4e15f99aff18ULL, /* bd_a_9509_bd_a_9500_2_s */
    0x6b1e3e74986c50ddULL, /* am_f_9610_am_m_9610_s */
    0x6ba730bb84094caaULL, /* wp_a_1200_s */
    0x6bb40771faf9563dULL, /* lg_f_2560_lg_m_2560_s */
    0x6bd98fd72ccf9089ULL, /* wp_a_1537_s */
    0x6be1c71a722f29e1ULL, /* am_f_9300_s */
    0x6bec4bc857cadf5fULL, /* wp_a_1513_s */
    0x6bf2cddbdf6d68d9ULL, /* bd_m_9349_bd_m_9340_1_s */
    0x6c166ccaf1a267a0ULL, /* bd_m_9510_2_s */
    0x6ce2d3ff65aa3dd2ULL, /* wp_a_0603_s */
    0x6d39e44be574189cULL, /* am_f_9620_s */
    0x6db1412f8fff0b3dULL, /* wp_a_1102_s */
    0x6e31d97b52ffd890ULL, /* wp_a_0915_s */
    0x6e50fd199d33a6caULL, /* wp_a_0654_s */
    0x6e5babece178710cULL, /* lg_m_9550_s */
    0x6e9d6ed6d93d5ed8ULL, /* bd_a_8200_1_s */
    0x6ea085ad9167f560ULL, /* wp_a_1549_s */
    0x6f2a35a05f3abe53ULL, /* lg_m_2550_s */
    0x6f8726c0736122d4ULL, /* am_m_9540_s */
    0x704eb51b3f83f7cdULL, /* bd_f_9300_1_s */
    0x70705176e5ade1e1ULL, /* am_m_2520_s */
    0x70822006f0b9e591ULL, /* am_f_9260_am_m_9260_s */
    0x70eb79f0c4c81192ULL, /* wp_a_1301_s */
    0x7189e00ac208dacfULL, /* lg_f_9370_s */
    0x722e9b1405e2a89eULL, /* bd_m_9220_2_s */
    0x7252a3ca2b8edb65ULL, /* wp_a_0703_s */
    0x72a18af0c594b403ULL, /* bd_m_9379_bd_m_9372_1_s */
    0x732208fa55e54e38ULL, /* wp_a_1000_s */
    0x7327c0d7308ff2ccULL, /* wp_a_1534_s */
    0x73bda1dad0acf6c1ULL, /* hd_f_9540_hd_m_9540_s */
    0x73f2d61cfbe22522ULL, /* bd_a_9310_2_s */
    0x74a05a9e7b5c0017ULL, /* wp_a_0208_s */
    0x74eb0131ceaee915ULL, /* lg_f_2520_lg_m_2520_s */
    0x756fa7cea5b24a22ULL, /* wp_a_1507_s */
    0x75d9522f9478dae0ULL, /* wp_a_1101_s */
    0x75ea2244fcfc2c79ULL, /* am_f_9720_am_m_9720_s */
    0x75efbead952a183bULL, /* wp_a_1548_s */
    0x76140deeba2a31fbULL, /* lg_m_9380_s */
    0x7615a43cc15e9332ULL, /* hd_a_2570_s */
    0x76498034178f81b3ULL, /* wp_a_0107_s */
    0x765c49bb8aae6225ULL, /* wp_a_1201_s */
    0x76a5b14cba115219ULL, /* hr_f_0009_l_s */
    0x76a7fedbe612a71cULL, /* bd_m_9349_bd_m_9340_2_s */
    0x76c607c092b80038ULL, /* hr_m_0006_wave_s */
    0x76c6f2f3620938f0ULL, /* hd_m_9450_s */
    0x773e6dc6c5ba5e79ULL, /* lg_f_9390_lg_m_9390_s */
    0x779cc61b434423b0ULL, /* bd_f_9300_2_s */
    0x779db5848b22353eULL, /* hd_a_9250_s */
    0x783c54d22fd04925ULL, /* hd_m_9260_s */
    0x78d0189c0bde70d5ULL, /* lg_f_2550_lg_m_2550_s */
    0x78e7127b59a5246bULL, /* wp_a_0914_s */
    0x79061619a3d8bc45ULL, /* wp_a_0655_s */
    0x7913b2f0c942252dULL, /* wp_a_1300_s */
    0x7aacf52ae891ad75ULL, /* lg_m_9289_lg_m_9280_s */
    0x7aef4195840f0f42ULL, /* wp_a_0401_s */
    0x7af5aca0a050ba18ULL, /* wp_a_0810_s */
    0x7b4c7b1113f19fcdULL, /* hd_a_9130_s */
    0x7baa68dab18fa147ULL, /* am_m_9530_s */
    0x7c2ed4ca317c28e8ULL, /* wp_a_0700_s */
    0x7c69f2a0511789cdULL, /* bd_m_2520_1_s */
    0x7d061d98509daab9ULL, /* bd_m_9440_2_l_s */
    0x7d150c3f74f0c928ULL, /* lg_m_9280_s */
    0x7d2ed7cce732c74dULL, /* lg_f_9550_lg_m_9550_s */
    0x7d3d882da362be4fULL, /* am_m_9440_s */
    0x7d98a0c74dd924baULL, /* bd_a_9490_1_s */
    0x7d98b9341b51a48eULL, /* wp_a_0100_s */
    0x7ddcf9d737353ea7ULL, /* wp_a_1535_s */
    0x7dff35f09513767bULL, /* bd_m_9740_2_s */
    0x7e42684605fc52d3ULL, /* am_m_9260_s */
    0x7e7cda7bb7d54c61ULL, /* bd_m_9540_2_s */
    0x7f4d1ac4b248a873ULL, /* bd_f_9420_2_s */
    0x7f55739e82011592ULL, /* wp_a_0207_s */
    0x7f5c2b7b5cad6d26ULL, /* wp_a_0913_s */
    0x7f84acc48a374578ULL, /* wp_a_1404_s */
    0x7fc01dff70c95950ULL, /* wp_a_0601_s */
    0x7ff49f48309839a1ULL, /* lg_m_9230_lg_f_9230_s */
    0x808fd7023a2ba50cULL, /* am_a_9290_s */
    0x80d8f29d88507d02ULL, /* hd_m_9320_s */
    0x812cbd62e44e9da1ULL, /* lg_f_2570_lg_m_2570_s */
    0x812e4719a852c248ULL, /* wp_a_0656_s */
    0x819d0ad124ee1da4ULL, /* am_m_9610_s */
    0x81e48d47cf031963ULL, /* bd_m_9300_2_s */
    0x820d4623cee9c74cULL, /* lg_f_9520_s */
    0x82428bd2776f329cULL, /* lg_m_9460_s */
    0x827a5dd542b455daULL, /* hd_a_5320_s */
    0x831ec5a0a4cc4a53ULL, /* wp_a_0811_s */
    0x836347051632ef95ULL, /* lg_f_9210_lg_m_9210_s */
    0x83b803a054d7b5b0ULL, /* bd_m_2520_2_s */
    0x84311972c0f47ae9ULL, /* bd_a_9290_2_s */
    0x84e6f1ceadee3ca0ULL, /* wp_a_1505_s */
    0x84f20b7bbaddbde4ULL, /* bd_m_9540_1_s */
    0x84fe3140fc26d7deULL, /* hr_f_0009_s */
    0x855ce01f0757154bULL, /* hr_m_0010_s */
    0x85a47a958ab45b1dULL, /* wp_a_0400_s */
    0x85c0d2341fcb81c9ULL, /* wp_a_0101_s */
    0x860444489f3201caULL, /* am_f_9460_s */
    0x866d96a31f9800f5ULL, /* hd_f_9280_hd_m_9280_s */
    0x86b7468b6a059f18ULL, /* wp_a_0502_s */
    0x86d2b9afd36893bdULL, /* lg_m_2570_s */
    0x876842cef0491032ULL, /* bd_m_2550_s */
    0x877dac9e867b292dULL, /* wp_a_0206_s */
    0x881b6746c5597a31ULL, /* bd_a_9500_2_s */
    0x887d6019ac14aec3ULL, /* wp_a_0657_s */
    0x887e6a2034f14fbaULL, /* am_m_9340_s */
    0x88d25c2218f9f4a9ULL, /* am_a_9509_am_a_9500_s */
    0x88f736d40ae79de6ULL, /* am_a_9630_s */
    0x895fc5c49022b733ULL, /* wp_a_1405_s */
    0x896236ef80a9d251ULL, /* am_m_9389_am_m_9380_s */
    0x89f16daca438c0e3ULL, /* lg_m_9300_s */
    0x8a11647b6352b901ULL, /* wp_a_0912_s */
    0x8b17fcf0d3a94febULL, /* wp_a_1302_s */
    0x8bbfe48e545ac0d6ULL, /* bd_f_9341_1_s */
    0x8c198b958dbc9640ULL, /* wp_a_0403_s */
    0x8c3552168ec7e049ULL, /* lg_m_9500_s */
    0x8c5a1e1414c233f7ULL, /* bd_m_9220_1_s */
    0x8ca12a15ee598f5aULL, /* hr_m_0008_s */
    0x8cc27f23d58f1327ULL, /* lg_f_9521_s */
    0x8d0c2f12d9e0acd4ULL, /* am_a_9470_s */
    0x8d5e772b0e5127f1ULL, /* lg_f_9220_lg_m_9220_s */
    0x8d8eed745b83a675ULL, /* wp_a_0251_s */
    0x8dd965365e38f675ULL, /* bd_f_9400_bd_m_9400_1_s */
    0x8df09dc8fcbd3451ULL, /* am_f_5320_am_m_5320_s */
    0x8e175c95318c1739ULL, /* bd_a_9600_s */
    0x8e1e591d0ac1b07bULL, /* bd_a_9310_1_s */
    0x8e40d4a72824d3d6ULL, /* bd_a_9570_2_s */
    0x8ee05f8b6e812f53ULL, /* wp_a_0503_s */
    0x8ee54a72c798062cULL, /* bd_a_9290_1_s */
    0x8f3b46c72d63ec29ULL, /* hd_m_9372_s */
    0x8f8ff3ca0eb3db98ULL, /* lg_m_2520_s */
    0x8f958b44db18c4a7ULL, /* wp_m_0231_s */
    0x8f9b2aceb491d57bULL, /* wp_a_1504_s */
    0x8fc3c198bc4b2ab9ULL, /* lg_f_9310_lg_m_9310_s */
    0x903f28d0c33ba755ULL, /* bd_f_2560_bd_m_2560_1_s */
    0x907603342670c00cULL, /* wp_a_0102_s */
    0x90a68320396b2cf5ULL, /* am_m_9341_s */
    0x90aefec493e4da0eULL, /* wp_a_1402_s */
    0x90c6ccea2393bb11ULL, /* am_m_9510_s */
    0x90cb770d8c2bc044ULL, /* am_m_5320_s */
    0x90e96fff7a753ae6ULL, /* wp_a_0607_s */
    0x918aa1e2ea461e8aULL, /* lg_f_9320_s */
    0x918d15f0d6b198a6ULL, /* wp_a_1305_s */
    0x91ef9e347181f814ULL, /* lg_f_9380_s */
    0x9224c42df9460a4fULL, /* lg_f_9460_s */
    0x9239957b67ccbf04ULL, /* wp_a_0911_s */
    0x92d09846cbfeb874ULL, /* bd_a_9500_1_s */
    0x92eb913f623cc2c8ULL, /* hd_a_9630_s */
    0x934ec0fa9449a595ULL, /* lg_m_9509_lg_m_9500_s */
    0x937f9e7fa5199621ULL, /* lg_f_9400_lg_m_9400_s */
    0x93c2a4fa67cd224cULL, /* wp_a_1004_s */
    0x94dba2e7ce41fb5bULL, /* am_f_9360_s */
    0x951b6eacf99b1bd4ULL, /* hd_m_9300_s */
    0x953a700157befb05ULL, /* hd_a_1000_s */
    0x95702ec58fe7dd5eULL, /* am_m_9740_s */
    0x957fb427c098f9a1ULL, /* bd_f_9270_bd_m_9270_1_s */
    0x968a77c73126016cULL, /* hd_m_9371_s */
    0x96cec4959461e21bULL, /* wp_a_0402_s */
    0x96ea43ceb853c1f6ULL, /* wp_a_1503_s */
    0x96eb03c75c0088f3ULL, /* bd_a_9490_2_s */
    0x9727d668c7886330ULL, /* am_a_2570_s */
    0x972f5cd7455ca2e0ULL, /* wp_a_1538_s */
    0x973a7a4099028699ULL, /* lg_m_9521_lg_f_9521_s */
    0x97554b803782d30dULL, /* am_f_9280_am_m_9280_s */
    0x975b98b27f27477aULL, /* am_f_9529_am_f_9521_s */
    0x975defe1e9719966ULL, /* lg_m_9400_s */
    0x978d59d0c6fc0998ULL, /* bd_f_2560_bd_m_2560_2_s */
    0x97c53c342a32e2e7ULL, /* wp_a_0103_s */
    0x9838a8ff7e375dc1ULL, /* wp_a_0606_s */
    0x98441e746228e4b8ULL, /* wp_a_0252_s */
    0x988e963664de34b8ULL, /* bd_f_9400_bd_m_9400_2_s */
    0x996a2d4441c0b35bULL, /* am_m_2540_s */
    0x996c3049b486df1dULL, /* bd_m_9280_1_s */
    0x9981f69e90e253ebULL, /* wp_a_0204_s */
    0x9987ae7b6b8cf87fULL, /* wp_a_0910_s */
    0x9994988b7524c82eULL, /* wp_a_0504_s */
    0x99a7b219b5c24359ULL, /* wp_a_0651_s */
    0x99bef2f569cb4f53ULL, /* am_m_9379_am_m_9372_s */
    0x9a07e1d64abfadeaULL, /* lg_m_9372_s */
    0x9a7b63af95907b96ULL, /* hd_a_8200_s */
    0x9c41c3c81235606aULL, /* wp_a_0106_mailbreaker_s */
    0x9c424ef0dd56e481ULL, /* wp_a_1304_s */
    0x9d3e4a05cba0020aULL, /* am_m_9410_s */
    0x9e18c669560f00beULL, /* wp_a_0302_s */
    0x9e3557325fb8b50eULL, /* am_a_9560_s */
    0x9e7e95d7491ec5bbULL, /* wp_a_1539_s */
    0x9e8f19d9bd3de750ULL, /* bd_f_9480_s */
    0x9f18f872511dde0eULL, /* wp_a_0908_s */
    0x9f2213238b6da7a4ULL, /* am_a_9500_s */
    0x9f2eb26925dd0525ULL, /* lg_m_9250_s */
    0x9f4bf85e31fc9961ULL, /* bd_f_9371_1_s */
    0x9f5ae527c6849424ULL, /* bd_f_9270_bd_m_9270_2_s */
    0x9fd466758d319e69ULL, /* hd_m_9289_hd_m_9280_s */
    0x9fe14149b78f1a40ULL, /* bd_m_9280_2_s */
    0x9ff70f9e93ea9ca6ULL, /* wp_a_0203_s */
    0xa00ab18b782ec3e9ULL, /* wp_a_0505_s */
    0xa01c31fc24bea1d9ULL, /* lg_m_9430_s */
    0xa060d9ff82b163c4ULL, /* wp_a_0605_s */
    0xa0c57ccebe3f6a11ULL, /* wp_a_1502_s */
    0xa0cf933b0aed3272ULL, /* am_m_9371_s */
    0xa1046967b86c0901ULL, /* bd_m_9720_2_s */
    0xa13eb0c737c99a47ULL, /* hd_m_9370_s */
    0xa173d6c0bf8dac82ULL, /* hd_m_9430_s */
    0xa1ff577f04bbd9d1ULL, /* lg_f_9540_lg_m_9540_s */
    0xa2b1f01e51e81531ULL, /* lg_f_9440_lg_m_9440_s */
    0xa42d368f300e2b0dULL, /* wp_a_0260_s */
    0xa45ce319bc67819cULL, /* wp_a_0652_s */
    0xa512678e62825b6fULL, /* bd_f_9341_2_s */
    0xa53dd88b5de3439dULL, /* hd_f_9260_hd_m_9260_s */
    0xa5c1295e35050ae4ULL, /* bd_f_9371_2_s */
    0xa678cd410ec84fb2ULL, /* hr_f_0005_s */
    0xa74111725597bb49ULL, /* wp_a_0909_s */
    0xa744cc3b0df5b18dULL, /* am_m_9370_s */
    0xa77ffdf25d021b7cULL, /* lg_f_9300_s */
    0xa79357a7364c6e6fULL, /* bd_a_9570_1_s */
    0xa814adcec2017f54ULL, /* wp_a_1501_s */
    0xa92c9a67bce60f04ULL, /* bd_m_9720_1_s */
    0xa9daaf350d42f7a5ULL, /* bd_f_9372_2_s */
    0xa9ec8d75b729bca1ULL, /* lg_f_2870_lg_m_2870_s */
    0xaaabce6ae51b3f3cULL, /* hd_m_9380_s */
    0xaaac489e9a8fe881ULL, /* wp_a_0202_s */
    0xaab31958cdb47005ULL, /* hd_m_9230_hd_f_9230_s */
    0xaabee28b7ed24f2cULL, /* wp_a_0506_s */
    0xaadb81c4a2c61867ULL, /* wp_a_1401_s */
    0xab0c3d763e2dbe39ULL, /* hd_f_9610_hd_m_9610_s */
    0xab15f2ff8956793fULL, /* wp_a_0604_s */
    0xab322bd6546d34e8ULL, /* lg_m_9370_s */
    0xac851c19c0e19537ULL, /* wp_a_0653_s */
    0xaeb095a18567b061ULL, /* hd_f_9400_hd_m_9400_s */
    0xaedd13872d25cffbULL, /* hd_a_9550_s */
    0xaee2478f36b332f0ULL, /* wp_a_0263_s */
    0xb01c10696074787cULL, /* wp_a_0300_s */
    0xb04fe035104b6928ULL, /* bd_f_9372_1_s */
    0xb055064114b5aacdULL, /* hr_f_0004_s */
    0xb0b2a217ba5f0da7ULL, /* hd_a_2780_s */
    0xb10255d470c619ddULL, /* bd_f_5320_bd_m_5320_1_s */
    0xb102c3f8793a9601ULL, /* lg_f_9490_lg_m_9490_s */
    0xb1321d373ba667c1ULL, /* am_m_5370_s */
    0xb209306e811bf2d5ULL, /* bd_a_9250_2_s */
    0xb20e1b8b82947207ULL, /* wp_a_0507_s */
    0xb28044d6582d6e63ULL, /* lg_m_9371_s */
    0xb2d4799e9f09ee84ULL, /* wp_a_0201_s */
    0xb3278e15a0727b96ULL, /* bd_m_9410_1_s */
    0xb333a6153e5bfefcULL, /* am_m_9430_s */
    0xb37688ecf31f1f39ULL, /* bd_f_9620_2_s */
    0xb570600e6207d149ULL, /* hd_a_5350_s */
    0xb5841bff8d04d31fULL, /* hd_a_5370_s */
    0xb5b652d030d84b54ULL, /* hd_m_9410_s */
    0xb603562f15e52bc5ULL, /* bd_a_2570_2_s */
    0xb65b14d6a9cdb411ULL, /* hd_m_9350_s */
    0xb73e2e38d914a8f1ULL, /* hd_f_9360_s */
    0xb76f6095a649b62fULL, /* wp_a_0406_s */
    0xb79b55a16872d86eULL, /* hd_a_9500_s */
    0xb81850a02a5417bfULL, /* hd_m_9280_s */
    0xb8b087965e7b2195ULL, /* am_m_9480_am_f_9480_s */
    0xb948163b185b294bULL, /* am_m_9372_s */
    0xb957616e84dc5518ULL, /* bd_a_9250_1_s */
    0xb9b3ae3584e4f1fdULL, /* lg_f_9270_lg_m_9270_s */
    0xb9c378bb7bbf97c9ULL, /* bd_f_9340_1_s */
    0xba22929ea2ca27ffULL, /* wp_a_0200_s */
    0xba36348b870e4f42ULL, /* wp_a_0508_s */
    0xba3cd7976c384f2aULL, /* lg_m_2540_s */
    0xbac660e426d3aa41ULL, /* bd_m_9230_bd_f_9230_1_s */
    0xbadd66d476b17e00ULL, /* bd_f_5320_bd_m_5320_2_s */
    0xbb2e81b0ec951c35ULL, /* lg_f_9610_lg_m_9610_s */
    0xbba5a80bd1c50529ULL, /* bd_f_9220_bd_m_9220_1_s */
    0xbbbb04ec8b186531ULL, /* lg_f_9470_lg_m_9470_s */
    0xbc1dbef2866341e9ULL, /* am_m_9710_am_f_9710_s */
    0xbc65c5fc9275e081ULL, /* bd_f_9430_bd_m_9430_1_s */
    0xbcc253fe4b22d325ULL, /* bd_f_9720_bd_m_9720_1_s */
    0xbd22735ac9ad35e5ULL, /* lg_m_8200_s */
    0xbdd5b94d3976baa1ULL, /* am_a_9550_s */
    0xbe2ab9ecf9c2aa7cULL, /* bd_f_9620_1_s */
    0xbe2b872f1a5f31c8ULL, /* bd_a_2570_1_s */
    0xbe59998f3eef3306ULL, /* wp_a_0265_s */
    0xbfbcf746c2aad724ULL, /* am_a_9210_s */
    0xc06a770e702f0355ULL, /* bd_m_9480_bd_f_9480_s */
    0xc07bc69cee80994dULL, /* lg_m_9309_lg_m_9300_s */
    0xc07fefc4b542825dULL, /* bd_f_9540_bd_m_9540_1_s */
    0xc0da7a89296b6ebaULL, /* bd_m_9260_2_s */
    0xc1d994ea6f06c9c7ULL, /* hd_m_9460_s */
    0xc210b26b2a87949dULL, /* lg_f_9250_lg_m_9250_s */
    0xc25850411f1b228bULL, /* hr_f_0006_s */
    0xc2b1377b982142b5ULL, /* hd_a_9220_s */
    0xc2c3f36cc31c918bULL, /* hd_a_9200_s */
    0xc2ee91e42b4db044ULL, /* bd_m_9230_bd_f_9230_2_s */
    0xc2f4d90bd5871a6cULL, /* bd_f_9220_bd_m_9220_2_s */
    0xc3161f1793254f9dULL, /* lg_f_5370_lg_m_5370_s */
    0xc320947265ea8e22ULL, /* wp_a_0904_s */
    0xc33784fe4e2b44a8ULL, /* bd_f_9720_bd_m_9720_2_s */
    0xc37fd6e27c61e703ULL, /* wp_a_0284_s */
    0xc382b5d16606f34dULL, /* lg_f_9630_lg_m_9630_s */
    0xc478a9bb8264d60cULL, /* bd_f_9340_2_s */
    0xc48df6fc96efe684ULL, /* bd_f_9430_bd_m_9430_2_s */
    0xc4eb6d8b8db39b1dULL, /* wp_a_0509_s */
    0xc551e682e2adb20dULL, /* bd_a_9550_1_s */
    0xc55aa41c0a533db9ULL, /* lg_f_9350_s */
    0xc5deb6408d5329c9ULL, /* bd_m_2560_1_s */
    0xc65ef50820eeecacULL, /* hd_a_9120_s */
    0xc6d42e2c3c85b037ULL, /* wp_a_1399_s */
    0xc6df7aefb6b0538eULL, /* hd_m_9540_s */
    0xc740e0b704782244ULL, /* am_f_9530_s */
    0xc7e14d358e5a23ecULL, /* hd_f_9521_s */
    0xc845d920f02e092bULL, /* am_a_9310_s */
    0xc8d32e181be3ffdfULL, /* hd_m_0001_m_s */
    0xc90dd28f4592cbe1ULL, /* wp_a_0264_s */
    0xc916b4715f9e5422ULL, /* wp_a_0301_estoc_s */
    0xc9550a3d12f9e3aaULL, /* am_m_9270_s */
    0xc9909e059f19c24bULL, /* hd_a_2920_s */
    0xc9a488ff4a159e6aULL, /* bd_m_6200_1_s */
    0xca0f0c747dbe4d62ULL, /* wp_a_0258_s */
    0xca5b00c4bb2de680ULL, /* bd_f_9540_bd_m_9540_2_s */
    0xca6ecd7269aafdfdULL, /* wp_a_0905_s */
    0xcac90dc9824307d3ULL, /* lg_m_9450_s */
    0xcc7a1115ae9a162fULL, /* bd_m_9410_2_s */
    0xccddae5e4f967929ULL, /* bd_m_9309_bd_m_9300_1_s */
    0xccec595ecaa7fae8ULL, /* bd_m_9440_1_l_s */
    0xcd9a63066391b63aULL, /* bd_a_9390_2_s */
    0xcdae2e0824b10f87ULL, /* hd_a_9121_s */
    0xcdea64cd724dc911ULL, /* wp_m_1533_s */
    0xce9f14862b23a3d8ULL, /* wp_a_0274_s */
    0xcead962ff268783aULL, /* hd_m_9610_s */
    0xcec259deda6a4a85ULL, /* am_m_9289_am_m_9280_s */
    0xcf83038f489b3d64ULL, /* wp_a_0267_s */
    0xcf8aa6d2ddfdbeedULL, /* hd_a_3460_s */
    0xd006f782e952b9f0ULL, /* bd_a_9550_2_s */
    0xd03d88a6eeb8a04aULL, /* wp_a_1550_s */
    0xd05c776330357c6dULL, /* am_f_9430_am_m_9430_s */
    0xd093e74093f8680cULL, /* bd_m_2560_2_s */
    0xd0e899603e4495b1ULL, /* bd_a_9210_2_s */
    0xd10e4c3eebad2139ULL, /* am_m_9460_s */
    0xd12474868ed4cacdULL, /* bd_m_9520_bd_f_9520_1_s */
    0xd21fe7a887f0cc53ULL, /* wp_a_0602_cadel_s */
    0xd295863594fdbcc7ULL, /* hd_f_9520_s */
    0xd297de726e2680a0ULL, /* wp_a_0906_s */
    0xd36f5dc1d6471e04ULL, /* wp_a_0731_s */
    0xd42cdf5e53588e6cULL, /* bd_m_9309_bd_m_9300_2_s */
    0xd494f21feaa2d4cbULL, /* hd_m_9379_hd_m_9372_s */
    0xd4a928e2860dc899ULL, /* wp_a_0282_s */
    0xd4c445748463993dULL, /* wp_a_0259_s */
    0xd4d00899bb8d0755ULL, /* lg_f_9620_s */
    0xd5050fa3b13804baULL, /* lg_m_9310_s */
    0xd511b35130519e49ULL, /* bd_f_9260_bd_m_9260_1_s */
    0xd54cfa1f5aa9894dULL, /* hd_f_9341_hd_m_9341_s */
    0xd5b8fca96fb86841ULL, /* am_f_9540_am_m_9540_s */
    0xd5f20ec9d7a3afc4ULL, /* hd_m_9270_s */
    0xd645a2434061e33bULL, /* hd_a_2560_s */
    0xd6af3367040bbb21ULL, /* lg_f_9430_lg_m_9430_s */
    0xd6b5ef9bf8f2f7e1ULL, /* wp_a_1700_s */
    0xd6f5f8f9e10ae391ULL, /* hd_m_9440_s */
    0xd72972f3acd86e59ULL, /* lg_m_9480_lg_f_9480_s */
    0xd75b8ffec6c23c96ULL, /* lg_m_9530_s */
    0xd7dad119d96e7ec6ULL, /* wp_a_0658_s */
    0xd87285869294f6b0ULL, /* bd_m_9520_bd_f_9520_2_s */
    0xd888282c085c3924ULL, /* hd_f_9460_s */
    0xd8989b1618d0365dULL, /* hr_m_0001_s */
    0xd9002e095b3166e9ULL, /* bd_f_9520_1_s */
    0xda2cdd893792d2f3ULL, /* bd_m_9260_1_s */
    0xda381c8f4f4052dfULL, /* wp_a_0266_s */
    0xda96d7fa061ad4f5ULL, /* lg_m_9620_lg_f_9620_s */
    0xdabd76c1da07577fULL, /* wp_a_0730_s */
    0xdac57602aacba9a4ULL, /* wp_a_1361_s */
    0xdb9dca6044e9d3f4ULL, /* bd_a_9210_1_s */
    0xdbaad3412d42bd24ULL, /* hr_f_0003_s */
    0xdbf6bd85d2f944d1ULL, /* bd_m_9420_1_s */
    0xdbf859e289cfdddcULL, /* wp_a_0281_s */
    0xdc1c5ecdf1116019ULL, /* am_f_9740_am_m_9740_s */
    0xdc1dd5dff91bfeeaULL, /* wp_a_1525_s */
    0xdcfd38dedaf63bd3ULL, /* am_a_9390_s */
    0xdd4c177274ca197bULL, /* wp_a_0907_s */
    0xde166686335fa3eeULL, /* wp_a_0272_s */
    0xde709d7070b3ea21ULL, /* am_a_9240_s */
    0xded269d28c7ec925ULL, /* lg_f_9290_lg_m_9290_s */
    0xdf1f3402252b67daULL, /* bd_m_9400_2_s */
    0xdf2a0a19dd30a1a1ULL, /* wp_a_0659_s */
    0xdf30d021a6a31882ULL, /* bd_m_9372_2_s */
    0xdfac66c17096b421ULL, /* lg_f_1000_lg_m_1000_s */
    0xdfc6e45136f6dc8cULL, /* bd_f_9260_bd_m_9260_2_s */
    0xdfef24dedb4108d2ULL, /* am_f_9370_s */
    0xe1110a0635df6b2bULL, /* hd_f_9300_s */
    0xe15d4df912ca71caULL, /* am_f_9341_s */
    0xe1dc50364415dbf7ULL, /* am_a_2870_s */
    0xe2031122f770a2b4ULL, /* lg_m_5370_s */
    0xe2148f02ae8d961fULL, /* wp_a_1360_s */
    0xe21cebff57835ee3ULL, /* bd_m_6200_2_s */
    0xe27191d8f670dd3dULL, /* lg_m_9470_s */
    0xe2f9ec413104a99fULL, /* hr_f_0002_s */
    0xe32203b06fa9daf6ULL, /* hd_a_2660_s */
    0xe36beedffcdc3865ULL, /* wp_a_1524_s */
    0xe3b45f0961d4f22cULL, /* bd_f_9520_2_s */
    0xe42092e28e49f177ULL, /* wp_a_0280_s */
    0xe47daf3b4ebfc0d2ULL, /* am_m_2550_s */
    0xe52ebf73cc670751ULL, /* hd_a_4160_s */
    0xe5c1b101c702605cULL, /* hd_a_9700_s */
    0xe6aaee85d99cd014ULL, /* bd_m_9420_2_s */
    0xe6ecc60671b91a73ULL, /* bd_a_9390_1_s */
    0xe79840d56f4c603bULL, /* bd_m_2540_s */
    0xe7c7f0515bac2932ULL, /* lg_m_9260_s */
    0xe7f17f86394b15a9ULL, /* wp_a_0273_s */
    0xe8175ddedfbb1c6dULL, /* am_f_9371_s */
    0xe86dca57d4a36db7ULL, /* lg_m_9700_s */
    0xe8808ce2d21db065ULL, /* bd_a_9730_1_s */
    0xe986d52d66d9a589ULL, /* bd_m_9360_2_s */
    0xe9a7d86ede52dc8bULL, /* wp_a_1100_wingedspear_s */
    0xe9c2e516227dbd5bULL, /* hr_m_0003_s */
    0xe9c532be429d6f7eULL, /* hd_m_9360_s */
    0xe9c956f1fbfd8dd2ULL, /* bd_f_9360_2_s */
    0xea04bb7f9cf6ce7dULL, /* am_f_9440_am_m_9440_s */
    0xea12bbfdcacf915bULL, /* wp_a_0652_l_s */
    0xea3872f97a66a57cULL, /* hr_f_0008_tail_s */
    0xeb2ecbf331bc2518ULL, /* lg_m_9420_s */
    0xeb6c87339b4319edULL, /* hd_a_9509_hd_a_9500_s */
    0xebe211a986a3eac5ULL, /* bd_f_9440_bd_m_9440_1_s */
    0xec1266f9196f8745ULL, /* am_f_9340_s */
    0xec5b4c207355f8deULL, /* hd_f_9350_s */
    0xecf28467c55b7f91ULL, /* bd_m_5320_2_s */
    0xed481fe002c985e8ULL, /* wp_a_1527_s */
    0xee0769db4dd94876ULL, /* bd_m_9270_1_s */
    0xee7669727e77ae11ULL, /* wp_a_0901_s */
    0xef40b0863d0d2aecULL, /* wp_a_0270_s */
    0xef745e2fdaf48d52ULL, /* hd_f_9372_s */
    0xf0c56fdf37cdffe5ULL, /* am_m_9620_am_f_9620_s */
    0xf0d5062d6a9a07ccULL, /* bd_m_9360_1_s */
    0xf25846321af4b16cULL, /* am_m_9360_s */
    0xf25cbde2d80afde8ULL, /* bd_a_9730_2_s */
    0xf29e5d86cf418551ULL, /* bd_f_9740_bd_m_9740_1_s */
    0xf2cc6edee6602450ULL, /* am_f_9372_s */
    0xf32e33370c3b38dfULL, /* hd_a_9600_s */
    0xf3cb71be8a3c668dULL, /* lg_m_9320_s */
    0xf40a42a98b1df0c8ULL, /* bd_f_9440_bd_m_9440_2_s */
    0xf4296760af0e1d8aULL, /* hd_f_9620_s */
    0xf441b567c91d94d4ULL, /* bd_m_5320_1_s */
    0xf457584431167600ULL, /* hr_m_0012_tail_s */
    0xf477fe162922d2d6ULL, /* hr_m_0004_s */
    0xf49638e00689bf63ULL, /* wp_a_1526_s */
    0xf4c212c504d548adULL, /* lg_f_5320_lg_m_5320_s */
    0xf5c59a728239c354ULL, /* wp_a_0902_s */
    0xf5c869c420a79dd3ULL, /* hd_f_9380_s */
    0xf68cf2ed0ee0df44ULL, /* hd_m_9740_s */
    0xf757dc6f468bc0d1ULL, /* am_a_2560_s */
    0xf7a95321b4110f5bULL, /* bd_m_9372_1_s */
    0xf7c0365a52381581ULL, /* hd_f_9430_hd_m_9430_s */
    0xf8427380c35d1791ULL, /* bd_f_9530_2_s */
    0xf8682214d1df6313ULL, /* wp_a_0801_scythe_s */
    0xf8707f133c9925f9ULL, /* bd_m_9389_bd_m_9380_1_s */
    0xf87197023352cc13ULL, /* bd_m_9400_1_s */
    0xf9a25ec600255ce1ULL, /* bd_a_9470_2_s */
    0xf9f4e98643b0c3c7ULL, /* wp_a_0271_s */
    0xfca037162d9ce671ULL, /* hr_m_0005_s */
    0xfcb02e6745ede3c1ULL, /* lg_m_9720_s */
    0xfcbe71e00b03d2feULL, /* wp_a_1521_s */
    0xfce46c60cdb06bc4ULL, /* lg_m_9240_s */
    0xfd528e86d5e51094ULL, /* bd_f_9740_bd_m_9740_2_s */
    0xfd6d280c4aa168b3ULL, /* am_m_9400_s */
    0xfdedb37286b3a08fULL, /* wp_a_0903_s */
    0xfdf6fab5d130d2aeULL, /* hd_a_3322_s */
    0xfea95f5e1499ef69ULL, /* hd_m_9620_hd_f_9620_s */
    0xff91a480c71f2cd4ULL, /* bd_f_9530_1_s */
    0xffa0bd9c057005ddULL, /* bd_f_9230_1_s */
    0xffbcb13fecfb12d8ULL, /* wp_a_0600_club_s */
    0xffbfb013405b3b3cULL, /* bd_m_9389_bd_m_9380_2_s */
    0xffc52a7253366a7aULL, /* bd_m_9450_1_s */
};

static const u64 k_normal_names[] = {
    0x001794c6032dd6e3ULL, /* bd_a_9470_1_n */
    0x001c3ae4c9de6ed9ULL, /* lg_m_1000_n */
    0x012ee6088b73cef0ULL, /* am_f_9320_n */
    0x0251a52fe613a3b7ULL, /* hd_f_9370_n */
    0x025ae8f96526cd7dULL, /* hd_f_9480_n */
    0x0268f62d7cf06d65ULL, /* bd_m_9371_1_n */
    0x0272b9cdf9383283ULL, /* lg_m_9341_n */
    0x02adea987570532eULL, /* hd_a_9570_n */
    0x032aabc64e2a718bULL, /* am_m_9372_l_n */
    0x0342c24ca7a34da2ULL, /* lg_m_5320_n */
    0x037a334384dda985ULL, /* bd_f_9370_1_n */
    0x03f4e4f20add2bdcULL, /* bd_f_9360_1_n */
    0x05d4a169f7f3524dULL, /* am_f_9420_n */
    0x06872dcd07cbc15dULL, /* bd_a_2870_2_n */
    0x0759dfdb5c00ccf8ULL, /* bd_m_9270_2_n */
    0x084da7418db0b6ddULL, /* am_m_9300_n */
    0x097beb9c0b5b9b47ULL, /* bd_f_9230_2_n */
    0x099fec2fe9d42b5cULL, /* hd_f_9371_n */
    0x09a4f6186711d602ULL, /* lg_f_9740_lg_m_9740_n */
    0x09c0e0cdfcf883c8ULL, /* lg_m_9340_n */
    0x0ad0643dcfa9c0ecULL, /* am_a_9220_n */
    0x0b4a6a63cdf05411ULL, /* hd_a_9210_n */
    0x0c6803ff90658288ULL, /* lg_m_6200_n */
    0x0c768fe2ab6470cdULL, /* am_a_9490_n */
    0x0cf5edfc7d097c62ULL, /* bd_m_9610_2_n */
    0x0d80e528f1cf06deULL, /* bd_m_9460_1_n */
    0x0e1efb9c5b989c46ULL, /* bd_a_9240_1_n */
    0x0e2ca8e66276cb84ULL, /* lg_m_9560_n */
    0x0f2159b5dade7d5bULL, /* hd_a_3320_n */
    0x106d4b131750d70fULL, /* lg_f_9340_n */
    0x114d56c6a68bbeb2ULL, /* hd_m_2540_n */
    0x1159ff044305bc0dULL, /* lg_f_9360_n */
    0x11adaa029c68e74cULL, /* bd_a_1000_n */
    0x12547f0ca084e294ULL, /* lg_f_9420_n */
    0x12c9045affd45aa2ULL, /* hd_f_9720_hd_m_9720_n */
    0x12dfa3d7ef642fecULL, /* hd_m_9510_n */
    0x13b65669187d110aULL, /* hd_a_9310_n */
    0x14fca43193eb30eaULL, /* hd_m_9349_hd_m_9340_n */
    0x154cc65bb113296cULL, /* am_a_1000_n */
    0x15dbb4c08f40a129ULL, /* lg_m_9440_n */
    0x15e524e83b28b8a1ULL, /* lg_m_9390_n */
    0x16154face101cb22ULL, /* lg_f_9570_lg_m_9570_n */
    0x167060b5dea04b40ULL, /* hd_a_3321_n */
    0x1676095224464387ULL, /* lg_m_9590_n */
    0x1797abff6be9e992ULL, /* am_m_9520_am_f_9520_n */
    0x17aaf8fc83ae7a13ULL, /* bd_m_9610_1_n */
    0x18363028f874714fULL, /* bd_m_9460_2_n */
    0x18d4269c623dd057ULL, /* bd_a_9240_2_n */
    0x1917a872615dfc94ULL, /* bd_m_9450_2_n */
    0x1a4872131d3c6094ULL, /* lg_f_9341_n */
    0x1a97e51cc4eb792aULL, /* hd_m_9480_hd_f_9480_n */
    0x1ae26f2d8a600640ULL, /* bd_m_9371_2_n */
    0x1b1c152c9fd1a815ULL, /* hd_f_9530_n */
    0x1b8e3d14a8c3f256ULL, /* hd_f_9340_hd_m_9340_n */
    0x1bfeea5b681a01d5ULL, /* hd_a_4150_n */
    0x1d0da727245f9f8dULL, /* am_m_9450_n */
    0x1d4afd8a7d2aef92ULL, /* lg_m_9210_n */
    0x1d79b0a08b73822eULL, /* am_f_9400_am_m_9400_n */
    0x1da5ac4393bd23e0ULL, /* bd_f_9370_2_n */
    0x1edea1a35346cce6ULL, /* lg_f_8200_lg_m_8200_n */
    0x1f718f10fce434deULL, /* hd_f_2550_hd_m_2550_n */
    0x1fd9a6cd15f34af8ULL, /* bd_a_2870_1_n */
    0x2167ca434ee8a44eULL, /* bd_f_9380_1_n */
    0x21a5e374f5c398faULL, /* hd_f_9270_hd_m_9270_n */
    0x22171011be83e83dULL, /* bd_f_9460_1_n */
    0x2222a294d9150a4dULL, /* bd_m_9340_1_n */
    0x2264d12c54ac7260ULL, /* lg_m_9490_n */
    0x228dfa21bae25d89ULL, /* hd_a_9290_n */
    0x22e6a7169b88e9e2ULL, /* lg_f_9200_lg_m_9200_n */
    0x234814f5d4793fe6ULL, /* lg_m_9540_n */
    0x239b9523155b22ecULL, /* hd_f_9450_n */
    0x23c5f9569e51766aULL, /* bd_f_9710_2_n */
    0x23f7de8ba4b7d92fULL, /* hd_a_9490_n */
    0x254fd7f3a67cfe8dULL, /* bd_m_9530_2_n */
    0x264554a796e79ceaULL, /* bd_m_9289_bd_m_9280_1_n */
    0x265d8bd9bb0c1dbaULL, /* bd_m_9430_2_n */
    0x26a3a8a95466d66bULL, /* lg_m_2560_n */
    0x26ed8a3b6adfef8aULL, /* bd_f_5370_bd_m_5370_1_n */
    0x275cf534a372260cULL, /* lg_m_9740_n */
    0x27f3662ec87a224aULL, /* bd_m_9521_bd_f_9521_1_n */
    0x28701395dae46e84ULL, /* hd_m_9590_n */
    0x287e31fd91fb7062ULL, /* am_f_9230_n */
    0x287f235c210a8d2aULL, /* lg_f_9710_n */
    0x28aa34677cf97787ULL, /* am_a_9250_n */
    0x298fd543536269bfULL, /* bd_f_9380_2_n */
    0x2a2b5baa40cc894eULL, /* hd_f_9740_hd_m_9740_n */
    0x2a9f8f1b3e387986ULL, /* bd_f_2520_bd_m_2520_1_n */
    0x2abfa58b63cbfb06ULL, /* bd_f_9510_bd_m_9510_1_n */
    0x2b1fe278c65fa926ULL, /* bd_m_5370_1_n */
    0x2bb1168d7d6b05c0ULL, /* hd_a_9240_n */
    0x2bdba1d5e8f7fe8aULL, /* am_f_9380_n */
    0x2c0659100fc64bbfULL, /* lg_m_9510_n */
    0x2d2baf62c9a29962ULL, /* hd_f_9600_hd_a_9600_n */
    0x2d5ee5ab82d08d69ULL, /* bd_a_9700_1_n */
    0x2e3bd53b6ea07dfbULL, /* bd_f_5370_bd_m_5370_2_n */
    0x2e3f7e9f3a902046ULL, /* lg_f_9730_lg_m_9730_n */
    0x2e7a4456a4f52ddbULL, /* bd_f_9710_1_n */
    0x2fa3a9ee8e58bea2ULL, /* am_m_9309_am_m_9300_n */
    0x303f025804b1bb6dULL, /* bd_f_9529_bd_f_9521_1_n */
    0x30402cd122aeeb66ULL, /* bd_m_9320_2_n */
    0x3082a895b7d2c97dULL, /* bd_f_9320_2_n */
    0x30af402266e53b3eULL, /* lg_f_9510_lg_m_9510_n */
    0x30f99fa79d8b545bULL, /* bd_m_9289_bd_m_9280_2_n */
    0x3101545172e3cb4aULL, /* bd_m_9620_bd_f_9620_1_n */
    0x31082f364e8e068fULL, /* lg_f_9450_n */
    0x311296d9c1b11b6bULL, /* bd_m_9430_1_n */
    0x31ae4df8d4b98c74ULL, /* hd_a_9730_n */
    0x31edba1b41f8d197ULL, /* bd_f_2520_bd_m_2520_2_n */
    0x32026d4867f33d6aULL, /* lg_m_9290_n */
    0x320dd08b678c5317ULL, /* bd_f_9510_bd_m_9510_2_n */
    0x324b978579306525ULL, /* am_m_9380_n */
    0x32a8b12ecf1f8cbbULL, /* bd_m_9521_bd_f_9521_2_n */
    0x331840df222fd2a2ULL, /* bd_f_9350_2_n */
    0x3445b1a0b45c341eULL, /* hd_m_9520_hd_f_9520_n */
    0x3484f7b815173ce0ULL, /* lg_m_9379_lg_m_9372_n */
    0x34c03cc89b5d19c8ULL, /* hd_f_9230_n */
    0x35048b9740b6991fULL, /* hd_m_6200_n */
    0x35d50d78cd04dd37ULL, /* bd_m_5370_2_n */
    0x36faaf2cc1ec4926ULL, /* lg_f_9530_n */
    0x376ec1178054f03eULL, /* hd_f_9320_n */
    0x378f57d12670f677ULL, /* bd_m_9320_1_n */
    0x3842324ac1649e2eULL, /* am_f_5370_am_m_5370_n */
    0x3868bf04d2879602ULL, /* lg_f_9260_lg_m_9260_n */
    0x38907c953254bd86ULL, /* bd_f_body_m_n */
    0x39142a9b16bbca31ULL, /* hd_a_9470_n */
    0x398570d3191e8f52ULL, /* lg_f_2540_lg_m_2540_n */
    0x3a22f2169b5a9c72ULL, /* lg_f_9280_lg_m_9280_n */
    0x3a473294029e902cULL, /* hd_m_2520_n */
    0x3a674bdf25f1a753ULL, /* bd_f_9350_1_n */
    0x3a6b3ce25569815dULL, /* lg_m_2870_n */
    0x3b3021290e6bf35eULL, /* lg_m_9610_n */
    0x3b3ca9ce9967ffbaULL, /* am_m_9521_am_f_9521_n */
    0x3b42f51a39676afcULL, /* lg_m_9630_n */
    0x3b698911ccab71d8ULL, /* bd_f_9460_2_n */
    0x3bb69f51798935bbULL, /* bd_m_9620_bd_f_9620_2_n */
    0x3d6ac32927f20364ULL, /* am_m_9350_n */
    0x3d902b0314bdc5c6ULL, /* hd_f_2520_hd_m_2520_n */
    0x3dc950f3b3ec9768ULL, /* bd_m_9530_1_n */
    0x3e011b94e9661928ULL, /* bd_m_9340_2_n */
    0x3e0849e65836e5e6ULL, /* lg_f_9720_lg_m_9720_n */
    0x3e5fdd5f72c519d5ULL, /* bd_m_9440_2_n */
    0x3ec3efcca19b7e89ULL, /* am_f_9450_l_n */
    0x3f134fb44573c753ULL, /* am_m_9720_n */
    0x3fb837babe6970a2ULL, /* am_m_9280_n */
    0x4175654cc2aad987ULL, /* hd_f_9420_n */
    0x420babf8d2f4bb06ULL, /* lg_m_9520_lg_f_9520_n */
    0x4277808d42184f7aULL, /* am_m_9349_am_m_9340_n */
    0x431b5c87baf22e82ULL, /* lg_f_9240_lg_m_9240_n */
    0x43af430fa9d78572ULL, /* am_f_9270_am_m_9270_n */
    0x459155d826c5a742ULL, /* bd_f_9450_1_n */
    0x45a4aa7bb424d93aULL, /* lg_f_9700_lg_m_9700_n */
    0x45d73eab903e3ce4ULL, /* bd_a_9700_2_n */
    0x460871a1781bae12ULL, /* hd_f_2540_hd_m_2540_n */
    0x466d93b72b6d705dULL, /* am_a_9700_n */
    0x46a6b427fc20220eULL, /* bd_m_9710_bd_f_9710_1_n */
    0x477addbd57a076d7ULL, /* am_f_9480_n */
    0x47f7d957daaf558cULL, /* lg_m_9270_n */
    0x489cc55e53a505a7ULL, /* lg_m_9730_n */
    0x49c825f0c142f663ULL, /* am_f_9350_n */
    0x49d52195c5fa5318ULL, /* bd_f_9320_1_n */
    0x4a6a7b58139135c8ULL, /* bd_f_9529_bd_f_9521_2_n */
    0x4a75221a99c1ba12ULL, /* bd_m_9370_1_n */
    0x4c4b60eadc7428d8ULL, /* lg_m_9410_n */
    0x4c6630815c5a8649ULL, /* bd_m_9350_2_n */
    0x4ca05d52b95dd502ULL, /* bd_f_9280_bd_m_9280_1_n */
    0x4ce060d82a877bf3ULL, /* bd_f_9450_2_n */
    0x4cfc28eff0ba30a9ULL, /* hd_f_9529_hd_f_9521_n */
    0x4d1a43ba066cd901ULL, /* bd_a_9560_1_n */
    0x4d50b5bbb41abd21ULL, /* bd_m_9380_1_n */
    0x4dad9cd07fa1c252ULL, /* bd_f_9610_bd_m_9610_1_n */
    0x4dc3a3fe5a0e4ae5ULL, /* hd_m_9720_n */
    0x4ecebf280099e77fULL, /* bd_m_9710_bd_f_9710_2_n */
    0x4f371f2ff4167eddULL, /* bd_f_9521_1_n */
    0x50ab25c950b5d379ULL, /* bd_a_9630_1_n */
    0x50e7296104f0b6f6ULL, /* am_f_9450_n */
    0x51b7d3d168e3faaaULL, /* am_a_9730_n */
    0x5248bb5ffec2259eULL, /* lg_f_9480_n */
    0x529d6d1a9e3bec43ULL, /* bd_m_9370_2_n */
    0x52f7cb2ff6cc7c1fULL, /* lg_f_9230_n */
    0x5321d83b52b9acb0ULL, /* am_a_9570_n */
    0x533042c5717b7fc6ULL, /* hd_f_9510_hd_m_9510_n */
    0x53bc66ada1b418b2ULL, /* hd_a_9390_n */
    0x541b1d7218ddd765ULL, /* lg_m_9220_n */
    0x54acc836e148827eULL, /* lg_f_9560_lg_m_9560_n */
    0x54d44cc509f76bddULL, /* lg_m_9350_n */
    0x55738bd886d9965eULL, /* hd_m_9400_n */
    0x55865fc9b1d50dfcULL, /* hd_m_9420_n */
    0x55bab2ac76a6acdaULL, /* bd_m_9341_1_n */
    0x55d5e7d0841bf483ULL, /* bd_f_9610_bd_m_9610_2_n */
    0x562b01d10acf47baULL, /* hd_m_9389_hd_m_9380_n */
    0x562c0bd02acc0457ULL, /* hd_m_9340_n */
    0x5668feabf8531b96ULL, /* am_m_9230_am_f_9230_n */
    0x56d8565f8032ffb0ULL, /* bd_m_9440_1_n */
    0x57546852c0011fb3ULL, /* bd_f_9280_bd_m_9280_2_n */
    0x58752cf0b6b3b489ULL, /* bd_m_9379_bd_m_9372_2_n */
    0x58fd2a70f2ed1d60ULL, /* bd_a_9600_fur_n */
    0x593f34dd6f7436f9ULL, /* lg_m_9570_n */
    0x595319664f7d7abbULL, /* am_f_9710_n */
    0x5975839273d45af6ULL, /* lg_m_9389_lg_m_9380_n */
    0x5a19086065845b12ULL, /* lg_f_9500_lg_m_9500_n */
    0x5a8a11467db2fc72ULL, /* hd_a_2870_n */
    0x5b2e015e74d6d826ULL, /* bd_f_2550_bd_m_2550_n */
    0x5b8702bdb2d16fc7ULL, /* hd_m_2550_n */
    0x5c7e65322cb6b7d2ULL, /* hd_m_9710_hd_f_9710_n */
    0x5c9e94b30fc66572ULL, /* hd_f_9440_hd_m_9440_n */
    0x5d09bdac7a68818bULL, /* bd_m_9341_2_n */
    0x5d7a52d02e8c8bfcULL, /* hd_m_9341_n */
    0x5e4fb058eb8db3d1ULL, /* bd_m_body_m_n */
    0x5ea1504f17f467ceULL, /* am_a_9200_n */
    0x5eab7352d8265f9fULL, /* am_m_9320_n */
    0x5eaca10ab6e9d202ULL, /* lg_f_9372_n */
    0x5ed612bf55ffaea9ULL, /* am_f_9521_n */
    0x5f38c49c711ed25dULL, /* hd_f_9710_n */
    0x601e8cdf476df62aULL, /* lg_m_9360_n */
    0x604ecee664fcb00aULL, /* bd_a_9200_1_n */
    0x6146087656094e2aULL, /* hd_m_9521_hd_f_9521_n */
    0x622708b480dcc547ULL, /* am_a_8200_n */
    0x62df1015f5da86beULL, /* bd_a_9509_bd_a_9500_1_n */
    0x63b1230ec31b8a11ULL, /* lg_f_9529_lg_f_9521_n */
    0x63ca969020287e03ULL, /* hd_a_9560_n */
    0x63d3a7f08633d871ULL, /* bd_m_9740_1_n */
    0x63e830d6d2980a7eULL, /* bd_a_8200_2_n */
    0x63ec7f6ab0b9005dULL, /* am_m_6200_n */
    0x63ed5ecaed26ea16ULL, /* bd_m_9510_1_n */
    0x64ae24f63f67051eULL, /* hd_a_9101_n */
    0x65c92ebbc188a2fcULL, /* bd_m_9380_2_n */
    0x65fa9cc4a4211659ULL, /* bd_f_9420_1_n */
    0x66709c7e9fa01b66ULL, /* lg_m_9710_lg_f_9710_n */
    0x669289816b3b7d44ULL, /* bd_m_9350_1_n */
    0x66b08fe937aed6ceULL, /* hd_m_9530_n */
    0x6745bcba154c535cULL, /* bd_a_9560_2_n */
    0x679d19e668bd3e7bULL, /* bd_a_9200_2_n */
    0x68212f1e315eb736ULL, /* am_m_9420_n */
    0x68899830023e0878ULL, /* bd_f_9521_2_n */
    0x690220fc11dccbb6ULL, /* lg_m_9349_lg_m_9340_n */
    0x6937a9818ae23527ULL, /* lg_m_9200_n */
    0x6960ac0abd8d1cb3ULL, /* lg_f_9371_n */
    0x696c2f47c1956169ULL, /* bd_m_9300_1_n */
    0x698b39bf5ca4dbeeULL, /* am_f_9520_n */
    0x69f52412b81fc812ULL, /* am_f_9510_am_m_9510_n */
    0x69fdbec95edd9374ULL, /* bd_a_9630_2_n */
    0x6a2d5b15f99b152fULL, /* bd_a_9509_bd_a_9500_2_n */
    0x6b1e4174986c55f6ULL, /* am_f_9610_am_m_9610_n */
    0x6bb40a71faf95b56ULL, /* lg_f_2560_lg_m_2560_n */
    0x6be1b21a722f0632ULL, /* am_f_9300_n */
    0x6bf2e8dbdf6d96baULL, /* bd_m_9349_bd_m_9340_1_n */
    0x6c1689caf1a298e7ULL, /* bd_m_9510_2_n */
    0x6c5542dc76ea2b3cULL, /* bd_a_underwear_n */
    0x6d39d94be57405ebULL, /* am_f_9620_n */
    0x6e5bc0ece17894bbULL, /* lg_m_9550_n */
    0x6e9d7bd6d93d74efULL, /* bd_a_8200_1_n */
    0x6f2a50a05f3aec34ULL, /* lg_m_2550_n */
    0x6f870bc07360f4f3ULL, /* am_m_9540_n */
    0x704e981b3f83c686ULL, /* bd_f_9300_1_n */
    0x70703c76e5adbe32ULL, /* am_m_2520_n */
    0x70822b06f0b9f842ULL, /* am_f_9260_am_m_9260_n */
    0x7189d30ac208c4b8ULL, /* lg_f_9370_n */
    0x722ea81405e2beb5ULL, /* bd_m_9220_2_n */
    0x724f72e11b34424eULL, /* bd_f_body_n */
    0x72a185f0c594ab84ULL, /* bd_m_9379_bd_m_9372_1_n */
    0x73bd8cdad0acd312ULL, /* hd_f_9540_hd_m_9540_n */
    0x73f2cb1cfbe21271ULL, /* bd_a_9310_2_n */
    0x74eaf431ceaed2feULL, /* lg_f_2520_lg_m_2520_n */
    0x75ea3d44fcfc5a5aULL, /* am_f_9720_am_m_9720_n */
    0x7613f8eeba2a0e4cULL, /* lg_m_9380_n */
    0x7615b93cc15eb6e1ULL, /* hd_a_2570_n */
    0x76a7f3dbe612946bULL, /* bd_m_9349_bd_m_9340_2_n */
    0x76c6eff3620933d7ULL, /* hd_m_9450_n */
    0x773e88c6c5ba8c5aULL, /* lg_f_9390_lg_m_9390_n */
    0x777ed32aecc3be51ULL, /* bd_m_body_n */
    0x779cc31b43441e97ULL, /* bd_f_9300_2_n */
    0x779dc2848b224b55ULL, /* hd_a_9250_n */
    0x783c67d22fd0696eULL, /* hd_m_9260_n */
    0x78d00b9c0bde5abeULL, /* lg_f_2550_lg_m_2550_n */
    0x7aace82ae891975eULL, /* lg_m_9289_lg_m_9280_n */
    0x7b4c5e1113f16e86ULL, /* hd_a_9130_n */
    0x7baa4bdab18f7000ULL, /* am_m_9530_n */
    0x7c69d5a051175886ULL, /* bd_m_2520_1_n */
    0x7d063898509dd89aULL, /* bd_m_9440_2_l_n */
    0x7d14f93f74f0a8dfULL, /* lg_m_9280_n */
    0x7d2ebacce7329606ULL, /* lg_f_9550_lg_m_9550_n */
    0x7d3d7b2da362a838ULL, /* am_m_9440_n */
    0x7d9885c74dd8f6d9ULL, /* bd_a_9490_1_n */
    0x7dff20f0951352ccULL, /* bd_m_9740_2_n */
    0x7e42834605fc80b4ULL, /* am_m_9260_n */
    0x7e7cc57bb7d528b2ULL, /* bd_m_9540_2_n */
    0x7f4d35c4b248d654ULL, /* bd_f_9420_2_n */
    0x7ff48a48309815f2ULL, /* lg_m_9230_lg_f_9230_n */
    0x808fec023a2bc8bbULL, /* am_a_9290_n */
    0x80d8e79d88506a51ULL, /* hd_m_9320_n */
    0x812ca862e44e79f2ULL, /* lg_f_2570_lg_m_2570_n */
    0x819d0fd124ee2623ULL, /* am_m_9610_n */
    0x81e48847cf0310e4ULL, /* bd_m_9300_2_n */
    0x820d5b23cee9eafbULL, /* lg_f_9520_n */
    0x824280d2776f1febULL, /* lg_m_9460_n */
    0x827a42d542b427f9ULL, /* hd_a_5320_n */
    0x83633a051632d97eULL, /* lg_f_9210_lg_m_9210_n */
    0x83b800a054d7b097ULL, /* bd_m_2520_2_n */
    0x84311472c0f4726aULL, /* bd_a_9290_2_n */
    0x84f2107bbaddc663ULL, /* bd_m_9540_1_n */
    0x860449489f320a49ULL, /* am_f_9460_n */
    0x866d89a31f97eadeULL, /* hd_f_9280_hd_m_9280_n */
    0x86d2bcafd36898d6ULL, /* lg_m_2570_n */
    0x876857cef04933e1ULL, /* bd_m_2550_n */
    0x881b7246c5598ce2ULL, /* bd_a_9500_2_n */
    0x887e4f2034f121d9ULL, /* am_m_9340_n */
    0x88d2572218f9ec2aULL, /* am_a_9509_am_a_9500_n */
    0x88f753d40ae7cf2dULL, /* am_a_9630_n */
    0x896241ef80a9e502ULL, /* am_m_9389_am_m_9380_n */
    0x89f168aca438b864ULL, /* lg_m_9300_n */
    0x8bbfe18e545abbbdULL, /* bd_f_9341_1_n */
    0x8c354d168ec7d7caULL, /* lg_m_9500_n */
    0x8c5a211414c23910ULL, /* bd_m_9220_1_n */
    0x8cc26223d58ee1e0ULL, /* lg_f_9521_n */
    0x8d0c1412d9e07ef3ULL, /* am_a_9470_n */
    0x8d5e822b0e513aa2ULL, /* lg_f_9220_lg_m_9220_n */
    0x8dd958365e38e05eULL, /* bd_f_9400_bd_m_9400_1_n */
    0x8df0a8c8fcbd4702ULL, /* am_f_5320_am_m_5320_n */
    0x8e177795318c451aULL, /* bd_a_9600_n */
    0x8e1e441d0ac18cccULL, /* bd_a_9310_1_n */
    0x8e40d1a72824cebdULL, /* bd_a_9570_2_n */
    0x8ee55f72c79829dbULL, /* bd_a_9290_1_n */
    0x8f3b41c72d63e3aaULL, /* hd_m_9372_n */
    0x8f9000ca0eb3f1afULL, /* lg_m_2520_n */
    0x8fc3dc98bc4b589aULL, /* lg_f_9310_lg_m_9310_n */
    0x903f1bd0c33b913eULL, /* bd_f_2560_bd_m_2560_1_n */
    0x90a67620396b16deULL, /* am_m_9341_n */
    0x90c6d7ea2393cdc2ULL, /* am_m_9510_n */
    0x90cb7c0d8c2bc8c3ULL, /* am_m_5320_n */
    0x918aa6e2ea462709ULL, /* lg_f_9320_n */
    0x91ef83347181ca33ULL, /* lg_f_9380_n */
    0x9224b72df945f438ULL, /* lg_f_9460_n */
    0x92d07d46cbfe8a93ULL, /* bd_a_9500_1_n */
    0x92eb7e3f623ca27fULL, /* hd_a_9630_n */
    0x934eb3fa94498f7eULL, /* lg_m_9509_lg_m_9500_n */
    0x937f897fa5197272ULL, /* lg_f_9400_lg_m_9400_n */
    0x94db8de7ce41d7acULL, /* am_f_9360_n */
    0x951b53acf99aedf3ULL, /* hd_m_9300_n */
    0x953a830157bf1b4eULL, /* hd_a_1000_n */
    0x95703bc58fe7f375ULL, /* am_m_9740_n */
    0x957f9f27c098d5f2ULL, /* bd_f_9270_bd_m_9270_1_n */
    0x968a8cc73126251bULL, /* hd_m_9371_n */
    0x96eb1ec75c00b6d4ULL, /* bd_a_9490_2_n */
    0x9727d368c7885e17ULL, /* am_a_2570_n */
    0x973a95409902b47aULL, /* lg_m_9521_lg_f_9521_n */
    0x97552e803782a1c6ULL, /* am_f_9280_am_m_9280_n */
    0x975b7db27f271999ULL, /* am_f_9529_am_f_9521_n */
    0x975e0ce1e971caadULL, /* lg_m_9400_n */
    0x978d66d0c6fc1fafULL, /* bd_f_2560_bd_m_2560_2_n */
    0x988ea33664de4acfULL, /* bd_f_9400_bd_m_9400_2_n */
    0x996a184441c08facULL, /* am_m_2540_n */
    0x996c3349b486e436ULL, /* bd_m_9280_1_n */
    0x99bf0df569cb7d34ULL, /* am_m_9379_am_m_9372_n */
    0x9a07e6d64abfb669ULL, /* lg_m_9372_n */
    0x9a7b60af9590767dULL, /* hd_a_8200_n */
    0x9d3e4f05cba00a89ULL, /* am_m_9410_n */
    0x9e3544325fb894c5ULL, /* am_a_9560_n */
    0x9e8f16d9bd3de237ULL, /* bd_f_9480_n */
    0x9f2218238b6db023ULL, /* am_a_9500_n */
    0x9f2ec56925dd256eULL, /* lg_m_9250_n */
    0x9f4be35e31fc75b2ULL, /* bd_f_9371_1_n */
    0x9f5aea27c6849ca3ULL, /* bd_f_9270_bd_m_9270_2_n */
    0x9fd461758d3195eaULL, /* hd_m_9289_hd_m_9280_n */
    0x9fe15e49b78f4b87ULL, /* bd_m_9280_2_n */
    0xa01c4cfc24becfbaULL, /* lg_m_9430_n */
    0xa0cfa83b0aed5621ULL, /* am_m_9371_n */
    0xa1045467b86be552ULL, /* bd_m_9720_2_n */
    0xa13e93c737c96900ULL, /* hd_m_9370_n */
    0xa173cbc0bf8d99d1ULL, /* hd_m_9430_n */
    0xa1ff627f04bbec82ULL, /* lg_f_9540_lg_m_9540_n */
    0xa2b1fb1e51e827e2ULL, /* lg_f_9440_lg_m_9440_n */
    0xa3de2a37bd570cf6ULL, /* bd_m_body_m_l_n */
    0xa5125a8e62824558ULL, /* bd_f_9341_2_n */
    0xa53ddb8b5de348b6ULL, /* hd_f_9260_hd_m_9260_n */
    0xa5c12e5e35051363ULL, /* bd_f_9371_2_n */
    0xa744af3b0df58046ULL, /* am_m_9370_n */
    0xa77ff2f25d0208cbULL, /* lg_f_9300_n */
    0xa7934aa7364c5858ULL, /* bd_a_9570_1_n */
    0xa92c9f67bce61783ULL, /* bd_m_9720_1_n */
    0xa9dac2350d4317eeULL, /* bd_f_9372_2_n */
    0xa9ec7875b72998f2ULL, /* lg_f_2870_lg_m_2870_n */
    0xaaabc36ae51b2c8bULL, /* hd_m_9380_n */
    0xaab32c58cdb4904eULL, /* hd_m_9230_hd_f_9230_n */
    0xab0c58763e2dec1aULL, /* hd_f_9610_hd_m_9610_n */
    0xab3218d6546d149fULL, /* lg_m_9370_n */
    0xaeb080a185678cb2ULL, /* hd_f_9400_hd_m_9400_n */
    0xaedcfe872d25ac4cULL, /* hd_a_9550_n */
    0xb04fcd35104b48dfULL, /* bd_f_9372_1_n */
    0xb0b28517ba5edc60ULL, /* hd_a_2780_n */
    0xb10258d470c61ef6ULL, /* bd_f_5320_bd_m_5320_1_n */
    0xb102aef8793a7252ULL, /* lg_f_9490_lg_m_9490_n */
    0xb13208373ba64412ULL, /* am_m_5370_n */
    0xb209236e811bdcbeULL, /* bd_a_9250_2_n */
    0xb2803fd6582d65e4ULL, /* lg_m_9371_n */
    0xb3278b15a072767dULL, /* bd_m_9410_1_n */
    0xb3339b153e5bec4bULL, /* am_m_9430_n */
    0xb376a3ecf31f4d1aULL, /* bd_f_9620_2_n */
    0xb5705b0e6207c8caULL, /* hd_a_5350_n */
    0xb5842eff8d04f368ULL, /* hd_a_5370_n */
    0xb5b637d030d81d73ULL, /* hd_m_9410_n */
    0xb603692f15e54c0eULL, /* bd_a_2570_2_n */
    0xb65b1fd6a9cdc6c2ULL, /* hd_m_9350_n */
    0xb73e3938d914bba2ULL, /* hd_f_9360_n */
    0xb79b42a16872b825ULL, /* hd_a_9500_n */
    0xb81863a02a543808ULL, /* hd_m_9280_n */
    0xb8b07a965e7b0b7eULL, /* am_m_9480_am_f_9480_n */
    0xb948213b185b3bfcULL, /* am_m_9372_n */
    0xb9576e6e84dc6b2fULL, /* bd_a_9250_1_n */
    0xb9b3b13584e4f716ULL, /* lg_f_9270_lg_m_9270_n */
    0xb9c373bb7bbf8f4aULL, /* bd_f_9340_1_n */
    0xba3cdc976c3857a9ULL, /* lg_m_2540_n */
    0xbac64be426d38692ULL, /* bd_m_9230_bd_f_9230_1_n */
    0xbadd83d476b1af47ULL, /* bd_f_5320_bd_m_5320_2_n */
    0xbb2e74b0ec95061eULL, /* lg_f_9610_lg_m_9610_n */
    0xbba5a30bd1c4fcaaULL, /* bd_f_9220_bd_m_9220_1_n */
    0xbbbb0fec8b1877e2ULL, /* lg_f_9470_lg_m_9470_n */
    0xbc1db9f28663396aULL, /* am_m_9710_am_f_9710_n */
    0xbc65b0fc9275bcd2ULL, /* bd_f_9430_bd_m_9430_1_n */
    0xbcc266fe4b22f36eULL, /* bd_f_9720_bd_m_9720_1_n */
    0xbd22865ac9ad562eULL, /* lg_m_8200_n */
    0xbdd5a44d397696f2ULL, /* am_a_9550_n */
    0xbe2aaeecf9c297cbULL, /* bd_f_9620_1_n */
    0xbe2b742f1a5f117fULL, /* bd_a_2570_1_n */
    0xbfbcfc46c2aadfa3ULL, /* am_a_9210_n */
    0xc06a6a0e702eed3eULL, /* bd_m_9480_bd_f_9480_n */
    0xc07ba99cee806806ULL, /* lg_m_9309_lg_m_9300_n */
    0xc07ff2c4b5428776ULL, /* bd_f_9540_bd_m_9540_1_n */
    0xc0da5f89296b40d9ULL, /* bd_m_9260_2_n */
    0xc1d977ea6f069880ULL, /* hd_m_9460_n */
    0xc210b56b2a8799b6ULL, /* lg_f_9250_lg_m_9250_n */
    0xc2b12a7b98212c9eULL, /* hd_a_9220_n */
    0xc2c3fe6cc31ca43cULL, /* hd_a_9200_n */
    0xc2ee96e42b4db8c3ULL, /* bd_m_9230_bd_f_9230_2_n */
    0xc2f4ee0bd5873e1bULL, /* bd_f_9220_bd_m_9220_2_n */
    0xc3162217932554b6ULL, /* lg_f_5370_lg_m_5370_n */
    0xc33771fe4e2b245fULL, /* bd_f_9720_bd_m_9720_2_n */
    0xc38298d16606c206ULL, /* lg_f_9630_lg_m_9630_n */
    0xc478bebb8264f9bbULL, /* bd_f_9340_2_n */
    0xc48dfbfc96efef03ULL, /* bd_f_9430_bd_m_9430_2_n */
    0xc551c982e2ad80c6ULL, /* bd_a_9550_1_n */
    0xc55abf1c0a536b9aULL, /* lg_f_9350_n */
    0xc5deb1408d53214aULL, /* bd_m_2560_1_n */
    0xc65f0a0820ef105bULL, /* hd_a_9120_n */
    0xc6df67efb6b03345ULL, /* hd_m_9540_n */
    0xc740e5b704782ac3ULL, /* am_f_9530_n */
    0xc7e162358e5a479bULL, /* hd_f_9521_n */
    0xc845e420f02e1bdcULL, /* am_a_9310_n */
    0xc8d341181be42028ULL, /* hd_m_0001_m_n */
    0xc9550f3d12f9ec29ULL, /* am_m_9270_n */
    0xc990a9059f19d4fcULL, /* hd_a_2920_n */
    0xc9a48dff4a15a6e9ULL, /* bd_m_6200_1_n */
    0xca5b1dc4bb2e17c7ULL, /* bd_f_9540_bd_m_9540_2_n */
    0xcac928c9824335b4ULL, /* lg_m_9450_n */
    0xcc7a0415ae9a0018ULL, /* bd_m_9410_2_n */
    0xccdda95e4f9670aaULL, /* bd_m_9309_bd_m_9300_1_n */
    0xccec465ecaa7da9fULL, /* bd_m_9440_1_l_n */
    0xcd9a480663918859ULL, /* bd_a_9390_2_n */
    0xcdae110824b0de40ULL, /* hd_a_9121_n */
    0xcead7b2ff2684a59ULL, /* hd_m_9610_n */
    0xcec26cdeda6a6aceULL, /* am_m_9289_am_m_9280_n */
    0xcf8a89d2ddfd8da6ULL, /* hd_a_3460_n */
    0xd006f482e952b4d7ULL, /* bd_a_9550_2_n */
    0xd05c5a6330354b26ULL, /* am_f_9430_am_m_9430_n */
    0xd093fc4093f88bbbULL, /* bd_m_2560_2_n */
    0xd0e8a4603e44a862ULL, /* bd_a_9210_2_n */
    0xd10e673eebad4f1aULL, /* am_m_9460_n */
    0xd12457868ed49986ULL, /* bd_m_9520_bd_f_9520_1_n */
    0xd295693594fd8b80ULL, /* hd_f_9520_n */
    0xd42cf45e5358b21bULL, /* bd_m_9309_bd_m_9300_2_n */
    0xd494fd1feaa2e77cULL, /* hd_m_9379_hd_m_9372_n */
    0xd4cffb99bb8cf13eULL, /* lg_f_9620_n */
    0xd504f4a3b137d6d9ULL, /* lg_m_9310_n */
    0xd511ae51305195caULL, /* bd_f_9260_bd_m_9260_1_n */
    0xd54cdd1f5aa95806ULL, /* hd_f_9341_hd_m_9341_n */
    0xd5b8e7a96fb84492ULL, /* am_f_9540_am_m_9540_n */
    0xd5f213c9d7a3b843ULL, /* hd_m_9270_n */
    0xd6458d434061bf8cULL, /* hd_a_2560_n */
    0xd6af1e67040b9772ULL, /* lg_f_9430_lg_m_9430_n */
    0xd6f603f9e10af642ULL, /* hd_m_9440_n */
    0xd7298df3acd89c3aULL, /* lg_m_9480_lg_f_9480_n */
    0xd75b8cfec6c2377dULL, /* lg_m_9530_n */
    0xd87282869294f197ULL, /* bd_m_9520_bd_f_9520_2_n */
    0xd8882d2c085c41a3ULL, /* hd_f_9460_n */
    0xd90029095b315e6aULL, /* bd_f_9520_1_n */
    0xda2cf889379300d4ULL, /* bd_m_9260_1_n */
    0xda96cafa061abedeULL, /* lg_m_9620_lg_f_9620_n */
    0xdb9daf6044e9a613ULL, /* bd_a_9210_1_n */
    0xdbf6c885d2f95782ULL, /* bd_m_9420_1_n */
    0xdc1c79cdf1118dfaULL, /* am_f_9740_am_m_9740_n */
    0xdcfd53dedaf669b4ULL, /* am_a_9390_n */
    0xde70887070b3c672ULL, /* am_a_9240_n */
    0xded27cd28c7ee96eULL, /* lg_f_9290_lg_m_9290_n */
    0xdf1f1902252b39f9ULL, /* bd_m_9400_2_n */
    0xdf30c521a6a305d1ULL, /* bd_m_9372_2_n */
    0xdfac51c170969072ULL, /* lg_f_1000_lg_m_1000_n */
    0xdfc6f95136f7003bULL, /* bd_f_9260_bd_m_9260_2_n */
    0xdfef39dedb412c81ULL, /* am_f_9370_n */
    0xe111150635df7ddcULL, /* hd_f_9300_n */
    0xe15d52f912ca7a49ULL, /* am_f_9341_n */
    0xe1dc53364415e110ULL, /* am_a_2870_n */
    0xe202f622f77074d3ULL, /* lg_m_5370_n */
    0xe21ce6ff57835664ULL, /* bd_m_6200_2_n */
    0xe27194d8f670e256ULL, /* lg_m_9470_n */
    0xe32200b06fa9d5ddULL, /* hd_a_2660_n */
    0xe3b4740961d515dbULL, /* bd_f_9520_2_n */
    0xe47dc43b4ebfe481ULL, /* am_m_2550_n */
    0xe52eca73cc671a02ULL, /* hd_a_4160_n */
    0xe5c1a601c7024dabULL, /* hd_a_9700_n */
    0xe6aad385d99ca233ULL, /* bd_m_9420_2_n */
    0xe6ece10671b94854ULL, /* bd_a_9390_1_n */
    0xe7c805515bac4ce1ULL, /* lg_m_9260_n */
    0xe81740dedfbaeb26ULL, /* am_f_9371_n */
    0xe86dcd57d4a372d0ULL, /* lg_m_9700_n */
    0xe8809fe2d21dd0aeULL, /* bd_a_9730_1_n */
    0xe986d02d66d99d0aULL, /* bd_m_9360_2_n */
    0xe9c96bf1fbfdb181ULL, /* bd_f_9360_2_n */
    0xea04be7f9cf6d396ULL, /* am_f_9440_am_m_9440_n */
    0xeb2ed8f331bc3b2fULL, /* lg_m_9420_n */
    0xeb6c6a339b42e8a6ULL, /* hd_a_9509_hd_a_9500_n */
    0xebe224a986a40b0eULL, /* bd_f_9440_bd_m_9440_1_n */
    0xec1279f9196fa78eULL, /* am_f_9340_n */
    0xec5b592073560ef5ULL, /* hd_f_9350_n */
    0xecf28f67c55b9242ULL, /* bd_m_5320_2_n */
    0xee0766db4dd9435dULL, /* bd_m_9270_1_n */
    0xef74732fdaf4b101ULL, /* hd_f_9372_n */
    0xf0c582df37ce202eULL, /* am_m_9620_am_f_9620_n */
    0xf0d51b2d6a9a2b7bULL, /* bd_m_9360_1_n */
    0xf2585b321af4d51bULL, /* am_m_9360_n */
    0xf25caae2d80add9fULL, /* bd_a_9730_2_n */
    0xf29e6886cf419802ULL, /* bd_f_9740_bd_m_9740_1_n */
    0xf2cc6bdee6601f37ULL, /* am_f_9372_n */
    0xf32e46370c3b5928ULL, /* hd_a_9600_n */
    0xf3cb54be8a3c3546ULL, /* lg_m_9320_n */
    0xf40a2fa98b1dd07fULL, /* bd_f_9440_bd_m_9440_2_n */
    0xf4296c60af0e2609ULL, /* hd_f_9620_n */
    0xf4419a67c91d66f3ULL, /* bd_m_5320_1_n */
    0xf4c1f5c504d51766ULL, /* lg_f_5320_lg_m_5320_n */
    0xf5c884c420a7cbb4ULL, /* hd_f_9380_n */
    0xf68cf7ed0ee0e7c3ULL, /* hd_m_9740_n */
    0xf757e76f468bd382ULL, /* am_a_2560_n */
    0xf7a93e21b410ebacULL, /* bd_m_9372_1_n */
    0xf7c0215a5237f1d2ULL, /* hd_f_9430_hd_m_9430_n */
    0xf8427e80c35d2a42ULL, /* bd_f_9530_2_n */
    0xf8709a133c9953daULL, /* bd_m_9389_bd_m_9380_1_n */
    0xf871b2023352f9f4ULL, /* bd_m_9400_1_n */
    0xf9a249c600253932ULL, /* bd_a_9470_2_n */
    0xfcb0196745edc012ULL, /* lg_m_9720_n */
    0xfce47160cdb07443ULL, /* lg_m_9240_n */
    0xfd527386d5e4e2b3ULL, /* bd_f_9740_bd_m_9740_2_n */
    0xfd6d430c4aa19694ULL, /* am_m_9400_n */
    0xfdf6e7b5d130b265ULL, /* hd_a_3322_n */
    0xfea95a5e1499e6eaULL, /* hd_m_9620_hd_f_9620_n */
    0xff918980c71efef3ULL, /* bd_f_9530_1_n */
    0xffa0c09c05700af6ULL, /* bd_f_9230_1_n */
    0xffbfa513405b288bULL, /* bd_m_9389_bd_m_9380_2_n */
};

static const u64 k_diffuse_names[] = {
    0x003edc3c61d5b42bULL, /* lg_f_5370_lg_m_5370 */
    0x00b515c367a88e31ULL, /* hd_a_9550 */
    0x00b89bc367aba7baULL, /* hd_a_9560 */
    0x00bf87c367b1a46cULL, /* hd_a_9500 */
    0x019dea2c07dd144aULL, /* am_m_9540 */
    0x01a85c2c07e62a85ULL, /* am_m_9530 */
    0x01aee82c07eb8417ULL, /* am_m_9510 */
    0x01b3953ef3405cb0ULL, /* bd_m_9300_1 */
    0x01b3983ef34061c9ULL, /* bd_m_9300_2 */
    0x01fa980ab8aae51bULL, /* lg_f_9550_lg_m_9550 */
    0x023e86468b8e3f83ULL, /* bd_m_9710_bd_f_9710_1 */
    0x023e87468b8e4136ULL, /* bd_m_9710_bd_f_9710_2 */
    0x0284be4d34f2f9d3ULL, /* lg_f_9560_lg_m_9560 */
    0x03662a75ea919557ULL, /* lg_f_9260_lg_m_9260 */
    0x04efcec4cd60a667ULL, /* lg_f_2570_lg_m_2570 */
    0x061b7a5a0cc9cfdfULL, /* bd_f_9710_2 */
    0x061b7b5a0cc9d192ULL, /* bd_f_9710_1 */
    0x07177b1cc47f41d3ULL, /* bd_a_8200_2 */
    0x07177c1cc47f4386ULL, /* bd_a_8200_1 */
    0x076811c4e36ce91fULL, /* bd_f_9220_bd_m_9220_1 */
    0x076812c4e36cead2ULL, /* bd_f_9220_bd_m_9220_2 */
    0x07cffd6b2cb194ffULL, /* bd_m_9521_bd_f_9521_1 */
    0x07cffe6b2cb196b2ULL, /* bd_m_9521_bd_f_9521_2 */
    0x07e098b4b30a43c0ULL, /* bd_a_9490_1 */
    0x07e09bb4b30a48d9ULL, /* bd_a_9490_2 */
    0x07fa0359694cdde3ULL, /* am_f_5370_am_m_5370 */
    0x084f157c8ac84e6bULL, /* lg_m_9389_lg_m_9380 */
    0x086daa148a0a03bbULL, /* lg_f_5320_lg_m_5320 */
    0x095c0a0aaf526f4fULL, /* hd_m_9389_hd_m_9380 */
    0x09631719200afad7ULL, /* bd_m_9420_1 */
    0x09631819200afc8aULL, /* bd_m_9420_2 */
    0x09b72cc36cdc0bc6ULL, /* hd_a_9490 */
    0x09bdb8c36ce16558ULL, /* hd_a_9470 */
    0x0a51f72c0cce5f90ULL, /* am_m_9410 */
    0x0a54fd2c0cd09f99ULL, /* am_m_9400 */
    0x0a5f8f2c0cd9ec34ULL, /* am_m_9450 */
    0x0a62952c0cdc2c3dULL, /* am_m_9440 */
    0x0a69812c0ce228efULL, /* am_m_9460 */
    0x0bd3ac6f2051d557ULL, /* lg_f_9740_lg_m_9740 */
    0x0be9f79f25cef827ULL, /* hd_f_9400_hd_m_9400 */
    0x0c5a84de310d8b64ULL, /* bd_f_9521_1 */
    0x0c5a87de310d907dULL, /* bd_f_9521_2 */
    0x10565b76fc8d9c9bULL, /* bd_f_2520_bd_m_2520_1 */
    0x10565c76fc8d9e4eULL, /* bd_f_2520_bd_m_2520_2 */
    0x11f63c978b88ed93ULL, /* bd_f_2540_bd_m_2540 */
    0x127bd7c371db23b9ULL, /* hd_a_9730 */
    0x127f5dc371de3d42ULL, /* hd_a_9700 */
    0x12d477ce8ff66ec7ULL, /* am_f_9540_am_m_9540 */
    0x135da02c120976c0ULL, /* am_m_9340 */
    0x135da12c12097873ULL, /* am_m_9341 */
    0x1361262c120c9049ULL, /* am_m_9350 */
    0x1364ac2c120fa9d2ULL, /* am_m_9360 */
    0x1368312c1212c1a8ULL, /* am_m_9371 */
    0x1368322c1212c35bULL, /* am_m_9370 */
    0x136b382c12150364ULL, /* am_m_9300 */
    0x1386a82c122c896cULL, /* am_m_9380 */
    0x13a99dde34cf77dfULL, /* bd_f_9520_1 */
    0x13a99ede34cf7992ULL, /* bd_f_9520_2 */
    0x1604dc6d4b86f9d7ULL, /* lg_f_9240_lg_m_9240 */
    0x1638132e0d51bd87ULL, /* lg_f_2540_lg_m_2540 */
    0x1ad91da62036966fULL, /* lg_f_9310_lg_m_9310 */
    0x1b17fbca50f5f598ULL, /* bd_a_9310_2 */
    0x1b17feca50f5fab1ULL, /* bd_a_9310_1 */
    0x1b9598c37722a10dULL, /* hd_a_9600 */
    0x1b98fec377258436ULL, /* hd_a_9630 */
    0x1c69c92c17456770ULL, /* am_m_9270 */
    0x1c6d4f2c174880f9ULL, /* am_m_9260 */
    0x1c9ce32c1770ed37ULL, /* am_m_9280 */
    0x1d060f67201eb268ULL, /* hd_a_2570 */
    0x1d0915672020f271ULL, /* hd_a_2560 */
    0x1e1c0ee70776882bULL, /* hd_f_9260_hd_m_9260 */
    0x1ea054d5a1af3334ULL, /* bd_m_9530_2 */
    0x1ea057d5a1af384dULL, /* bd_m_9530_1 */
    0x1ef9e6b184cb26ebULL, /* bd_f_9540_bd_m_9540_1 */
    0x1ef9e7b184cb289eULL, /* bd_f_9540_bd_m_9540_2 */
    0x2385515e48963d3bULL, /* lg_f_8200_lg_m_8200 */
    0x24a930333bfa86c3ULL, /* hd_m_9230_hd_f_9230 */
    0x250bbc0e68504c13ULL, /* bd_a_9250_2 */
    0x250bbd0e68504dc6ULL, /* bd_a_9250_1 */
    0x25945015dcad7dc0ULL, /* bd_m_9260_2 */
    0x25945315dcad82d9ULL, /* bd_m_9260_1 */
    0x25bec542f7d3cc9fULL, /* am_a_9509_am_a_9500 */
    0x25cdc06725200a64ULL, /* hd_a_2660 */
    0x273c74a0fa2e90f7ULL, /* bd_m_9610_2 */
    0x273c75a0fa2e92aaULL, /* bd_m_9610_1 */
    0x28a0e5c74a89c277ULL, /* lg_f_9200_lg_m_9200 */
    0x2de3889f704521e3ULL, /* am_f_9400_am_m_9400 */
    0x2ef85f672a75c125ULL, /* hd_a_2780 */
    0x2f69bf742897212fULL, /* lg_f_9390_lg_m_9390 */
    0x300118e112524c57ULL, /* am_m_9389_am_m_9380 */
    0x30d8ef7eae2c1cafULL, /* bd_m_9389_bd_m_9380_1 */
    0x30d8f07eae2c1e62ULL, /* bd_m_9389_bd_m_9380_2 */
    0x32bfcfcc4a063497ULL, /* bd_m_5320_2 */
    0x32bfd0cc4a06364aULL, /* bd_m_5320_1 */
    0x345d79c6f7b1e0c7ULL, /* bd_m_9230_bd_f_9230_1 */
    0x345d7ac6f7b1e27aULL, /* bd_m_9230_bd_f_9230_2 */
    0x351e34268c10f350ULL, /* bd_m_9350_2 */
    0x351e37268c10f869ULL, /* bd_m_9350_1 */
    0x362308d1b667c8f2ULL, /* lg_f_9520 */
    0x362309d1b667caa5ULL, /* lg_f_9521 */
    0x36268ed1b66ae27bULL, /* lg_f_9530 */
    0x364ef4baf9acb8c4ULL, /* hd_a_8200 */
    0x37f4c8c358cc635bULL, /* lg_f_9730_lg_m_9730 */
    0x380b94672fb7e4e7ULL, /* hd_a_2870 */
    0x38ddde7918e2aac8ULL, /* am_m_2550 */
    0x38e1647918e5c451ULL, /* am_m_2540 */
    0x38e1c5853bcd821bULL, /* lg_m_9520_lg_f_9520 */
    0x38f5e87918f74da7ULL, /* am_m_2520 */
    0x396e69fc26095983ULL, /* bd_a_2570_2 */
    0x396e6afc26095b36ULL, /* bd_a_2570_1 */
    0x3b0f959565284b98ULL, /* bd_m_9740_1 */
    0x3b0f9895652850b1ULL, /* bd_m_9740_2 */
    0x3b1bc0ea5d89fe04ULL, /* bd_f_9341_1 */
    0x3b1bc3ea5d8a031dULL, /* bd_f_9341_2 */
    0x3bfa0f55c609b0e4ULL, /* am_m_6200 */
    0x3cae991857d67defULL, /* hd_f_9610_hd_m_9610 */
    0x3d78abd1ba2f0b99ULL, /* lg_f_9420 */
    0x3d8643d1ba3a983dULL, /* lg_f_9460 */
    0x3d89a9d1ba3d7b66ULL, /* lg_f_9450 */
    0x3d9aa7d1ba4beb33ULL, /* lg_f_9480 */
    0x3dfbed368cdbc737ULL, /* hd_f_9720_hd_m_9720 */
    0x3e5fdb5f72c5166fULL, /* bd_m_9440_2_l */
    0x3ecec2e6ba797bffULL, /* bd_m_2560_1 */
    0x3ecec3e6ba797db2ULL, /* bd_m_2560_2 */
    0x3f5039673370b7c1ULL, /* hd_a_2920 */
    0x4033b500bbbeb8c4ULL, /* bd_m_9410_1 */
    0x4033b800bbbebdddULL, /* bd_m_9410_2 */
    0x419bdda3abded5e3ULL, /* lg_m_8200 */
    0x4211fe35f99a54acULL, /* bd_m_9371_1 */
    0x42120135f99a59c5ULL, /* bd_m_9371_2 */
    0x421425857266671bULL, /* lg_m_9309_lg_m_9300 */
    0x430a40ff28f16893ULL, /* bd_m_9480_bd_f_9480 */
    0x43b9e4c41792d16bULL, /* am_f_9610_am_m_9610 */
    0x43dbb22a240cb887ULL, /* bd_f_9610_bd_m_9610_1 */
    0x43dbb32a240cba3aULL, /* bd_f_9610_bd_m_9610_2 */
    0x45d059ea642e39ffULL, /* bd_f_9340_1 */
    0x45d05aea642e3bb2ULL, /* bd_f_9340_2 */
    0x469958d1bf7c859fULL, /* lg_f_9710 */
    0x47c246be5e5b5d43ULL, /* am_m_9289_am_m_9280 */
    0x48188ef2121f178bULL, /* hd_f_9340_hd_m_9340 */
    0x48495098059888dbULL, /* am_f_9280_am_m_9280 */
    0x4a3a3735fe146847ULL, /* bd_m_9370_1 */
    0x4a3a3835fe1469faULL, /* bd_m_9370_2 */
    0x4b6f7ea36f194c63ULL, /* bd_a_9730_1 */
    0x4b6f7fa36f194e16ULL, /* bd_a_9730_2 */
    0x4d68967a15ae126aULL, /* lg_m_5370 */
    0x4d79947a15bc8237ULL, /* lg_m_5320 */
    0x4daea8c9fe2b6673ULL, /* hd_f_2550_hd_m_2550 */
    0x4fb2f9d1c4c3cc93ULL, /* lg_f_9620 */
    0x4fef1f8581501ccbULL, /* am_m_9230_am_f_9230 */
    0x50e7276104f0b390ULL, /* am_f_9450_l */
    0x513db3312cadc12eULL, /* bd_f_9480 */
    0x517372f5f80f427bULL, /* bd_m_5370_1 */
    0x517373f5f80f442eULL, /* bd_m_5370_2 */
    0x54f966c6e41f2147ULL, /* lg_f_9500_lg_m_9500 */
    0x553c3a4eb9010083ULL, /* bd_f_9440_bd_m_9440_1 */
    0x553c3b4eb9010236ULL, /* bd_f_9440_bd_m_9440_2 */
    0x5643edb7f587223fULL, /* bd_f_5370_bd_m_5370_1 */
    0x5643eeb7f58723f2ULL, /* bd_f_5370_bd_m_5370_2 */
    0x565df5503c43b96eULL, /* bd_m_9320_1 */
    0x56d8585f80330316ULL, /* bd_m_9440_1_l */
    0x57c29b8c697c8bc7ULL, /* am_m_9520_am_f_9520 */
    0x5a812828a954bc57ULL, /* bd_f_9280_bd_m_9280_1 */
    0x5a812928a954be0aULL, /* bd_f_9280_bd_m_9280_2 */
    0x5b025ef6034a6ebfULL, /* bd_a_9200_1 */
    0x5b025ff6034a7072ULL, /* bd_a_9200_2 */
    0x5c3e0236087abb38ULL, /* bd_m_9372_2 */
    0x5c3e0536087ac051ULL, /* bd_m_9372_1 */
    0x5cb9f89ff0f55653ULL, /* lg_f_2520_lg_m_2520 */
    0x5d526d681648a630ULL, /* bd_m_6200_1 */
    0x5d5270681648ab49ULL, /* bd_m_6200_2 */
    0x5da60e6652956fedULL, /* hd_f_9230 */
    0x5dc4319cde49e768ULL, /* bd_m_2550 */
    0x5dc7379cde4c2771ULL, /* bd_m_2540 */
    0x5e4fae58eb8db06bULL, /* bd_m_body_m_l */
    0x5e9afb865f00d740ULL, /* bd_f_9420_1 */
    0x5e9afe865f00dc59ULL, /* bd_f_9420_2 */
    0x6047304991c61d96ULL, /* hd_m_6200 */
    0x63ca100764ecbc9bULL, /* bd_m_9520_bd_f_9520_1 */
    0x63ca110764ecbe4eULL, /* bd_m_9520_bd_f_9520_2 */
    0x648220b53f2826d4ULL, /* bd_f_9529_bd_f_9521_1 */
    0x648223b53f282bedULL, /* bd_f_9529_bd_f_9521_2 */
    0x6623900562fc9a13ULL, /* lg_f_2550_lg_m_2550 */
    0x66a49f6657c5d3f9ULL, /* hd_f_9380 */
    0x66bfaf6657dcb6e1ULL, /* hd_f_9300 */
    0x66c2653c9c8167bbULL, /* lg_m_9710_lg_f_9710 */
    0x66c69b6657e2b393ULL, /* hd_f_9320 */
    0x66ca216657e5cd1cULL, /* hd_f_9350 */
    0x66d0ab6657eb2348ULL, /* hd_f_9372 */
    0x66d0ad6657eb26aeULL, /* hd_f_9370 */
    0x66d0ae6657eb2861ULL, /* hd_f_9371 */
    0x66d4336657ee4037ULL, /* hd_f_9360 */
    0x673a14bdd77f3427ULL, /* bd_m_9540_2 */
    0x673a15bdd77f35daULL, /* bd_m_9540_1 */
    0x68c872d1d2b7a810ULL, /* lg_f_9320 */
    0x68cefed1d2bd01a2ULL, /* lg_f_9300 */
    0x68d60ad1d2c334b4ULL, /* lg_f_9360 */
    0x68d90fd1d2c5730aULL, /* lg_f_9371 */
    0x68d910d1d2c574bdULL, /* lg_f_9370 */
    0x68dc76d1d2c857e6ULL, /* lg_f_9340 */
    0x68dc77d1d2c85999ULL, /* lg_f_9341 */
    0x68dffcd1d2cb716fULL, /* lg_f_9350 */
    0x68ea0ed1d2d3e48aULL, /* lg_f_9380 */
    0x68f12290e72887d7ULL, /* lg_f_9540_lg_m_9540 */
    0x698683d4473e125fULL, /* bd_m_9289_bd_m_9280_1 */
    0x698684d4473e1412ULL, /* bd_m_9289_bd_m_9280_2 */
    0x6bc75e00d244d077ULL, /* lg_f_9440_lg_m_9440 */
    0x6c4c821a8d67588bULL, /* lg_f_2560_lg_m_2560 */
    0x6dee124fc91b5c7fULL, /* bd_f_9260_bd_m_9260_1 */
    0x6dee134fc91b5e32ULL, /* bd_f_9260_bd_m_9260_2 */
    0x6ede9f771f40bd82ULL, /* am_f_9620 */
    0x6ee71413ce32d6ceULL, /* am_a_2570 */
    0x6eea1a13ce3516d7ULL, /* am_a_2560 */
    0x6f05f27f35135ad3ULL, /* am_m_9480_am_f_9480 */
    0x6fa5e965f3823f2fULL, /* am_f_9720_am_m_9720 */
    0x6fbada665d0a37c4ULL, /* hd_f_9480 */
    0x6fcbd8665d18a791ULL, /* hd_f_9450 */
    0x6fcf5e665d1bc11aULL, /* hd_f_9460 */
    0x6fdcd6665d27175eULL, /* hd_f_9420 */
    0x707b05468a039a1eULL, /* am_a_8200 */
    0x708aeaadbe0765f7ULL, /* hd_f_9600_hd_a_9600 */
    0x711e60a2571f6ae7ULL, /* lg_f_9400_lg_m_9400 */
    0x71e89fd1d8044896ULL, /* lg_f_9230 */
    0x724f71e11b34409bULL, /* bd_f_body_m */
    0x733cf42b060e5e3cULL, /* bd_m_9440_2 */
    0x733cf72b060e6355ULL, /* bd_m_9440_1 */
    0x74c55ad5df860c97ULL, /* bd_f_9530_2 */
    0x74c55bd5df860e4aULL, /* bd_f_9530_1 */
    0x7589af03e449d5b3ULL, /* hd_m_9520_hd_f_9520 */
    0x75a15d8af4cd8b37ULL, /* am_m_9309_am_m_9300 */
    0x772ef36660eb407cULL, /* hd_f_9530 */
    0x7732786660ee5852ULL, /* hd_f_9521 */
    0x7732796660ee5a05ULL, /* hd_f_9520 */
    0x777ed02aecc3b938ULL, /* bd_m_body_m */
    0x77ea4877247bd4b2ULL, /* am_f_9710 */
    0x7a69b8dd6ebd0693ULL, /* lg_f_9510_lg_m_9510 */
    0x7a9526b2752f264fULL, /* bd_m_9349_bd_m_9340_1 */
    0x7a9527b2752f2802ULL, /* bd_m_9349_bd_m_9340_2 */
    0x7aeac84a763d8b0fULL, /* lg_m_9521_lg_f_9521 */
    0x7c9fea1866014c5fULL, /* hd_m_9620_hd_f_9620 */
    0x7dade5b2b832dc07ULL, /* hd_f_9430_hd_m_9430 */
    0x7db6905ddfb4674fULL, /* am_m_9521_am_f_9521 */
    0x7f3ce5772840d750ULL, /* am_f_9460 */
    0x7f46f77728494a6bULL, /* am_f_9450 */
    0x7f4a7d77284c63f4ULL, /* am_f_9420 */
    0x7f6c79772869438eULL, /* am_f_9480 */
    0x7fdc146665d68f10ULL, /* hd_f_9620 */
    0x8031551d990f5bf4ULL, /* bd_m_9340_1 */
    0x8031581d990f610dULL, /* bd_m_9340_2 */
    0x810ec6def8cca077ULL, /* lg_f_9470_lg_m_9470 */
    0x83518d9889f13c47ULL, /* hd_f_2540_hd_m_2540 */
    0x83ef99eede2c7e6bULL, /* bd_f_9230_1 */
    0x83ef9aeede2c801eULL, /* bd_f_9230_2 */
    0x8583f5a2a2bd8e20ULL, /* hd_a_5320 */
    0x859513a2a2cc344dULL, /* hd_a_5370 */
    0x859b7fa2a2d1577fULL, /* hd_a_5350 */
    0x85d561c9a40789e4ULL, /* bd_a_2870_2 */
    0x85d564c9a4078efdULL, /* bd_a_2870_1 */
    0x886aaa772d99047aULL, /* am_f_9530 */
    0x886e0f772d9be5f0ULL, /* am_f_9521 */
    0x886e10772d9be7a3ULL, /* am_f_9520 */
    0x886e58df3aa8e613ULL, /* bd_a_9509_bd_a_9500_1 */
    0x886e59df3aa8e7c6ULL, /* bd_a_9509_bd_a_9500_2 */
    0x8879b7e787b1c74bULL, /* bd_m_9510_1 */
    0x8879b8e787b1c8feULL, /* bd_m_9510_2 */
    0x88f555666b1d32e4ULL, /* hd_f_9710 */
    0x8a0c6e1d9efacdafULL, /* bd_m_9341_1 */
    0x8a0c6f1d9efacf62ULL, /* bd_m_9341_2 */
    0x8d02b76fec2e11acULL, /* hd_a_3322 */
    0x8d02b96fec2e1512ULL, /* hd_a_3320 */
    0x8e6a17d299fe378cULL, /* bd_f_9370_1 */
    0x8e6a1ad299fe3ca5ULL, /* bd_f_9370_2 */
    0x9187d17732e364f7ULL, /* am_f_9230 */
    0x9262d3d015865e9fULL, /* hd_m_9521_hd_f_9521 */
    0x954d342d0b1293bfULL, /* bd_m_9360_2 */
    0x95d31e61b35415ffULL, /* bd_m_9620_bd_f_9620_1 */
    0x95d31f61b35417b2ULL, /* bd_m_9620_bd_f_9620_2 */
    0x961b6043913f919fULL, /* hd_m_9480_hd_f_9480 */
    0x9695bc8ba713a8b0ULL, /* bd_a_9700_1 */
    0x9695bf8ba713adc9ULL, /* bd_a_9700_2 */
    0x983b0471227566f0ULL, /* lg_m_2540 */
    0x983e8a7122788079ULL, /* lg_m_2550 */
    0x98421071227b9a02ULL, /* lg_m_2560 */
    0x98451671227dda0bULL, /* lg_m_2570 */
    0x9845d0d29feab927ULL, /* bd_f_9371_1 */
    0x9845d1d29feabadaULL, /* bd_f_9371_2 */
    0x984f88712286f046ULL, /* lg_m_2520 */
    0x993b4c50cd8084f3ULL, /* lg_m_9289_lg_m_9280 */
    0x99463dc71a3909a9ULL, /* hd_m_9590 */
    0x99614dc71a4fec91ULL, /* hd_m_9510 */
    0x996839c71a55e943ULL, /* hd_m_9530 */
    0x996b3fc71a58294cULL, /* hd_m_9540 */
    0x9a786a773807993fULL, /* am_f_9380 */
    0x9a78f43faeeda737ULL, /* lg_f_9220_lg_m_9220 */
    0x9a7bf077380ab2c8ULL, /* am_f_9370 */
    0x9a7bf177380ab47bULL, /* am_f_9371 */
    0x9a82fc773810e5daULL, /* am_f_9350 */
    0x9a8661773813c750ULL, /* am_f_9341 */
    0x9a8662773813c903ULL, /* am_f_9340 */
    0x9a8a4a23afa558e7ULL, /* lg_f_1000_lg_m_1000 */
    0x9a8cee7738192295ULL, /* am_f_9320 */
    0x9a93fa77381f55a7ULL, /* am_f_9300 */
    0x9b60df64935b24cfULL, /* lg_f_9700_lg_m_9700 */
    0x9b83b7ca76c4ad77ULL, /* bd_a_9500_2 */
    0x9b83b8ca76c4af2aULL, /* bd_a_9500_1 */
    0x9f9464d2a3abc3a3ULL, /* bd_f_9372_2 */
    0x9f9465d2a3abc556ULL, /* bd_f_9372_1 */
    0xa093c7e5f18478d3ULL, /* lg_f_9210_lg_m_9210 */
    0xa0b6f0c71e172f38ULL, /* hd_m_9430 */
    0xa0ba76c71e1a48c1ULL, /* hd_m_9420 */
    0xa0bd7cc71e1c88caULL, /* hd_m_9410 */
    0xa0c0e2c71e1f6bf3ULL, /* hd_m_9400 */
    0xa0c7eec71e259f05ULL, /* hd_m_9460 */
    0xa0cb74c71e28b88eULL, /* hd_m_9450 */
    0xa0ce7ac71e2af897ULL, /* hd_m_9440 */
    0xa16c5fbeda623ed3ULL, /* lg_m_9509_lg_m_9500 */
    0xa38696e20c027f37ULL, /* bd_f_9350_2 */
    0xa38697e20c0280eaULL, /* bd_f_9350_1 */
    0xa3b81f44877a45e7ULL, /* am_f_9270_am_m_9270 */
    0xa44177e4b210d904ULL, /* bd_a_9570_2 */
    0xa4417ae4b210de1dULL, /* bd_a_9570_1 */
    0xa513a04cf04141f9ULL, /* am_a_9390 */
    0xa52eb04cf05824e1ULL, /* am_a_9310 */
    0xa5643037356df9dcULL, /* bd_m_9220_2 */
    0xa5643337356dfef5ULL, /* bd_m_9220_1 */
    0xa5cc62ed437043b7ULL, /* lg_f_9570_lg_m_9570 */
    0xa6bd8ea3c6142a97ULL, /* am_f_9260_am_m_9260 */
    0xa95babb7c7f857b3ULL, /* lg_f_9610_lg_m_9610 */
    0xa9ae1d7d6bff4197ULL, /* bd_f_9450_1 */
    0xa9ae1e7d6bff434aULL, /* bd_f_9450_2 */
    0xa9c23e57f9ecae0fULL, /* am_m_9349_am_m_9340 */
    0xa9d091c7235e762cULL, /* hd_m_9720 */
    0xaac48db0d4bb712bULL, /* lg_f_9250_lg_m_9250 */
    0xaafceb0117c8726bULL, /* bd_f_5320_bd_m_5320_1 */
    0xaafcec0117c8741eULL, /* bd_f_5320_bd_m_5320_2 */
    0xac00cacd0380dadfULL, /* am_m_9710_am_f_9710 */
    0xaca4051d12230cdbULL, /* hd_f_2520_hd_m_2520 */
    0xae22cf4cf57f72b2ULL, /* am_a_9290 */
    0xae3ad94cf5941591ULL, /* am_a_9220 */
    0xae3e5f4cf5972f1aULL, /* am_a_9210 */
    0xae41c54cf59a1243ULL, /* am_a_9200 */
    0xae4bd74cf5a2855eULL, /* am_a_9250 */
    0xae4f5d4cf5a59ee7ULL, /* am_a_9240 */
    0xaefc8d9acc322556ULL, /* lg_m_9280 */
    0xaf00139acc353edfULL, /* lg_m_9290 */
    0xaf03999acc385868ULL, /* lg_m_9260 */
    0xaf069f9acc3a9871ULL, /* lg_m_9270 */
    0xaf0a259acc3db1faULL, /* lg_m_9240 */
    0xaf0d8b9acc409523ULL, /* lg_m_9250 */
    0xaf11119acc43aeacULL, /* lg_m_9220 */
    0xaf179d9acc49083eULL, /* lg_m_9200 */
    0xaf1b239acc4c21c7ULL, /* lg_m_9210 */
    0xafaa56fc4a31d3c4ULL, /* bd_f_9320_2 */
    0xafaa59fc4a31d8ddULL, /* bd_f_9320_1 */
    0xb071cd90214bf5fbULL, /* hd_a_9509_hd_a_9500 */
    0xb0fd870815ca3207ULL, /* hd_m_9710_hd_f_9710 */
    0xb249c766f5ae25e7ULL, /* hd_f_9440_hd_m_9440 */
    0xb27d32c72848eb40ULL, /* hd_m_9610 */
    0xb2aab3a4e9f436dbULL, /* hd_f_9510_hd_m_9510 */
    0xb31ea758adcafbc0ULL, /* lg_m_1000 */
    0xb5b54d6ad2c850d1ULL, /* hd_m_2520 */
    0xb5c64b6ad2d6c09eULL, /* hd_m_2550 */
    0xb5c9d16ad2d9da27ULL, /* hd_m_2540 */
    0xb7bdb29ad12e23c0ULL, /* lg_m_9310 */
    0xb7c1389ad1313d49ULL, /* lg_m_9300 */
    0xb7c8449ad137705bULL, /* lg_m_9320 */
    0xb7cb4a9ad139b064ULL, /* lg_m_9350 */
    0xb7cecf9ad13cc83aULL, /* lg_m_9341 */
    0xb7ced09ad13cc9edULL, /* lg_m_9340 */
    0xb7d2369ad13fad16ULL, /* lg_m_9370 */
    0xb7d2379ad13faec9ULL, /* lg_m_9371 */
    0xb7d5bc9ad142c69fULL, /* lg_m_9360 */
    0xb7d9429ad145e028ULL, /* lg_m_9390 */
    0xb7dc489ad1482031ULL, /* lg_m_9380 */
    0xbab181e9c9f207e7ULL, /* lg_f_9430_lg_m_9430 */
    0xbabb38ff9be42fcfULL, /* lg_m_9480_lg_f_9480 */
    0xbbea5af4263f4fdbULL, /* bd_a_9550_1 */
    0xbbea5bf4263f518eULL, /* bd_a_9550_2 */
    0xbc04a67254c8a378ULL, /* lg_f_9529_lg_f_9521 */
    0xbc1e9bedae01dcf7ULL, /* bd_a_9210_2 */
    0xbc1e9cedae01deaaULL, /* bd_a_9210_1 */
    0xbceba30811c8639bULL, /* bd_m_2520_1 */
    0xbceba40811c8654eULL, /* bd_m_2520_2 */
    0xbd68967f6e0519c3ULL, /* hd_a_1000 */
    0xbe4ff522130c9080ULL, /* bd_m_9450_1 */
    0xbe4ff822130c9599ULL, /* bd_m_9450_2 */
    0xc07d3e3c05753d1fULL, /* bd_m_9309_bd_m_9300_1 */
    0xc07d3f3c05753ed2ULL, /* bd_m_9309_bd_m_9300_2 */
    0xc0c9db9ad66a1470ULL, /* lg_m_9440 */
    0xc0cd619ad66d2df9ULL, /* lg_m_9450 */
    0xc0d0e79ad6704782ULL, /* lg_m_9460 */
    0xc0d3ed9ad672878bULL, /* lg_m_9470 */
    0xc0d7739ad675a114ULL, /* lg_m_9400 */
    0xc0daf99ad678ba9dULL, /* lg_m_9410 */
    0xc0de5f9ad67b9dc6ULL, /* lg_m_9420 */
    0xc0e1e59ad67eb74fULL, /* lg_m_9430 */
    0xc0f5e99ad68f6725ULL, /* lg_m_9490 */
    0xc20d14e20a49ae3bULL, /* lg_f_9720_lg_m_9720 */
    0xc3ed3a0bbb7d219bULL, /* bd_f_9300_1 */
    0xc3ed3b0bbb7d234eULL, /* bd_f_9300_2 */
    0xc564f10f5dab3be4ULL, /* bd_m_9270_1 */
    0xc564f40f5dab40fdULL, /* bd_m_9270_2 */
    0xc5c0d538f58f532bULL, /* lg_m_9349_lg_m_9340 */
    0xc6f75a3d3031f6efULL, /* bd_f_9620_2 */
    0xc6f75b3d3031f8a2ULL, /* bd_f_9620_1 */
    0xc764564d0398a0e4ULL, /* am_a_9700 */
    0xc76ec84d03a1b71fULL, /* am_a_9730 */
    0xc7a82714f101c647ULL, /* am_f_9510_am_m_9510 */
    0xc7fa0f99b16f311bULL, /* lg_f_9630_lg_m_9630 */
    0xc9d6049adba60520ULL, /* lg_m_9570 */
    0xc9d98a9adba91ea9ULL, /* lg_m_9560 */
    0xc9dc909adbab5eb2ULL, /* lg_m_9550 */
    0xc9e0169adbae783bULL, /* lg_m_9540 */
    0xc9e39c9adbb191c4ULL, /* lg_m_9530 */
    0xc9ea889adbb78e76ULL, /* lg_m_9510 */
    0xc9ed8e9adbb9ce7fULL, /* lg_m_9500 */
    0xca05989adbce715eULL, /* lg_m_9590 */
    0xca73b73c513be873ULL, /* bd_m_9460_1 */
    0xca73b83c513bea26ULL, /* bd_m_9460_2 */
    0xcbf539c736908262ULL, /* hd_m_9380 */
    0xcc09bdc736a20bb8ULL, /* hd_m_9320 */
    0xcc1049c736a7654aULL, /* hd_m_9300 */
    0xcc1abac736b079d2ULL, /* hd_m_9371 */
    0xcc1abbc736b07b85ULL, /* hd_m_9370 */
    0xcc1e41c736b3950eULL, /* hd_m_9340 */
    0xcc1e42c736b396c1ULL, /* hd_m_9341 */
    0xcc2147c736b5d517ULL, /* hd_m_9350 */
    0xce955114048166f5ULL, /* am_a_2870 */
    0xcf248799ae81063cULL, /* hd_a_4150 */
    0xcf2e9999ae897957ULL, /* hd_a_4160 */
    0xcf6e2339c3593fb8ULL, /* bd_m_body */
    0xcffbfbab98106bdfULL, /* bd_a_9290_2 */
    0xcffbfcab98106d92ULL, /* bd_a_9290_1 */
    0xd0707f4d08d49194ULL, /* am_a_9630 */
    0xd14dc39adf8a5dc1ULL, /* lg_m_9630 */
    0xd1542f9adf8f80f3ULL, /* lg_m_9610 */
    0xd25a6f29f798635fULL, /* hd_m_9289_hd_m_9280 */
    0xd29e02376425e887ULL, /* lg_f_9490_lg_m_9490 */
    0xd2fbf24f41ffe247ULL, /* am_m_5370 */
    0xd4ee9b0aeabbca11ULL, /* am_a_1000 */
    0xd50b74c73bd4e62dULL, /* hd_m_9280 */
    0xd51c72c73be355faULL, /* hd_m_9270 */
    0xd51fd8c73be63923ULL, /* hd_m_9260 */
    0xd5b2eb64418ce267ULL, /* lg_m_9230_lg_f_9230 */
    0xd627f3f4a8cc5b8fULL, /* hd_f_9270_hd_m_9270 */
    0xd6f415806ac0d0a8ULL, /* bd_m_9380_1 */
    0xd6f418806ac0d5c1ULL, /* bd_m_9380_2 */
    0xd8635319798fb323ULL, /* bd_f_9720_bd_m_9720_1 */
    0xd8635419798fb4d6ULL, /* bd_f_9720_bd_m_9720_2 */
    0xd8a419c9a6447ac3ULL, /* bd_f_9380_1 */
    0xd8a41ac9a6447c76ULL, /* bd_f_9380_2 */
    0xd96acb900cd5d71bULL, /* hd_f_9341_hd_m_9341 */
    0xd9912c4d0e220b9aULL, /* am_a_9500 */
    0xd997984d0e272eccULL, /* am_a_9560 */
    0xd99b1e4d0e2a4855ULL, /* am_a_9570 */
    0xd9a22a4d0e307b67ULL, /* am_a_9550 */
    0xda16d87d6a3ecda0ULL, /* bd_a_9630_1 */
    0xda16db7d6a3ed2b9ULL, /* bd_a_9630_2 */
    0xda596c9ae4c574f1ULL, /* lg_m_9740 */
    0xda67649ae4d1a4b5ULL, /* lg_m_9700 */
    0xda6a6a9ae4d3e4beULL, /* lg_m_9730 */
    0xda6df09ae4d6fe47ULL, /* lg_m_9720 */
    0xdabfd391db70b923ULL, /* lg_f_9290_lg_m_9290 */
    0xdb115b85c7bdf940ULL, /* bd_a_9390_2 */
    0xdb115e85c7bdfe59ULL, /* bd_a_9390_1 */
    0xdb8157e7541550a7ULL, /* bd_a_9470_2 */
    0xdb8158e75415525aULL, /* bd_a_9470_1 */
    0xdc14f680f1dea601ULL, /* bd_a_underwear */
    0xdc7cfa17944ad947ULL, /* hd_f_9540_hd_m_9540 */
    0xdc853f175d7c7f5bULL, /* bd_a_9240_1 */
    0xdc8540175d7c810eULL, /* bd_a_9240_2 */
    0xdcd0edef5f5e2c73ULL, /* hd_f_9280_hd_m_9280 */
    0xdd18bb6504cfd684ULL, /* bd_f_9460_1 */
    0xdd18be6504cfdb9dULL, /* bd_f_9460_2 */
    0xde2067c354177012ULL, /* hd_a_9120 */
    0xde2068c3541771c5ULL, /* hd_a_9121 */
    0xde23edc3541a899bULL, /* hd_a_9130 */
    0xdf1bddc5a4372580ULL, /* am_f_9529_am_f_9521 */
    0xdfb93e8aa05fa267ULL, /* bd_f_9270_bd_m_9270_1 */
    0xdfb93f8aa05fa41aULL, /* bd_f_9270_bd_m_9270_2 */
    0xdfd1a25f97ec8fe3ULL, /* am_m_9620_am_f_9620 */
    0xe030541caa12c21bULL, /* bd_f_9510_bd_m_9510_1 */
    0xe030551caa12c3ceULL, /* bd_f_9510_bd_m_9510_2 */
    0xe0d5d14d11dade74ULL, /* am_a_9490 */
    0xe0e9d54d11eb8e4aULL, /* am_a_9470 */
    0xe17fd5d9c574e8c3ULL, /* bd_f_body */
    0xe3a4938a923837abULL, /* bd_m_9280_1 */
    0xe3a4948a9238395eULL, /* bd_m_9280_2 */
    0xe4ac6851fb18e073ULL, /* lg_m_9620_lg_f_9620 */
    0xe5da0ced77f9d97bULL, /* bd_f_2550_bd_m_2550 */
    0xe65ad75b3b8dc757ULL, /* bd_f_9740_bd_m_9740_1 */
    0xe65ad85b3b8dc90aULL, /* bd_f_9740_bd_m_9740_2 */
    0xe66554841c252387ULL, /* bd_m_9720_2 */
    0xe66555841c25253aULL, /* bd_m_9720_1 */
    0xe78f050e212497f0ULL, /* hd_f_9529_hd_f_9521 */
    0xe92ecaafe44b865fULL, /* hd_m_9349_hd_m_9340 */
    0xeb92e77308960c4bULL, /* lg_f_9270_lg_m_9270 */
    0xee9996d91900afc8ULL, /* bd_f_9360_2 */
    0xef0e08ecb5031a25ULL, /* bd_a_9600_fur */
    0xef5478dbbf0f0b48ULL, /* bd_a_9560_1 */
    0xef547bdbbf0f1061ULL, /* bd_a_9560_2 */
    0xf04937c35e9ce7bfULL, /* hd_a_9310 */
    0xf064c7c35eb4a427ULL, /* hd_a_9390 */
    0xf139182bfed7a0eaULL, /* am_m_9720 */
    0xf2fb3b2f5bc514cbULL, /* am_f_9440_am_m_9440 */
    0xf30bd64ba4716c93ULL, /* bd_f_2560_bd_m_2560_1 */
    0xf30bd74ba4716e46ULL, /* bd_f_2560_bd_m_2560_2 */
    0xf3340d384a73f990ULL, /* bd_m_9379_bd_m_9372_2 */
    0xf33410384a73fea9ULL, /* bd_m_9379_bd_m_9372_1 */
    0xf5209409aec05020ULL, /* bd_m_9400_2 */
    0xf5209709aec05539ULL, /* bd_m_9400_1 */
    0xf72058f27c6c5131ULL, /* bd_a_1000 */
    0xf78ad6c362537a90ULL, /* hd_a_9290 */
    0xf7a5e6c3626a5d78ULL, /* hd_a_9210 */
    0xf7a96cc3626d7701ULL, /* hd_a_9200 */
    0xf7afd8c362729a33ULL, /* hd_a_9220 */
    0xf7b35ec36275b3bcULL, /* hd_a_9250 */
    0xf7b6e4c36278cd45ULL, /* hd_a_9240 */
    0xf9edb191d913acadULL, /* lg_m_6200 */
    0xfa230eceec3d100dULL, /* hd_m_0001_m */
    0xfa354e2355b684e7ULL, /* lg_f_9280_lg_m_9280 */
    0xfa45412c0413919aULL, /* am_m_9610 */
    0xfd796866f3c7a5f3ULL, /* bd_f_9400_bd_m_9400_1 */
    0xfd796966f3c7a7a6ULL, /* bd_f_9400_bd_m_9400_2 */
};

static int pair_lookup(const struct pair_hash *p,u32 count,u64 a,u64 b){u32 lo=0,hi=count;while(lo<hi){u32 mid=lo+((hi-lo)>>1);u64 x=p[mid].spec,y=p[mid].asset;if(a<x||(a==x&&b<y))hi=mid;else if(a>x||(a==x&&b>y))lo=mid+1;else return 1;}return 0;}
static int safe_normal_tuple(u64 d,u64 s,u64 b){
 u32 lo=0,hi=(u32)(sizeof(k_normal_triples)/sizeof(k_normal_triples[0]));
 while(lo<hi){u32 m=lo+((hi-lo)>>1);struct triple_hash x=k_normal_triples[m];
  if(d<x.diffuse||(d==x.diffuse&&(s<x.spec||(s==x.spec&&b<x.bump))))hi=m;
  else if(d>x.diffuse||(d==x.diffuse&&(s>x.spec||(s==x.spec&&b>x.bump))))lo=m+1;
  else return 1;}
 return 0;
}
static int safe_diffuse_pair(u64 a,u64 b){return pair_lookup(k_diffuse_pairs,(u32)(sizeof(k_diffuse_pairs)/sizeof(k_diffuse_pairs[0])),a,b);}
static int hash_lookup_u64(const u64 *p,u32 count,u64 h){u32 lo=0,hi=count;while(lo<hi){u32 m=lo+((hi-lo)>>1);u64 x=p[m];if(h<x)hi=m;else if(h>x)lo=m+1;else return 1;}return 0;}
static int spec_hash(u64 h){return hash_lookup_u64(k_spec_names,(u32)(sizeof(k_spec_names)/sizeof(k_spec_names[0])),h);}
static int normal_hash(u64 h){return hash_lookup_u64(k_normal_names,(u32)(sizeof(k_normal_names)/sizeof(k_normal_names[0])),h);}
static int diffuse_hash(u64 h){return hash_lookup_u64(k_diffuse_names,(u32)(sizeof(k_diffuse_names)/sizeof(k_diffuse_names[0])),h);}
static int is_spec_name(const u16*s,u32 n){return s&&n&&spec_hash(fnv_name(s,n));}
static int is_normal_name(const u16*s,u32 n){return s&&n&&normal_hash(fnv_name(s,n));}
static int is_diffuse_name(const u16*s,u32 n){return s&&n&&diffuse_hash(fnv_name(s,n));}
int should_track_name(const u16*s,u32 n){if(!s||n<1)return 0;u64 h=fnv_name(s,n);uptr base=module_base();if(spec_hash(h)){log_once(base,TELEM_CAPTURE_SPEC,"[DSRRL][ASSET] CAPTURE_SPEC accepted");return 1;}if(normal_hash(h)){log_once(base,TELEM_CAPTURE_NORMAL,"[DSRRL][ASSET] CAPTURE_NORMAL accepted");return 1;}if(diffuse_hash(h)){log_once(base,TELEM_CAPTURE_DIFFUSE,"[DSRRL][ASSET] CAPTURE_DIFFUSE accepted");return 1;}return 0;}

static u32 load_mode_for_resource(uptr base,u64 key){u32 mode=0;struct asset_entry*tab=(struct asset_entry*)(base+RVA_ASSET_MAP);for(u32 i=0;i<ASSET_MAP_COUNT;++i){if(tab[i].loading_resource==key&&tab[i].load_mode){if(mode&&mode!=tab[i].load_mode)return LOAD_NONE;mode=tab[i].load_mode;}}return mode;}
int build_asset_path(void *state_,void *exact_,u16*out){
 if(!state_||!exact_||!out)return 0;uptr base=module_base();u8*state=(u8*)state_;u8*exact=(u8*)exact_;u64 key=*(u64*)exact;u32 mode=load_mode_for_resource(base,key);if(!mode)return 0;
 u32 n=*(u32*)(exact+0x10);const u16*name=(const u16*)(exact+0x18);if(mode==LOAD_NORMAL&&!is_normal_name(name,n))return 0;if(mode==LOAD_DIFFUSE&&!is_diffuse_name(name,n))return 0;
 u32 old_len=*(u32*)(state+0x34);if(old_len<15)return 0;u32 prefix=old_len-15;const u16*old=(const u16*)(state+0xb38);const char*ns=(mode==LOAD_NORMAL)?"DSRRL\\Normals\\":"DSRRL\\Diffuse\\";u32 nslen=(mode==LOAD_NORMAL)?14u:14u;if(prefix+nslen+n+4>0x207)return 0;u32 o=0;for(u32 i=0;i<prefix;++i)out[o++]=old[i];for(u32 i=0;i<nslen;++i)out[o++]=(u8)ns[i];for(u32 i=0;i<n;++i)out[o++]=name[i];out[o++]='.';out[o++]='d';out[o++]='d';out[o++]='s';out[o]=0;return 1;
}
/* V12 receiver ordinal fix.
   TLS block+0x14 is the local metadata-table ordinal written by the Material Response core,
   not the canonical receiver_registry.receiver_index. Static table RE proves:
     local 0..22  = ordinary DifSpcBmp HemEnv/HemEnvLerp expansion records (canonical Bmp family 24..35)
     local 23..46 = DifSpc non-Bmp records (canonical family 36..47)
     local 47     = inherited metadata hole/non-record
     local 48..50 = separate Subsurf operator-island records
   Therefore the ordinary Normal/Diffuse asset bridge preserves its original Bmp semantic scope
   by accepting local 0..22 only. No receiver-family widening is performed. */
static int current_receiver_class(uptr base){
 u32 idx=*(u32*)(base+RVA_TLS_INDEX);uptr arr;__asm__("movq %%gs:0x58, %0":"=r"(arr));if(!arr)return 0;
 uptr block=*(uptr*)(arr+((uptr)idx*8));if(!block)return 1;
 int local=*(int*)(block+0x14);if(local<0)return 2;if(local<=22)return 3;if(local<=46)return 4;if(local<=50)return 5;return 6;
}
static void log_diff_receiver_class(uptr base,int c){
 if(c==0)log_once(base,TELEM_RX_DIFF_TLS_ARRAY_NULL,"[DSRRL][ASSET_DIAG_RX] DIFFUSE TLS array NULL");
 else if(c==1)log_once(base,TELEM_RX_DIFF_TLS_BLOCK_NULL,"[DSRRL][ASSET_DIAG_RX] DIFFUSE TLS block NULL");
 else if(c==2)log_once(base,TELEM_RX_DIFF_NEGATIVE,"[DSRRL][ASSET_DIAG_RX] DIFFUSE local receiver NEGATIVE");
 else if(c==3)log_once(base,TELEM_RX_DIFF_0_23,"[DSRRL][ASSET_DIAG_RX] DIFFUSE local 0..22 DifSpcBmp PASS");
 else if(c==4)log_once(base,TELEM_RX_DIFF_24_35,"[DSRRL][ASSET_DIAG_RX] DIFFUSE local 23..46 DifSpc REJECT");
 else if(c==5)log_once(base,TELEM_RX_DIFF_36_47,"[DSRRL][ASSET_DIAG_RX] DIFFUSE local 47..50 nonordinary/Subsurf REJECT");
 else log_once(base,TELEM_RX_DIFF_48_PLUS,"[DSRRL][ASSET_DIAG_RX] DIFFUSE local >=51 invalid REJECT");
}
static void log_norm_receiver_class(uptr base,int c){
 if(c==0)log_once(base,TELEM_RX_NORM_TLS_ARRAY_NULL,"[DSRRL][ASSET_DIAG_RX] NORMAL TLS array NULL");
 else if(c==1)log_once(base,TELEM_RX_NORM_TLS_BLOCK_NULL,"[DSRRL][ASSET_DIAG_RX] NORMAL TLS block NULL");
 else if(c==2)log_once(base,TELEM_RX_NORM_NEGATIVE,"[DSRRL][ASSET_DIAG_RX] NORMAL local receiver NEGATIVE");
 else if(c==3)log_once(base,TELEM_RX_NORM_0_23,"[DSRRL][ASSET_DIAG_RX] NORMAL local 0..22 DifSpcBmp PASS");
 else if(c==4)log_once(base,TELEM_RX_NORM_24_35,"[DSRRL][ASSET_DIAG_RX] NORMAL local 23..46 DifSpc REJECT");
 else if(c==5)log_once(base,TELEM_RX_NORM_36_47,"[DSRRL][ASSET_DIAG_RX] NORMAL local 47..50 nonordinary/Subsurf REJECT");
 else log_once(base,TELEM_RX_NORM_48_PLUS,"[DSRRL][ASSET_DIAG_RX] NORMAL local >=51 invalid REJECT");
}
static void ps_set_srv(uptr native,u32 slot,u64 view){if(!native)return;void**vt=*(void***)native;if(!vt||!vt[8])return;typedef void(*fn_t)(void*,u32,u32,const u64*);((fn_t)vt[8])((void*)native,slot,1,&view);}
static u64 ps_get_srv(uptr native,u32 slot){u64 view=0;if(!native)return 0;void**vt=*(void***)native;if(!vt||!vt[73])return 0;typedef void(*fn_t)(void*,u32,u32,u64*);((fn_t)vt[73])((void*)native,slot,1,&view);return view;}
static void release_srv(u64 view){if(!view)return;void**vt=*(void***)view;if(!vt||!vt[2])return;typedef u32(*fn_t)(void*);((fn_t)vt[2])((void*)view);}

void prepare_diffuse(void *cmd,u32 route_index){
 uptr base=module_base();if(!cmd||route_index>=369)return;
 int rxclass=current_receiver_class(base);log_diff_receiver_class(base,rxclass);if(rxclass!=3)return;
 uptr native=native_from_cmd(cmd);if(!native)return;struct asset_entry*e=find_asset(base,native,1);if(!e)return;e->prepared_t0=0;e->flags&=~F_DIFF_READY;
 u8 flag=*(u8*)(base+RVA_REGISTRY+(uptr)route_index*REGISTRY_STRIDE+REGISTRY_FLAG_OFF);if(flag>2)return;
 u64 t0=ps_get_srv(native,0),t1=ps_get_srv(native,1);if(!t0||!t1){release_srv(t0);release_srv(t1);return;}
 typedef void*(*state_get_t)(void);void*state=((state_get_t)(base+RVA_STATE_GET))();if(!state){release_srv(t0);release_srv(t1);return;}
 typedef void*(*cmd_lookup_t)(void*,uptr,u32);void*cs=((cmd_lookup_t)(base+RVA_CMD_LOOKUP))(state,native,0);if(!cs){release_srv(t0);release_srv(t1);return;}
 typedef void*(*exact_lookup_t)(void*,u64);void*e1=((exact_lookup_t)(base+RVA_EXACT_LOOKUP))(state,t1);void*e0=((exact_lookup_t)(base+RVA_EXACT_LOOKUP))(state,t0);if(!e1||!e0){release_srv(t0);release_srv(t1);return;}
 log_once(base,TELEM_DIAG_DIFF_EXACT_PASS,"[DSRRL][ASSET_DIAG_SAFE] DIFFUSE exact SRV lookup PASS");
 u32 n1=*(u32*)((u8*)e1+0x10),n0=*(u32*)((u8*)e0+0x10);const u16*s1=(const u16*)((u8*)e1+0x18),*s0=(const u16*)((u8*)e0+0x18);
 if(!n1||!n0||!is_diffuse_name(s0,n0)||!safe_diffuse_pair(fnv_name(s1,n1),fnv_name(s0,n0))){log_once(base,TELEM_DIAG_DIFF_TUPLE_REJECT,"[DSRRL][ASSET_DIAG_SAFE] DIFFUSE safe pair REJECT");release_srv(t0);release_srv(t1);return;}
 log_once(base,TELEM_DIFF_GATE,"[DSRRL][ASSET] DIFFUSE_GATE hit");
 u64 old=*(u64*)((u8*)cs+0x08);*(u64*)((u8*)cs+0x08)=t0;e->loading_resource=t0;e->load_mode=LOAD_DIFFUSE;typedef void(*ensure_t)(uptr);((ensure_t)(base+RVA_ENSURE_ASSET))(native);e->loading_resource=0;e->load_mode=0;*(u64*)((u8*)cs+0x08)=old;
 u64 side=*(u64*)((u8*)e0+0x08);typedef void(*restore_t)(uptr);((restore_t)(base+RVA_RESTORE_T10))(native);if(side){e->prepared_t0=side;e->flags|=F_DIFF_READY;log_once(base,TELEM_DIFF_READY,"[DSRRL][ASSET] DIFFUSE_SIDECAR ready");}
 release_srv(t0);release_srv(t1);
}
int diffuse_ready(uptr native){uptr base=module_base();struct asset_entry*e=find_asset(base,native,0);return e&&e->prepared_t0&&(e->flags&F_DIFF_READY);}

static void cleanup_draw_transaction(uptr native,struct asset_entry*e){
 if(!e)return;
 if(e->current_t2){if(e->flags&F_NORMAL_BOUND)ps_set_srv(native,2,e->current_t2);release_srv(e->current_t2);e->current_t2=0;}
 if(e->current_t0){if(e->flags&F_DIFF_BOUND)ps_set_srv(native,0,e->current_t0);release_srv(e->current_t0);e->current_t0=0;}
 e->flags&=~(F_NORMAL_BOUND|F_DIFF_BOUND);
}

void asset_pre(uptr native){
 uptr base=module_base();if(!native)return;
 int rxclass=current_receiver_class(base);log_norm_receiver_class(base,rxclass);if(rxclass!=3)return;
 struct asset_entry*e=find_asset(base,native,1);if(!e)return;
 /* Selector has already consumed F_DIFF_READY semantically; take the sidecar once so it can never leak into a later draw on an early return. */
 u64 diffuse_side=((e->flags&F_DIFF_READY)&&e->prepared_t0)?e->prepared_t0:0;e->prepared_t0=0;e->flags&=~F_DIFF_READY;
 /* A missed post must never leak a COM reference or leave a PTDE sidecar bound into the next draw. */
 if(e->current_t0||e->current_t2||(e->flags&(F_NORMAL_BOUND|F_DIFF_BOUND)))cleanup_draw_transaction(native,e);
 u64 t0=ps_get_srv(native,0),t1=ps_get_srv(native,1),t2=ps_get_srv(native,2);if(!t0||!t1||!t2){release_srv(t0);release_srv(t1);release_srv(t2);return;}
 u64 normal_side=0;
 typedef void*(*state_get_t)(void);void*state=((state_get_t)(base+RVA_STATE_GET))();
 if(state){typedef void*(*cmd_lookup_t)(void*,uptr,u32);void*cs=((cmd_lookup_t)(base+RVA_CMD_LOOKUP))(state,native,0);
  if(cs){typedef void*(*exact_lookup_t)(void*,u64);void*e0=((exact_lookup_t)(base+RVA_EXACT_LOOKUP))(state,t0),*e1=((exact_lookup_t)(base+RVA_EXACT_LOOKUP))(state,t1),*e2=((exact_lookup_t)(base+RVA_EXACT_LOOKUP))(state,t2);
   if(e0&&e1&&e2){log_once(base,TELEM_DIAG_NORM_EXACT_PASS,"[DSRRL][ASSET_DIAG_SAFE] NORMAL exact SRV lookup PASS");u32 n0=*(u32*)((u8*)e0+0x10),n1=*(u32*)((u8*)e1+0x10),n2=*(u32*)((u8*)e2+0x10);const u16*s0=(const u16*)((u8*)e0+0x18),*s1=(const u16*)((u8*)e1+0x18),*s2=(const u16*)((u8*)e2+0x18);
    if(n0&&n1&&n2&&is_normal_name(s2,n2)&&safe_normal_tuple(fnv_name(s0,n0),fnv_name(s1,n1),fnv_name(s2,n2))){log_once(base,TELEM_NORMAL_GATE,"[DSRRL][ASSET] NORMAL_GATE hit");u64 old=*(u64*)((u8*)cs+0x08);*(u64*)((u8*)cs+0x08)=t2;e->loading_resource=t2;e->load_mode=LOAD_NORMAL;typedef void(*ensure_t)(uptr);((ensure_t)(base+RVA_ENSURE_ASSET))(native);e->loading_resource=0;e->load_mode=0;*(u64*)((u8*)cs+0x08)=old;normal_side=*(u64*)((u8*)e2+0x08);typedef void(*restore_t)(uptr);((restore_t)(base+RVA_RESTORE_T10))(native);if(normal_side)log_once(base,TELEM_NORMAL_READY,"[DSRRL][ASSET] NORMAL_SIDECAR ready");}else{log_once(base,TELEM_DIAG_NORM_TUPLE_REJECT,"[DSRRL][ASSET_DIAG_SAFE] NORMAL safe tuple REJECT");}
   }
  }
 }
 /* Bind only after all lookups/preparation. Ownership of PSGet refs moves into e only for slots that are replaced. */
 if(diffuse_side){ps_set_srv(native,0,diffuse_side);e->current_t0=t0;t0=0;e->flags|=F_DIFF_BOUND;log_once(base,TELEM_DIFF_BIND,"[DSRRL][ASSET] DIFFUSE_T0_BIND confirmed");}
 if(normal_side){ps_set_srv(native,2,normal_side);e->current_t2=t2;t2=0;e->flags|=F_NORMAL_BOUND;log_once(base,TELEM_NORMAL_BIND,"[DSRRL][ASSET] NORMAL_T2_BIND confirmed");}
 release_srv(t0);release_srv(t1);release_srv(t2);
}
void asset_post(uptr native){uptr base=module_base();struct asset_entry*e=find_asset(base,native,0);if(!e)return;cleanup_draw_transaction(native,e);e->prepared_t0=0;e->flags&=~F_DIFF_READY;e->loading_resource=0;e->load_mode=0;}

void asset_push_wrapper(void*cmd,u32 stages,u64 layout,u32 layout_param,const struct update_desc*u){uptr base=module_base();typedef void(*orig_t)(void*,u32,u64,u32,const struct update_desc*);((orig_t)(base+RVA_ORIGINAL_PUSH))(cmd,stages,layout,layout_param,u);}
