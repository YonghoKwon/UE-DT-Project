"""이름 교체가 계약 문자열/주석/기본 subobject 이름을 건드리지 않는지 검증한다."""
import importlib.util
from pathlib import Path
import unittest

spec=importlib.util.spec_from_file_location('apply_naming',Path(__file__).with_name('apply_naming.py'))
module=importlib.util.module_from_spec(spec);spec.loader.exec_module(module)


class NamingTokenTests(unittest.TestCase):
    def test_identifiers_only(self):
        text='OldType* ScanComponent; TEXT("ScanComponent"); // OldType ScanComponent\n'
        got=module.transform_source(text,{'OldType':'NewType','ScanComponent':'ScanComp'}, {})
        self.assertEqual(got,'NewType* ScanComp; TEXT("ScanComponent"); // OldType ScanComponent\n')

    def test_wire_and_test_literals_stay(self):
        text='TEXT("MA0T10.OldType"); TEXT("schemaVersion"); TEXT("slabAnalysis"); R"x(OldType)x";'
        self.assertEqual(module.transform_source(text,{'OldType':'NewType'},{}),text)

    def test_include_and_generated_only(self):
        text='#include "Old.h"\n#include "Old.generated.h"\nTEXT("Old.h");'
        got=module.transform_source(text,{}, {'Old.h':'New.h','Old.generated.h':'New.generated.h'})
        self.assertEqual(got,'#include "New.h"\n#include "New.generated.h"\nTEXT("Old.h");')

    def test_comment_and_escaped_string(self):
        text='/* OldType */ TEXT("quote\\\" OldType"); OldType Value;'
        self.assertEqual(module.transform_source(text,{'OldType':'NewType'},{}),'/* OldType */ TEXT("quote\\\" OldType"); NewType Value;')


if __name__=='__main__': unittest.main()
