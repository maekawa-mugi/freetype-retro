import tempfile
import unittest
from pathlib import Path

from compare_glyph import SIZES, compare


def fixture(ticks=100):
    rows = ["RG1,META,fixture,normal,4,94,1,1000000000"]
    for size in SIZES:
        rows.append(f"RG1,CHECK,{size},123456789abcdef0")
        for sample in range(6):
            rows.append(f"RG1,SAMPLE,{size},{sample},{ticks}")
    rows.append("RG1,DONE,PASS")
    return "\n".join(rows) + "\n"


class GlyphLogTests(unittest.TestCase):
    def check(self, candidate):
        with tempfile.TemporaryDirectory() as directory:
            a, b = Path(directory)/"a.log", Path(directory)/"b.log"
            a.write_text(fixture())
            b.write_text(candidate)
            return compare(a, b)

    def test_complete(self):
        self.assertEqual(set(self.check(fixture(80)).values()), {1.25})

    def test_missing_size(self):
        with self.assertRaises(ValueError):
            self.check("\n".join(line for line in fixture().splitlines() if ",128," not in line))

    def test_missing_sample(self):
        with self.assertRaises(ValueError):
            self.check(fixture().replace("RG1,SAMPLE,8,5,100\n", ""))

    def test_digest_mismatch(self):
        with self.assertRaises(ValueError):
            self.check(fixture().replace("RG1,CHECK,8,123456789abcdef0", "RG1,CHECK,8,0000000000000000"))

    def test_wrong_workload(self):
        with self.assertRaises(ValueError):
            self.check(fixture().replace(",normal,4,", ",normal,3,"))

    def test_missing_done(self):
        with self.assertRaises(ValueError):
            self.check(fixture().replace("RG1,DONE,PASS", ""))


if __name__ == "__main__":
    unittest.main()
