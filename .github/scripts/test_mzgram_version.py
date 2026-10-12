import subprocess
import sys
import unittest
from pathlib import Path

sys.path.insert(0, str(Path(__file__).parent))

from mzgram_version import VersionError, code, dev, previous, release  # noqa: E402

SCRIPT = Path(__file__).parent / "mzgram_version.py"


class CodeTest(unittest.TestCase):
    def test_stable_and_beta_codes(self):
        self.assertEqual(code("v1.2.0"), 1020099)
        self.assertEqual(code("v1.3.0-beta.2"), 1030002)
        self.assertEqual(code("v12.99.99"), 12999999)

    def test_betas_sit_between_releases(self):
        order = ["v1.2.0", "v1.3.0-beta.1", "v1.3.0-beta.98", "v1.3.0", "v1.3.1-beta.1", "v1.3.1", "v2.0.0"]
        self.assertEqual(order, sorted(order, key=code))

    def test_bad_tags(self):
        for tag in ["1.2.0", "v1.2", "v1.100.0", "v1.2.100", "v01.2.0", "v1.2.0-beta.99", "v1.2.0-beta.0", "v1.2.0-rc.1", "v210.0.0"]:
            with self.assertRaises(VersionError, msg=tag):
                code(tag)

    def test_final_code_fits_android(self):
        self.assertLessEqual(code("v209.99.99") * 10 + 9, 2100000000)


class ReleaseTest(unittest.TestCase):
    def test_release_outputs(self):
        out = release("v1.0.0", "12.10.1", 7038, ["v1.0.0"])
        self.assertEqual(out, {"version": "1.0.0", "code": "1000099", "name": "1.0.0 (12.10.1)", "prerelease": "false"})
        self.assertEqual(release("v1.1.0-beta.1", "12.10.1", 7038, [])["prerelease"], "true")

    def test_never_below_the_current_code(self):
        # 7038 * 10 + 9 is what the builds before the first tag carry
        with self.assertRaises(VersionError):
            release("v0.0.1", "12.10.1", 7038, [])
        self.assertEqual(release("v0.1.0", "12.10.1", 7038, [])["code"], "10099")

    def test_never_below_an_existing_tag(self):
        with self.assertRaises(VersionError):
            release("v1.0.1", "12.10.1", 7038, ["v1.1.0", "v1.0.1"])
        with self.assertRaises(VersionError):
            release("v1.1.0-beta.1", "12.10.1", 7038, ["v1.1.0"])
        self.assertEqual(release("v1.1.1", "12.10.1", 7038, ["v1.1.0", "release-12.10.1"])["code"], "1010199")


class DevTest(unittest.TestCase):
    def test_no_tags_keeps_the_gradle_code(self):
        self.assertEqual(dev("12.10.1", 7038, ["release-12.10.1"]), {"code": "7038", "name": ""})

    def test_dev_build_equals_the_newest_tag(self):
        out = dev("12.10.1", 7038, ["v1.0.0", "v1.1.0-beta.2", "v0.9.0"])
        self.assertEqual(out, {"code": "1010002", "name": "1.1.0-beta.2-dev (12.10.1)"})


class PreviousTest(unittest.TestCase):
    TAGS = ["v1.0.0", "v1.1.0-beta.1", "v1.1.0-beta.2", "v1.1.0", "v1.2.0-beta.1"]

    def test_stable_since_last_stable(self):
        self.assertEqual(previous("v1.1.0", self.TAGS), "v1.0.0")

    def test_beta_since_last_tag(self):
        self.assertEqual(previous("v1.1.0-beta.2", self.TAGS), "v1.1.0-beta.1")
        self.assertEqual(previous("v1.2.0-beta.1", self.TAGS), "v1.1.0")

    def test_first_release(self):
        self.assertEqual(previous("v1.0.0", self.TAGS), "")


class CliTest(unittest.TestCase):
    def run_cli(self, *args, stdin=""):
        return subprocess.run([sys.executable, str(SCRIPT), *args], input=stdin, capture_output=True, text=True)

    def test_ls_remote_lines(self):
        stdin = "abc\trefs/tags/v1.0.0\nabc\trefs/tags/v1.0.0^{}\ndef\trefs/tags/release-12.10.1\n"
        result = self.run_cli("dev", "--upstream", "12.10.1", "--floor", "7038", stdin=stdin)
        self.assertEqual(result.stdout.split(), ["code=1000099", "name=1.0.0-dev", "(12.10.1)"])

    def test_bad_tag_fails(self):
        result = self.run_cli("release", "v1.2", "--upstream", "12.10.1", "--floor", "7038")
        self.assertEqual(result.returncode, 1)
        self.assertIn("::error::", result.stdout)


if __name__ == "__main__":
    unittest.main()
