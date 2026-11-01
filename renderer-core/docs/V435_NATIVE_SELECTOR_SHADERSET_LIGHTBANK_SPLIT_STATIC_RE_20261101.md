# V435 — Exact native selector consumer split, static RE (2026-11-01)

Parent: V433 source commit `83015e850c098c7ceedd5ae89376960d00d00d7e`, preserving the complete post-V359 lineage. Read-only original owner DSR PE SHA-256 `a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b`. No EXE patch, no new runtime eligibility gates, and no producer/renderer behavior change.

## Result 1: 0x295F50 is shader-set variant selection, not physical LightBank row
`0x22BA20` calls `0x295F50` at `0x22BA74`, takes the returned DWORD to EDX and tail-jumps `0x1EC030` at `0x22BA8C`. `0x1EC030` tail-jumps `0x11B7240`. For the verified effective mode `m<5`, the model-table index is `u + 2*(3*(p+2*m)+q)`; `0x295FBC` reads one DWORD from `[entry+0x38+4*index]`. At `0x11B7288`, the resulting handle is used to select an object from a context-associated pointer table; `0x11B7293..0x11B7299` checks its vtable against RVA `0x1608530`. Original MSVC RTTI COL `0x16E39E0`, TypeDescriptor `0x1B5FFB0`, confirms `DLHG::HGShaderSetEntity`. The result is a shader-set selection handle, **not** a certified LightBank row.

## Result 2: separate physical packed LightBank consumer paths
The typed full model draw `0x20DDA0` reads both packed selector WORDs from packet `+0x4C/+0x4E`. At `0x20E2FE..0x20E314` it tests packet E0 and F0 and selects the native consumer `0x1EADE0` (CALL `0x20E342`) or `0x1EB320` (CALL `0x20E38B`).

In `0x1EB320`, `0x1EB469..0x1EB550` checks/caches the raw selector tuple and two other fields at FrpgDrawContext `+0x2348/+0x234A/+0x234C/+0x2350`. The nonnegative packed WORD splits into `group=(word>>8)&0x7f` and `entry=word&0xff`. Negative words are handled as signed sentinels; do **not** map FFFF to a physical row. It calls `0x569B40` with `RCX=[FrpgDrawContext+0x2320]`, group in EDX and category `5` in R8D. Function `0x569B40` accepts group `0..11`, category `0..12`, indexes group stride `0x1B0` and category-pointer stride `0x10`; no asset identity is inferred from those bounds. `0x563DE0` then uses the low byte (after nonnegative check) against the resource header count at `+0x0A`, subentry stride `0x110`, follows pointer at `+0xE8` and returns a DWORD at record `+0x28`.

## Result 3: no direct pair in alternate model-draw functions
Exhaustive over-approximate rel32 CALL-byte scanning of original code intervals: full `0x20DDA0..0x20E9D0` has both `0x20E342->0x1EADE0` and `0x20E38B->0x1EB320`; alternate intervals `0x20E9D0..0x20F130` and `0x20F9F0..0x20FEE0` have **no direct E8 CALL targeting either**. This does not exclude indirect or other LightBank consumers on alternate paths. V434's zeroed tuple in alternate packet paths is *not* evidence for LightBank row 0.

## Validation and next semantic cut
SHA-locked standalone V435 verifier (included in downloadable handoff) replays V434 and certifies 84 additional exact opcode preimages, 12 direct rel32 CALLs, 2 exact JMPs, and three RTTI class names. It also enumerates direct calls to the two physical bank consumers in the three paths. This is STATIC RE only: no compiled V435 addon, runtime bridge activation, per-actor packet→Source1 `native_producer_sequence`→RX34 issued join, or PTDE pixel improvement.

Next intervention, if evidence justifies one: **read-only, invocation-local** snapshot at the full model draw's packet A/B inputs and post-selector consumer outputs, tied to existing V411–V419 exact Source1 sequence/epoch, typed model generation, FLVER/MTD/slot and issued RX34 state. Protect R43 Source2 `latest_hook_source`, PointLight/DoF workstreams, and all already-working material/receiver families. No speculative gate, physical bank row pinning or global gains.

Status: STATIC consumer-path/RTTI CONFIRMED; per-model resolved LightBank, native event join, compatibility, runtime activation and pixels OPEN.
