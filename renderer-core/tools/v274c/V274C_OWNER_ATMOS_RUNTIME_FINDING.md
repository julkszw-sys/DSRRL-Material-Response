# V274C owner runtime: 96 Fog / LightScattering producer states

Source: ReShade(20261014-213805).log, 766847 bytes, 2056 lines, SHA256 f9904a2ec9eeb95a642e5a6b6d69d7fcc6f3033821e540b130a2466216e2de68.

Observed: 96/96 V274C atmosphere rows match 96/96 V274B bounded CB0 CPU writer shadows (shadow=1, same_thread=1, producer=1 Map/Unmap); RX33 74, RX34 22. Routes: 160=39, 232=19, 1073741869 ordinary leather=9, 345 metal=17, 88=12. Every sample has flver hash zero; physical FLVER full identity unresolved.

Exactly five distinct atmospheric cb12..cb20 vectors across all sampled draws, and precisely ONE shared set of those 36 floats for each beta, independent of material, slot, route or receiver. The five beta bins and sample counts: 0.0333333351 (22), 0.166666672 (22), 0.266666681 (22), 0.5 (22), 0.766666889 (8). Final bin has no route88, others include all five routes. No material-specific atmospheric CB producer observed.

| beta | Fog c12.w | LS HG g | LS blend c19.w | LS c20.w |
| --- | --- | --- | --- | --- |
| .0333333351 | .888333321 | .1099999995 | .503333330 | .129999995 |
| .166666672 | .841666639 | .150000006 | .516666651 | .250000000 |
| .266666681 | .806666672 | .180000007 | .526666641 | .340000004 |
| .5 | .725000024 | .25 | .549999952 | .549999952 |
| .766666889 | .631666601 | .3300000725 | .576666653 | .790000200 |

Source producer laws identified to float precision: c12.w = .9-.35*beta; g = c16.z/2 = .1+.3*beta, with HG tuple c16.xyz=(1-g*g,1+g,2*g); c19.w=.5+.1*beta; c20.w=.1+.9*beta. All 36 floats finite. Max absolute residual of extinction times inverse c13.rgb * c15.rgb - 1 is 4.60e-8. c20.xyz normalized length maximum deviation ~1.03e-7. Changes in derived c12.rgb and optical coefficients are nonlinear but internally consistent.

All 96 captured V274B producer cb85.x=0, cb100.w=1, cb102.x=0; old V274 GPU readback still invalid due DEFERRED_CONTEXT_NO_MAP. All measurements are PREPARED CPU writer shadows, not actual executed GPU draws. Four independent V250 Lerp DRAW_ISSUED callbacks exist, but cannot be joined to these exact 96 samples.

Interpretation: Fog and scattering SOURCE DATA varies with beta and does NOT vary with material at fixed beta. Not a leather-only Fog or LS producer fault. Cannot rule out downstream nonlinear DSR post-LS gamma amplifying a material-specific preFog specular signal. No pixel pass. Test first divergence preFog / postFog / postLS / postGamma at matched P_Leather vs P_Metal, with verified FLVER/MTD/slot/shader and PTDE pixels. Stable V272 preserved, no global atmosphere patch warranted.