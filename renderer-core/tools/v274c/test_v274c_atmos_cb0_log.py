#!/usr/bin/env python3
"""V274C audit: Fog cb12, LS cb13-20 only in authenticated interior Lerp."""
from pathlib import Path
import unittest
H=Path("renderer-core/tools/v274b/v274b_cb_shadow.hpp")
class V274C(unittest.TestCase):
    @classmethod
    def setUpClass(cls): cls.s=H.read_text(encoding="utf-8")
    def test_covers_exact_atmosphere_registers(self):
        self.assertIn("reg=12u; reg<=20u; ++reg",self.s)
        self.assertIn("reg-12u)*4u+lane",self.s)
        self.assertIn("std::array<float,36u> atmosphere",self.s)
        self.assertIn("cb%u=%+.9g,%+.9g,%+.9g,%+.9g",self.s)
    def test_reuses_original_bounded_cpu_snapshot(self):
        self.assertIn("k_prefix_bytes=103u*16u",self.s)
        self.assertIn("const auto o=observe(context);",self.s)
        self.assertIn("source=1u",self.s)
        self.assertIn("static std::array<std::atomic<std::uint64_t>,96> seen",self.s)
        self.assertIn("if(context==nullptr || rx<33u || rx>35u",self.s)
        self.assertIn("!(beta>0.0f && beta<1.0f)",self.s)
    def test_no_writes(self):
        for op in ["PSSet", "DrawIndexed(","Draw(","RSSet","OMSet",
                   "ExecuteCommandList(","ClearRenderTargetView("]:
            self.assertNotIn(op,self.s)
        self.assertIn("return false; // never block/modify stock D3D11 update",self.s)
        self.assertIn("authority=CPU_WRITER_ONLY",self.s)
        self.assertIn("gpu_cb=UNOBSERVED pixel=OPEN",self.s)
    def test_exact_scoped_original_engine_source(self):
        d=Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp")
        self.assertTrue(d.is_file())
        # Build overlays are added temporarily only by V274 and V274B
        self.assertNotIn("V274C ATMOS CB0",d.read_text(encoding="utf-8"))
if __name__=="__main__":unittest.main(verbosity=2)
