#!/usr/bin/env python3
from pathlib import Path
import sys
p=Path("renderer-core/src/runtime/pmetal_producer_state.cpp")
src=p.read_text(encoding="utf-8")
a="""    auto &slot = sync_slot_for(key);

    // Invalidate before source resolution."""
b="""    auto &slot = sync_slot_for(key);
    std::lock_guard<std::mutex> lock(slot.mutex);

    // Invalidate before source resolution."""
c="""    auto &slot = sync_slot_for(key);

    // Same exact material/source/epoch:"""
d="""    auto &slot = sync_slot_for(key);
    std::lock_guard<std::mutex> lock(slot.mutex);

    // Same exact material/source/epoch:"""
e="""        return;
    }

    std::lock_guard<std::mutex> lock(slot.mutex);
    slot.valid.store("""
f="""        return;
    }

    slot.valid.store("""
pairs=((a,b),(c,d),(e,f))
patched=all(src.count(before)==1 for before,after in pairs)
already=all(src.count(after)==1 for before,after in pairs)
if patched==already: raise SystemExit("V236 source guard failed")
if "--apply" in sys.argv and patched:
    for before,after in pairs: src=src.replace(before,after)
    p.write_text(src,encoding="utf-8")
    print("V236 exact same-slot begin/publish serialized")
else: print("V236 postcondition PASS", "patched" if already else "baseline")
