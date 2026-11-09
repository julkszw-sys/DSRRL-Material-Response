# V457 — Semantic consumer census: ChrModel Light / Fog / Scattering

**Original DSR EXE:** SHA256 \`a45aaa36dd2f6cc151670a639ea5547043cf38ea79ff4178b963c6ed71f98d7b\` — unmodified. Source complete exact-instruction verifier in this directory; the local handoff also includes offline results and full report. **54/54 byte witnesses, 19/19 E8 edges, 3 exact RIP-referenced diagnostic labels, 77/77 mutation negatives PASS.** Static only, no addon build, runtime actor RX34 and pixels OPEN.

## Semantic field identities from RETAIL EXE

At native logger RVA \`0x2EE120/+0x19/+0x32\`, signed-byte fields are read and printed with RIP-referenced retail .rdata labels:

| ChrModel byte | Literal name | Exact .rdata RVA | Formatter |
|---|---|---|---|
| \`+0x93\` | **Light id** | \`0x1316D00\` | \`Light id: %d\` |
| \`+0x94\` | **Fog id** | \`0x1316D10\` | \`Fog id: %d\` |
| \`+0x95\` | **Scattering id** | \`0x1316D20\` | \`Scattering id: %d\` |
| \`+0x96\` | UNKNOWN | No identified label | 16-bit first CPU state consumer at \`0x357CAA/+0x357CC4\` |

Other logger path \`0x2FB96D/0x2FB98D/0x2FB9AD\` reads the same three byte fields.

## Three CPU packed-key encoders

- \`Light (+0x93)\`: \`0x21F600\` — raw relative E8 callsites \`0x21CA6C, 0x2FC2C6, 0x326470, 0x357BFD\`.
- \`Fog (+0x94)\`: \`0x21F5C0\` — \`0x21CA7C, 0x2FC2D8, 0x326500, 0x357C37\`.
- \`Scattering (+0x95)\`: \`0x21F640\` — \`0x21CA89, 0x2FC2EA, 0x326590, 0x357C71\`.

Each reads signed \`model+0x92\` and signed byte index; writes a 32-bit value into caller-supplied out-pointer:

\`\`\`
family = int8(model+0x92) >> 3
index  = int8(model+channel)
if family < 0 or index < 0: packed = -1
else: packed = ((family & 0x7f)<<8) | uint8(index)
\`\`\`

Packed key is **not actor identity**, and is not yet certified as the same row30/row31 key emitted in RX34.

## Verifiable CPU command transport

At \`0x20B546\`, producer \`0x21C970\` is called. It ORs dirty bitmasks model\`+0x80\` and \`+0x84\`, clears \`+0x84\` at \`0x21C98F\`, and on \`0xE0\` condition collects the above three keys. \`0x21CAC7 → 0x222AC0\` serializes a CPU command with ID \`0x804C\` (\`0x222AD2\`), a float and three 32-bit packed keys.

A decoded command path \`0x22346C → 0x222B60 → 0x1D7580\` reads a float + three keys; \`RCX=receiver+0x108\` at \`0x222BA7\`. **Exact dispatch opcode 0x804C → this handler is not yet exhaustively certified.** The structures match, but do not promote to RX34 GPU consumer.

## Separate render-state copy

At \`0x357BFD/0x357C37/0x357C71\`, the three keys are extracted and their low byte is copied to \`[owner+0x850]+0xAB/+0xAC/+0xAD\` with conditional dirty update at that object's \`+0x9C\`. \`model+0x96\` 16-bit value is copied to \`[owner+0x850]+0xAE\` and dirty bit \`0x100\` (exact witness \`0x357CAA/0x357CBA/0x357CC4\`). The owner is not proven to be a specific PlayerIns generation; destination is CPU render state, **not an authenticated RX34 draw / GPU CB**.

## Next operator-local test

Light channel first: typed owner lifetime plus exact Light packed key, transport/CPU consumer, RX34 issued draw identity and only then visible surface. Fog and Scattering must not be conflated with Light. NPC shaded row31 contamination hypothesis and own-actor descriptor transition both OPEN. Leave current addon and protected R43/latest_hook_source/Source1/Source2 untouched; fail open. Runtime PASS ≠ pixel PASS. No arbitrary row gain or forced row30/31.
