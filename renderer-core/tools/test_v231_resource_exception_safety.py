from pathlib import Path
import subprocess
import unittest

ROOT = Path(__file__).resolve().parents[2]
FILE = "renderer-core/src/runtime/material_resource_draw_runtime.cpp"
BASE = "a327fe47b75b0de6a8d82c19c6bee955b5047022"

def source_at_base(path):
    return subprocess.check_output(["git", "show", f"{BASE}:{path}"], cwd=ROOT)

def mask(content):
    for start, end in [
        ("void release_cache_for_device(", "std::filesystem::path process_dir()"),
        ("    companion_set old{};", "    if (accepted)"),
    ]:
        assert content.count(start) == 1
        a = content.index(start)
        b = content.index(end, a)
        assert b > a
        content = content[:a] + "<ALLOWED>" + content[b:]
    return content

class ResourceExceptionSafety(unittest.TestCase):
    def test_only_two_edited_regions(self):
        old = source_at_base(FILE).decode()
        new = (ROOT / FILE).read_text()
        self.assertEqual(mask(old), mask(new))
    def test_no_allocating_teardown(self):
        src = (ROOT / FILE).read_text()
        a = src.index("void release_cache_for_device(")
        b = src.index("std::filesystem::path process_dir()", a)
        f = src[a:b]
        self.assertIn("std::array<companion_set, k_release_batch>", f)
        self.assertNotIn("push_back(", f)
        self.assertIn("invalidate_all_companion_epochs();", f)
        self.assertIn("release_set(dead[n]);", f)
    def test_cache_insert_failure_is_fail_open(self):
        src = (ROOT / FILE).read_text()
        a = src.index("    companion_set old{};")
        b = src.index("    if (accepted)", a)
        f = src[a:b]
        for marker in ("try {", "catch (...)", "g_quarantined.store(",
                       "invalidate_all_companion_epochs();", "release_set(old);",
                       "release_set(set);", "++g_fail_open;"):
            self.assertIn(marker, f)
        self.assertIn("had_old = true;\n                g_ambiguous_view_device.emplace(", f)

if __name__ == "__main__":
    unittest.main()
