#!/usr/bin/env python3
"""Negative tests of exact V274B read-only deferred Map/Unmap CB writer witness."""
from pathlib import Path
import unittest,sys
sys.path.insert(0,str(Path(__file__).resolve().parent))
import apply_v274b_deferred_cb0_shadow as v
sys.path.insert(0,str(Path("renderer-core/tools/v274").resolve()))
import apply_v274_lerp_actual_cb85_witness as v274

class TestV274B(unittest.TestCase):
    def test_original_v272_plus_v274_to_v274b_exact(self):
        old=v.DRAW.read_text(encoding='utf-8').replace('\r\n','\n')
        v274_src=v274.transform(old)
        updated=v.patch(v274_src,'draw')
        self.assertEqual(updated,v274_src.replace(v.HEADER,v.ADD_HEADER,1).replace(v.DRAW_CALL,v.DRAW_ADDED,1))
        self.assertEqual(v.patch(updated,'draw'),updated)
        self.assertIn('if (family == pmetal_envspec_receiver_family::hemenvlerp)',updated)
    def test_original_integrated_census(self):
        old=v.INTEGRATED.read_text(encoding='utf-8').replace('\r\n','\n')
        patched=v.patch(old,'integrated')
        self.assertEqual(v.patch(patched,'integrated'),patched)
        self.assertEqual(patched.count('addon_event::map_buffer_region>(dsrrl::runtime::v274b::map_buffer)'),2)
        self.assertEqual(patched.count('addon_event::unmap_buffer_region>(dsrrl::runtime::v274b::unmap_buffer)'),2)
    def test_unknown_preimage_fails(self):
        old=v.INTEGRATED.read_text(encoding='utf-8')
        with self.assertRaises(AssertionError):
            v.patch(old.replace(v.REGISTER,'NOT_V272_REGISTER',1),'integrated')
        with self.assertRaises(AssertionError):
            v.patch(old.replace(v.UNREGISTER,'NOT_V272_UNREGISTER',1),'integrated')
    def test_cpu_shadow_not_mutation(self):
        h=v.SRC.read_text(encoding='utf-8')
        self.assertIn('D3D11_DEVICE_CONTEXT',h.upper())
        self.assertIn('addon_event::',v.REGISTER_ADDED)
        self.assertIn('before the original ID3D11DeviceContext::Unmap',h)
        self.assertIn('CPU_WRITER_SAME_THREAD_ORDER_ONLY',h)
        self.assertIn('CPU_WRITER_CROSS_THREAD_UNVERIFIED',h)
        self.assertIn('return false; // never block/modify stock D3D11 update',h)
        self.assertIn('stage=PREPARED_ONLY GPU_READ=NO PIXEL=OPEN',h)
        self.assertIn('first!=0u',h) if False else None
        self.assertIn('out.first!=0u',h)
        self.assertNotIn('PSSetConstantBuffers',h)
        self.assertNotIn('PSSetShader(',h)
        self.assertNotIn('PSSetShaderResources',h)
        self.assertNotIn('DrawIndexed(',h)
        self.assertNotIn('Draw(',h)
    def test_stable_and_r19_materializers_not_modified(self):
        script=Path(v.__file__).read_text(encoding='utf-8')
        self.assertNotIn('pmetal_rgba_materializer.cpp',script)
        self.assertNotIn('pmetal_rgba_lerp_materializer.cpp',script)
        self.assertNotIn('pmetal_env_source_runtime.cpp',script)
if __name__=='__main__':unittest.main(verbosity=2)
