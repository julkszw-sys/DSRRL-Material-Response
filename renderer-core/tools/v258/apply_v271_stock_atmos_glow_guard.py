#!/usr/bin/env python3
"""V271 build-time overlay: original stock DSR Fog/LS and RT1 Glow preservation.

Only applied inside a reproducible V271 one-addon build. A mismatched
materialized shader is rejected to the original stock PS. No visual fix or
PTDE c84/c85 producer claim. Never touches latest_hook_source or game EXE.
"""
from pathlib import Path
import argparse
P=Path("renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
HEAD='bool final_postcondition(\n'
END='''#if defined(DSRRL_PMETAL_R19_LERP_EXACT_ENVDIFFUSE)
    outcome.envdiffuse_preserved = false;
#else
    outcome.envdiffuse_preserved = true;
#endif'''
TAG='V271 original DSR Fog/LightScattering and Glow preservation'
IMPL=r'''// V271 original DSR Fog/LightScattering and Glow preservation.
// Exact original stock shader SHA scopes this to RX33/34/35. The original
// game's stock_source is the reference; proprietary shader bytes stay local.
// Structural equivalence is NOT upstream material/Glow or PTDE pixel parity.
struct v271_retail_window {const char *sha;std::uint32_t rx;std::size_t fog,glow;};
constexpr std::array<v271_retail_window,3u> k_v271_windows{{
 {"7f113b3614f401de8f03cbc03f004fd71152f606cf58c86232c423479257fd85",33u,2552u,2895u},
 {"857aa73d1d2f7ab839d4e9feae29be5b9462e18a34305ade219e5ee2a85af9db",34u,2471u,2814u},
 {"9075b0806f0a8560977f096780aa68f44ef02be25d71be05bd51d802dca6d7e8",35u,2124u,2467u}
}};
bool v271_preserves_stock_atmos_glow(
    const std::uint8_t *stock,std::size_t stock_size,
    const std::vector<std::uint8_t> &compiled,std::uint32_t rx) noexcept
{
 if(stock==nullptr||!stock_size||compiled.empty())return false;
 try {
  const auto sha=hashing::sha256(stock,stock_size);
  const v271_retail_window *v=nullptr;
  for(const auto &x:k_v271_windows)
   if(x.rx==rx&&hashing::matches_hex(sha,x.sha)){
    if(v)return false;
    v=&x;
   }
  if(!v)return false;
  std::vector<chunk> a_chunks,b_chunks;
  std::vector<std::uint32_t> a,b;
  std::size_t a_idx=0u,b_idx=0u;
  if(!parse_dxbc(stock,stock_size,a_chunks,a_idx,a)||
     !parse_dxbc(compiled.data(),compiled.size(),b_chunks,b_idx,b))
   return false;
  std::vector<instruction_view> ai,bi;
  if(!decode(a,ai)||!decode(b,bi))return false;
  constexpr std::size_t atmo=212u;
  const auto f=v->fog,g=v->glow;
  if(f+atmo>a.size()||g+15u>a.size()||
     !opcode_at(a,g,0x36u,5u)||!opcode_at(a,g+10u,0x36u,5u)||
     a[g+1u]!=0x00102072u||a[g+2u]!=1u||
     a[g+11u]!=0x00102082u||a[g+12u]!=1u)return false;
  auto boundary=[&](std::size_t word){
    return std::any_of(ai.begin(),ai.end(),[&](const instruction_view &x){
      return x.offset==word;});
  };
  if(!boundary(f)||!boundary(f+atmo)||!boundary(g)||!boundary(g+10u))
   return false;
  auto unique=[&](std::size_t start,std::size_t len,std::size_t &hit){
   std::size_t count=0u;
   for(const auto &x:bi){
    if(x.offset+len<=b.size()&&
       std::equal(a.begin()+static_cast<std::ptrdiff_t>(start),
                  a.begin()+static_cast<std::ptrdiff_t>(start+len),
                  b.begin()+static_cast<std::ptrdiff_t>(x.offset))){
      hit=x.offset;
      if(++count>1u)return false;
    }
   }
   return count==1u;
  };
  std::size_t fog_hit=0u,glow_hit=0u,alpha_hit=0u;
  if(!unique(f,atmo,fog_hit)||!unique(g,5u,glow_hit)||
     !unique(g+10u,5u,alpha_hit)||
     fog_hit+atmo>glow_hit||glow_hit>=alpha_hit)return false;
  // RT0 is the material output and can differ. Both RT1 output MOVs
  // and the complete original Fog/LightScattering byte window may not.
  std::size_t rt1_count=0u;
  for(const auto &x:bi)
   if(x.opcode==0x36u&&x.length==5u&&
      x.offset+2u<b.size()&&b[x.offset+2u]==1u&&
      (b[x.offset+1u]==0x00102072u||b[x.offset+1u]==0x00102082u))
    ++rt1_count;
  return rt1_count==2u;
 }catch(...){return false;}
}

'''
CALL=r'''    // V271 create-time fail-open if any stock DSR Fog, LS or
    // Glow RT1 instruction differs after the complete R19 materializer.
    if (!v271_preserves_stock_atmos_glow(
            stock_source,stock_size,spec_rgb_base,
            site->semantic_receiver_id)) {
        outcome.result =
            pmetal_rgba_lerp_materialize_result::fail_postcondition;
        return outcome;
    }

'''
def main():
 p=argparse.ArgumentParser();p.add_argument('--apply',action='store_true')
 args=p.parse_args()
 original=P.read_text(encoding='utf8').replace('\r\n','\n')
 if TAG in original:
  if original.count(TAG)!=1 or original.count('v271_preserves_stock_atmos_glow(')!=2:
   raise RuntimeError('V271 duplicate/truncated materializer')
  print('V271_CREATE_TIME_STOCK_FOG_GLOW_GUARD_PASS')
  return
 if original.count(HEAD)!=1 or original.count(END)!=1:
  raise RuntimeError('V271 unexpected original R19 materializer preimage')
 result=original.replace(HEAD,IMPL+HEAD,1).replace(END,CALL+END,1)
 if args.apply:
  P.write_text(result,encoding='utf8',newline='\n')
  print('V271_CREATE_TIME_STOCK_FOG_GLOW_GUARD_APPLIED')
 else:
  raise RuntimeError('V271 guard not applied')
if __name__=='__main__':main()
