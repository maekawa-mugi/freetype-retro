#!/usr/bin/env python3
"""Deferred unit checks for verdict.py; not run during patch creation."""
import tempfile
import unittest
from pathlib import Path

from verdict import analyze, parse_file

def fixture(candidate_ticks=(80, 81, 79, 80, 82, 78),
            bad_check=False, bad_digest=False, incomplete=False):
    output = ["RB1,META,host,test-fixture,6,1000000000"]
    for trial in range(5):
        output.append(f"RB1,CHECK,suite-64,scalar,PASS,{trial:08x}")
        status = "FAIL" if bad_check and trial == 2 else "PASS"
        output.append(f"RB1,CHECK,suite-64,option_c,{status},{trial:08x}")
    output.append("RB1,GATE,PASS,1")
    for sample, ticks in enumerate(candidate_ticks):
        output.append(f"RB1,SAMPLE,suite-64,scalar,{sample},16,100,1000000000,deadbeef")
        if not incomplete or sample != 5:
            digest = "00000001" if bad_digest and sample == 3 else "deadbeef"
            output.append(
                f"RB1,SAMPLE,suite-64,option_c,{sample},16,{ticks},1000000000,{digest}"
            )
    output.append("RB1,DONE,PASS,1")
    return "\n".join(output) + "\n"

class VerdictTests(unittest.TestCase):
    def load(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sample.log"
            path.write_text(text)
            parsed = parse_file(path)
            return analyze(parsed, 1.05, 1.02, 0.12)

    def test_correct_and_faster_wins(self):
        rows, choices, invalid = self.load(fixture())
        self.assertFalse(invalid)
        self.assertEqual(choices["suite-64"], "SELECT option_c")
        self.assertEqual([r for r in rows if r["variant"] == "option_c"][0]["verdict"],
                         "ELIGIBLE")

    def test_slower_is_rejected(self):
        _, choices, invalid = self.load(fixture((130, 132, 129, 131, 130, 133)))
        self.assertFalse(invalid)
        self.assertEqual(choices["suite-64"], "KEEP SCALAR (no proven winner)")

    def test_failing_correctness_blocks_everything(self):
        _, choices, invalid = self.load(fixture(bad_check=True))
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_post_timer_digest_mismatch_blocks_everything(self):
        _, choices, invalid = self.load(fixture(bad_digest=True))
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_incomplete_run_cannot_have_winner(self):
        _, choices, invalid = self.load(fixture(incomplete=True))
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_missing_scalar_checks_does_not_crash(self):
        _, choices, invalid = self.load(
            fixture().replace("RB1,CHECK,suite-64,scalar", "RB1,CHECK,elsewhere,scalar")
        )
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

if __name__ == "__main__":
    unittest.main()
