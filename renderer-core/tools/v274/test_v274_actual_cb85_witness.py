#!/usr/bin/env python3
"""Negative oracles for Lerp-only read-only actual CB0 workflow sampling."""
import sys
from pathlib import Path
import unittest
sys.path.insert(0,str(Path(__file__).resolve().parent))
import apply_v274_lerp_actual_cb85_witness as v274

class V274Contract(unittest.TestCase):
    def test_source_exact_three_insertion_sites_only(self):
        original=v274.P.read_text(encoding="utf-8").replace("\r\n","\n")
        changed=v274.transform(original)
        expected=original.replace(v274.HEADER,v274.INSERT_HEADER,1)
        expected=expected.replace(v274.BEFORE,v274.BEFORE+v274.HELPER,1)
        expected=expected.replace(v274.AFTER,v274.CALL,1)
        self.assertEqual(changed,expected)
        self.assertEqual(v274.transform(changed),changed)
        self.assertEqual(changed.count(v274.MARKER),1)

    def test_reject_unknown_preimage(self):
        original=v274.P.read_text(encoding="utf-8")
        with self.assertRaises(AssertionError):
            v274.transform(original.replace(v274.AFTER,"NOT_V272",1))
        with self.assertRaises(AssertionError):
            v274.transform(original.replace(v274.BEFORE,"NOT_V272",1))

    def test_strict_lerp_and_no_shader_changes(self):
        code=v274.HELPER+v274.CALL
        self.assertIn("if (family == pmetal_envspec_receiver_family::hemenvlerp)",code)
        self.assertIn("beta <= 0.0f || beta >= 1.0f",code)
        self.assertIn("material.owner_tuple_exact",code)
        self.assertIn("D3D11_DEVICE_CONTEXT_IMMEDIATE",code)
        self.assertIn("PSGetConstantBuffers1",code)
        self.assertIn("first_constant",code)
        self.assertIn("D3D11_USAGE_STAGING",code)
        self.assertIn("num_constants < 103u",code)
        self.assertIn("CopyResource(copy,cb0)",code)
        for prohibited in ("PSSetConstantBuffers(", "PSSetShaderResources(",
                           "PSSetShader(", "DrawIndexed(", "Draw("):
            self.assertNotIn(prohibited,code)
        self.assertIn("static std::array<std::atomic<std::uint64_t>,96>",code)
        self.assertIn("stage=PREPARED_ONLY pixel=OPEN",code)

    def test_stable_is_never_patched(self):
        stable=Path("renderer-core/src/operators/env_spec/pmetal_rgba_materializer.cpp")
        lerp=Path("renderer-core/src/operators/env_spec/pmetal_rgba_lerp_materializer.cpp")
        self.assertTrue(stable.is_file())
        self.assertTrue(lerp.is_file())
        self.assertEqual(v274.P,Path("renderer-core/src/runtime/pmetal_envspec_draw_runtime.cpp"))
        self.assertNotIn(str(stable),Path(v274.__file__).read_text(encoding="utf-8"))

if __name__=="__main__":
    unittest.main(verbosity=2)
