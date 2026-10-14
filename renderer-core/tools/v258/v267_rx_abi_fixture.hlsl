// V267 *ABI FIXTURE ONLY*: never embed, bind or ship this pixel shader.
// Original DSR compiler's RX33/34/35 actual input signatures are reproduced
// from original retail DXBC + Common VTX_OUT; output1 ownership remains DSR.
// Synthetic output colors deliberately have NO meaning and are NEVER a mod.
#include "native_rx33_35_interpolants.fxh"
DSRRL_NativeDualTargetSignature rx33(DSRRL_NativeRX33_CsdInput v) {
  DSRRL_NativeDualTargetSignature o;
  float3 u = v.VtxWld.xyz + v.VecNrm.xyz + v.VecEye.xyz +
    v.VecTan.xyz + v.VecTan2.xyz + v.ColVtx.xyz;
  float3 t = float3(v.TexDifDif.xy, v.VecNrm.w) +
    v.VtxClp.xyz + v.VecTan.www + v.VecEye.www;
  float s = v.IsFrontFace ? 1.f : 2.f;
  o.stockColor = float4(u + t, v.VtxWld.w + v.VecTan.w);
  o.stockAuxiliary = float4(u - t, s + v.ColVtx.w);
  return o;
}
DSRRL_NativeDualTargetSignature rx34(DSRRL_NativeRX34_SdwInput v) {
  DSRRL_NativeDualTargetSignature o;
  float3 u = v.VtxWld.xyz + v.VecNrm.xyz + v.VecEye.xyz +
    v.VecTan.xyz + v.VecTan2.xyz + v.ColVtx.xyz + v.VtxLit.xyz;
  float3 t = float3(v.TexDifDif.xy, v.VecNrm.w) +
    v.VtxClp.xyz + v.VecTan.www + v.VecEye.www;
  float s = v.IsFrontFace ? 1.f : 2.f;
  o.stockColor = float4(u + t, v.VtxWld.w + v.VecTan.w);
  o.stockAuxiliary = float4(u - t, s + v.ColVtx.w + v.VtxLit.w);
  return o;
}
DSRRL_NativeDualTargetSignature rx35(DSRRL_NativeRX35_PlainInput v) {
  DSRRL_NativeDualTargetSignature o;
  float3 u = v.VtxWld.xyz + v.VecNrm.xyz + v.VecEye.xyz +
    v.VecTan.xyz + v.VecTan2.xyz + v.ColVtx.xyz;
  float3 t = float3(v.TexDifDif.xy, v.VecNrm.w) +
    v.VtxClp.xyz + v.VecTan.www + v.VecEye.www;
  float s = v.IsFrontFace ? 1.f : 2.f;
  o.stockColor = float4(u + t, v.VtxWld.w + v.VecTan.w);
  o.stockAuxiliary = float4(u - t, s + v.ColVtx.w);
  return o;
}
