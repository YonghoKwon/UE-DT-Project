"""컴포넌트 접미사가 다른 엔진 기반과 간접 상속의 누락을 방지한다."""
import json
from pathlib import Path
import subprocess
import sys
import tempfile
import unittest


class InventoryComponentTests(unittest.TestCase):
    def test_engine_component_roots_and_indirection(self):
        with tempfile.TemporaryDirectory() as tmp:
            root=Path(tmp)
            for name in ['host','device','core']:
                (root/name/'Source').mkdir(parents=True)
            (root/'device/Source/Types.h').write_text('''
UCLASS() class UCameraCaptureComponent : public USceneCaptureComponent2D {};
UCLASS() class UDepthProjectionComp : public USceneCaptureComponent2D {};
UCLASS() class UChildCapture : public UCameraCaptureComponent {};
UCLASS() class USphereCapture : public USphereComponent {};
UCLASS() class UComponentNamedData : public UObject {};
''',encoding='utf8')
            out=root/'inventory.json'
            subprocess.run([sys.executable,'-X','utf8','-I',str(Path(__file__).with_name('inventory.py')),
                str(root/'host'),str(root/'device'),str(root/'core'),str(out)],check=True,capture_output=True)
            rows={x['Name']:x for x in json.loads(out.read_text(encoding='utf8'))['Rows']}
            for name in ['UCameraCaptureComponent','UDepthProjectionComp','UChildCapture','USphereCapture']:
                self.assertTrue(rows[name]['Component'],name)
            self.assertIsNone(rows['UDepthProjectionComp']['Rename'])
            self.assertEqual(rows['UCameraCaptureComponent']['Rename'],'UCameraCaptureComp')
            self.assertNotIn('Component',rows['UComponentNamedData'])


if __name__=='__main__': unittest.main()
