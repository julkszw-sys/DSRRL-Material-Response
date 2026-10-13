# V274 source cut

Original PTDE and DSR executable audits confirm direct source-to-selector transport of blend weights on their supported ordinary paths. PTDE valid interior branch: c84.w=c85.w=t. DSR full LightBank builder reads beta from source offset 0x114 and writes draw descriptor offset 0x50, with no intervening arithmetic. DSR alternate builders create synthetic zero selector fields; zero alone is not authoritative proof of a real bank. This does not establish equality of authored data, temporal producer, same-draw values, or pixels. No addon shader or parameter modified.

Offline SHA-pinned Python audit V274: 12 original instruction positive checks and 12 tampered negative checks pass. Parent: V273, PR #363. STATUS: BINARY SOURCE CUT CONFIRMED; SEMANTIC SAME-DRAW and PIXEL OPEN.
