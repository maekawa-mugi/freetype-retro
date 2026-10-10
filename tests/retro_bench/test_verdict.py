#!/usr/bin/env python3
"""Deferred unit checks for verdict.py; not run during patch creation."""
import tempfile
import unittest
from pathlib import Path

from verdict import analyze, parse_file, provisional_crossover

def fixture(candidate_ticks=(80, 81, 79, 80, 82, 78),
            bad_check=False, bad_digest=False, incomplete=False):
    output = ["RB1,META,host,test-fixture,6,1000000000",
              "RB1,CASE,suite-64,16,64,scalar,option_c"]
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
    def test_grayfill_oracle_name_matches_manifest(self):
        captured = fixture().replace("suite-64", "grayfill-64").replace(",scalar,", ",scalar_loop,")
        _, _, invalid = self.load(captured)
        self.assertFalse(invalid)

    def test_grayfill_wrong_scalar_check_name_is_blocked(self):
        captured = fixture().replace("suite-64", "grayfill-64").replace(",scalar,", ",scalar_loop,")
        captured = captured.replace("CHECK,grayfill-64,scalar_loop,", "CHECK,grayfill-64,scalar,")
        _, choices, invalid = self.load(captured)
        self.assertTrue(invalid)
        self.assertTrue(choices["grayfill-64"].startswith("BLOCKED"))

    def load(self, text):
        with tempfile.TemporaryDirectory() as tmp:
            path = Path(tmp) / "sample.log"
            path.write_text(text)
            parsed = parse_file(path)
            return analyze(parsed, 1.05, 1.02, 0.12)

    def test_entire_declared_suite_missing_must_block(self):
        # Old parser silently selected the remaining suite because it
        # checked only the cases which happened to be present in stdout.
        captured = fixture()
        captured = captured.replace(
            "RB1,CASE,suite-64,16,64,scalar,option_c",
            "RB1,CASE,suite-64,16,64,scalar,option_c\n"
            "RB1,CASE,missing-4096,16,4096,scalar,option_c",
        )
        captured = captured.replace("RB1,GATE,PASS,1", "RB1,GATE,PASS,2")
        captured = captured.replace("RB1,DONE,PASS,1", "RB1,DONE,PASS,2")
        _, choices, invalid = self.load(captured)
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))
        self.assertTrue(choices["missing-4096"].startswith("BLOCKED"))

    def test_lost_entire_suite_manifest_must_block(self):
        captured = fixture().replace("RB1,GATE,PASS,1", "RB1,GATE,PASS,2")
        captured = captured.replace("RB1,DONE,PASS,1", "RB1,DONE,PASS,2")
        _, choices, invalid = self.load(captured)
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_old_log_without_manifest_is_not_approved(self):
        captured = fixture().replace(
            "RB1,CASE,suite-64,16,64,scalar,option_c\n", ""
        )
        _, choices, invalid = self.load(captured)
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_duplicate_suite_manifest_rejected(self):
        line = "RB1,CASE,suite-64,16,64,scalar,option_c\n"
        _, choices, invalid = self.load(fixture().replace(line, line * 2))
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_missing_check_trial_rejected(self):
        captured = fixture().replace(
            "RB1,CHECK,suite-64,option_c,PASS,00000003\n", ""
        )
        _, choices, invalid = self.load(captured)
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_missing_variant_from_manifest_rejected(self):
        captured = fixture().replace(
            "RB1,CASE,suite-64,16,64,scalar,option_c",
            "RB1,CASE,suite-64,16,64,scalar,bogus_candidate",
        )
        _, choices, invalid = self.load(captured)
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_mismatched_repetitions_rejected(self):
        captured = fixture().replace(
            "RB1,SAMPLE,suite-64,option_c,2,16,",
            "RB1,SAMPLE,suite-64,option_c,2,17,",
        )
        _, choices, invalid = self.load(captured)
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_duplicate_gate_rejected(self):
        captured = fixture().replace(
            "RB1,GATE,PASS,1\n", "RB1,GATE,PASS,1\nRB1,GATE,PASS,1\n",
        )
        _, choices, invalid = self.load(captured)
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_done_count_mismatch_rejected(self):
        _, choices, invalid = self.load(
            fixture().replace("RB1,DONE,PASS,1", "RB1,DONE,PASS,2")
        )
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

    def test_baselines_are_specific_to_each_case(self):
        captured = fixture((130, 132, 129, 131, 130, 133)).replace(
            "suite-64", "lcd-16"
        )
        captured = captured.replace(
            "RB1,CASE,lcd-16,16,64,scalar,option_c",
            "RB1,CASE,lcd-16,16,64,scalar,option_c\n"
            "RB1,CASE,msb-1024,16,1024,scalar,builtin",
        )
        checks = "".join(
            f"RB1,CHECK,msb-1024,{variant},PASS,{trial:08x}\n"
            for trial in range(5)
            for variant in ("scalar", "builtin")
        )
        captured = captured.replace("RB1,GATE,PASS,1", checks + "RB1,GATE,PASS,2")
        samples = "".join(
            f"RB1,SAMPLE,msb-1024,{variant},{idx},16,{ticks},"
            "1000000000,deadbeef\n"
            for idx in range(6)
            for variant, ticks in (("scalar", 100), ("builtin", 125))
        )
        captured = captured.replace("RB1,DONE,PASS,1", samples + "RB1,DONE,PASS,2")
        _, choices, invalid = self.load(captured)
        self.assertFalse(invalid)
        self.assertEqual(choices["lcd-16"], "KEEP SCALAR (no proven winner)")
        self.assertEqual(choices["msb-1024"], "KEEP BUILTIN (no proven winner)")

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

    def test_provisional_boundary_only_when_largest_size_wins(self):
        choices = {"bgra-16": "KEEP SCALAR (no proven winner)",
                   "bgra-256": "SELECT option_c",
                   "bgra-4096": "SELECT option_c",
                   "lcd-16": "SELECT folded_c",
                   "lcd-4096": "INCONCLUSIVE"}
        bounds = provisional_crossover(choices)
        self.assertEqual(bounds["bgra"]["observed_wins_at_or_above"], 256)
        self.assertEqual(bounds["bgra"]["candidate"], "option_c")
        self.assertNotIn("lcd", bounds)

    def test_missing_scalar_checks_does_not_crash(self):
        _, choices, invalid = self.load(
            fixture().replace("RB1,CHECK,suite-64,scalar", "RB1,CHECK,elsewhere,scalar")
        )
        self.assertTrue(invalid)
        self.assertTrue(choices["suite-64"].startswith("BLOCKED"))

if __name__ == "__main__":
    unittest.main()
