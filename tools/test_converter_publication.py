"""Run actual converter publication Bash blocks with a local gh fake.

GitHub owns scheduling; configuration checks do not emulate its scheduler.
Release-creation delay/failure is modeled as the separate player publisher not
having created rolling yet. Converter jobs must never mutate release metadata
or tags themselves.
"""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[1]
WORKFLOW = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / ".github/workflows/sms-converter.yml"
workflow = WORKFLOW.read_text(encoding="utf-8")
SHA, OLD = "a" * 40, "b" * 40
bash = os.environ.get("BASH_EXECUTABLE") or shutil.which("bash")
if os.name == "nt" and Path("C:/Program Files/Git/bin/bash.exe").is_file():
    bash = os.environ.get("BASH_EXECUTABLE", "C:/Program Files/Git/bin/bash.exe")
if not bash:
    raise SystemExit("Bash is required to test the GitHub Actions shell steps")

STEPS = {
    "Windows": ("Upload Windows Executable to Rolling Release", "SMS-Media-Converter.exe"),
    "Linux": ("Upload Linux Package to Rolling Release", "SMS-Media-Converter-Linux.zip"),
    "Win7": ("Upload Windows 7 Executable to Rolling Release", "SMS-Media-Converter-Win7.exe"),
}


MASTER_PUSH = "github.event_name == 'push' && github.ref == 'refs/heads/master'"


def shell_body(name, condition=MASTER_PUSH):
    step = workflow.split("    - name: " + name + "\n", 1)[1]
    header, body = step.split("      run: |\n", 1)
    assert "      shell: bash\n" in header, (name, "publication must use Bash")
    assert condition in header, (name, condition)
    lines = []
    for line in body.splitlines():
        if line and not line.startswith("        "):
            break
        lines.append(line[8:] if line.startswith("        ") else line)
    return "\n".join(lines)


scripts = {family: shell_body(name) for family, (name, _) in STEPS.items()}
modern = workflow.split("  test-and-build:\n", 1)[1].split("  build-windows7:\n", 1)[0]
win7 = workflow.split("  build-windows7:\n", 1)[1]
for job in (modern, win7):
    assert "    concurrency:\n" in job
    assert "      queue: max\n" in job
    assert "      cancel-in-progress: false\n" in job
    group = job.split("    concurrency:\n", 1)[1].splitlines()[0]
    assert "github.event_name == 'push' && github.ref == 'refs/heads/master'" in group
    assert "github.run_id, github.run_attempt" in group
    assert "python tools/test_converter_publication.py" in job
assert "format('sms-converter-rolling-{0}', matrix.os)" in modern
assert "format('sms-converter-{0}-{1}-{2}', matrix.os, github.run_id, github.run_attempt)" in modern
assert "'sms-converter-rolling-win7'" in win7
assert "format('sms-converter-win7-{0}-{1}', github.run_id, github.run_attempt)" in win7
assert "sms-player-rolling" not in workflow

FAKE = r'''
import json, os, sys
from pathlib import Path
p = Path(os.environ["SMS_CONVERTER_STATE"])
s = json.loads(p.read_text())
a = sys.argv[1:]
s["calls"].append(a)
code, output = 0, ""
if a[0] == "api":
    assert a[1:] == ["repos/example/SMS/git/ref/heads/master", "--jq", ".object.sha"], a
    s["api_reads"] += 1
    code = 1 if s.get("api_error_at") == s["api_reads"] else 0
    output = s.get("api_output", s["master"])
elif a[:3] == ["release", "upload", "rolling"]:
    assert a[-1] == "--clobber", a
    assets = [Path(v).name for v in a[3:-1]]
    assert assets == [os.environ["SMS_CONVERTER_ASSET"]], a
    assert all(Path(v).is_file() for v in a[3:-1]), a
    s["upload_attempts"] += 1
    if s.get("release_after") == s["upload_attempts"]:
        s["release_exists"] = True
    code = 1 if (not s["release_exists"] or s["upload_attempts"] <= s.get("upload_failures", 0)) else 0
    if not code:
        for asset in assets:
            s["assets"][asset] = os.environ["GITHUB_SHA"]
    if s.get("supersede_after") == s["upload_attempts"]:
        s["master"] = "c" * 40
else:
    raise AssertionError(a)
p.write_text(json.dumps(s))
print(output)
sys.exit(code)
'''


def run_case(family, name, expected, expected_attempts, **overrides):
    asset = STEPS[family][1]
    with tempfile.TemporaryDirectory(prefix="sms-converter-publish-") as td:
        tmp = Path(td)
        initial_assets = {"Simple-Media-System.elf": OLD, "Simple-Media-System.zip": OLD,
                          "Simple-Media-System-languages.zip": OLD,
                          **{item[1]: OLD for item in STEPS.values()}}
        state = {"master": SHA, "tag": OLD, "notes": "player notes",
                 "release_exists": True, "assets": initial_assets.copy(),
                 "api_reads": 0, "upload_attempts": 0, "calls": []}
        state.update(overrides)
        state_file = tmp / "state.json"
        state_file.write_text(json.dumps(state))
        (tmp / "fake.py").write_text(FAKE, encoding="utf-8")
        (tmp / "dist-artifact").mkdir()
        (tmp / "dist-artifact" / asset).write_bytes(b"fixture")
        prefix = 'gh() { "$SMS_RELEASE_PYTHON" "$SMS_RELEASE_FAKE" "$@"; }\n'
        prefix += 'sleep() { :; }\n'
        (tmp / "step.sh").write_bytes((prefix + scripts[family] + "\n").encode("utf-8"))
        env = dict(os.environ, GITHUB_SHA=SHA, GITHUB_REPOSITORY="example/SMS",
                   SMS_CONVERTER_STATE=state_file.as_posix(), SMS_CONVERTER_ASSET=asset,
                   SMS_RELEASE_FAKE=(tmp / "fake.py").as_posix(),
                   SMS_RELEASE_PYTHON=Path(sys.executable).as_posix())
        result = subprocess.run([bash, "--noprofile", "--norc", "-eo", "pipefail", "step.sh"],
                                cwd=tmp, env=env, text=True, capture_output=True, timeout=30)
        final = json.loads(state_file.read_text())
        assert (result.returncode == 0) == expected, (family, name, result.returncode, result.stdout, result.stderr)
        assert final["upload_attempts"] == expected_attempts, (family, name, final)
        assert final["tag"] == OLD and final["notes"] == "player notes", (family, name, final)
        for other, identity in initial_assets.items():
            if other != asset:
                assert final["assets"][other] == identity, (family, name, other, final)
        if name in ("matched-head", "release-created-after-wait", "transient-upload-failure"):
            assert final["assets"][asset] == SHA, (family, name, final)
        else:
            assert final["assets"][asset] == OLD, (family, name, final)
        if name in ("stale-rerun", "master-api-failure", "invalid-api-identity", "empty-api-identity"):
            assert len(final["calls"]) == 1, (family, name, final)
        assert final["api_reads"] >= final["upload_attempts"], (family, name, final)
        print("PASS", family, name)


for family in STEPS:
    run_case(family, "matched-head", True, 1)
    run_case(family, "stale-rerun", True, 0, master="c" * 40)
    run_case(family, "master-api-failure", False, 0, api_error_at=1)
    run_case(family, "invalid-api-identity", False, 0, api_output="null")
    run_case(family, "empty-api-identity", False, 0, api_output="")
    run_case(family, "release-created-after-wait", True, 3, release_exists=False, release_after=3)
    run_case(family, "release-creation-failure", False, 12, release_exists=False)
    run_case(family, "upload-failure", False, 12, upload_failures=12)
    run_case(family, "transient-upload-failure", True, 3, upload_failures=2)
    run_case(family, "superseded-while-waiting", True, 1, release_exists=False, supersede_after=1)
    run_case(family, "api-failure-while-waiting", False, 1, release_exists=False, api_error_at=2)
print("Converter rolling publication: 33 shell controls and asset-family concurrency configuration passed")


def run_tag_case(family, failures, expected, attempts):
    """A version-tag upload must fail the job when the asset never reaches the release."""
    name, asset = STEPS[family][0].replace("Rolling", "Version"), STEPS[family][1]
    with tempfile.TemporaryDirectory(prefix="sms-converter-publish-") as td:
        tmp = Path(td)
        (tmp / "dist-artifact").mkdir()
        (tmp / "dist-artifact" / asset).write_bytes(b"fixture")
        prefix = 'gh() { echo "$*" >> calls; [ "$(wc -l < calls)" -gt "$FAILURES" ]; }\n'
        prefix += 'sleep() { :; }\n'
        body = shell_body(name, "startsWith(github.ref, 'refs/tags/v')")
        (tmp / "step.sh").write_bytes((prefix + body + "\n").encode("utf-8"))
        env = dict(os.environ, GITHUB_REF_NAME="v9.9.9", FAILURES=str(failures))
        result = subprocess.run([bash, "--noprofile", "--norc", "-eo", "pipefail", "step.sh"],
                                cwd=tmp, env=env, text=True, capture_output=True, timeout=30)
        calls = (tmp / "calls").read_text().splitlines()
        assert (result.returncode == 0) == expected, (name, failures, result.returncode, result.stdout, result.stderr)
        assert calls == ["release upload v9.9.9 dist-artifact/%s --clobber" % asset] * attempts, (name, failures, calls)
        print("PASS", family, "version-release", failures, "failed uploads")


for family in STEPS:
    run_tag_case(family, 0, True, 1)
    run_tag_case(family, 2, True, 3)
    run_tag_case(family, 12, False, 12)
print("Converter version-tag publication: 9 shell controls passed")

# The Linux package is built after the tests ran in the same checkout, so it must
# take the tracked converter files only, not what the tests left beside them.
with tempfile.TemporaryDirectory(prefix="sms-converter-package-") as td:
    tmp = Path(td)
    source = tmp / "tools" / "sms_converter"
    (source / "__pycache__").mkdir(parents=True)
    for leaf in ("main.py", "sms-converter-crash.log", "__pycache__/main.cpython-312.pyc"):
        (source / leaf).write_bytes(b"fixture")
    git = ["git", "-c", "user.name=fixture", "-c", "user.email=fixture@example.invalid",
           "-c", "commit.gpgsign=false"]
    for command in (["init", "-q"], ["add", "tools/sms_converter/main.py"], ["commit", "-q", "-m", "fixture"]):
        subprocess.run(git + command, cwd=tmp, check=True, capture_output=True, timeout=30)
    body = shell_body("Build Portable Package (Linux)", "runner.os == 'Linux'")
    (tmp / "step.sh").write_bytes(("zip() { :; }\n" + body + "\n").encode("utf-8"))
    subprocess.run([bash, "--noprofile", "--norc", "-eo", "pipefail", "step.sh"],
                   cwd=tmp, check=True, capture_output=True, timeout=30)
    package = tmp / "dist-artifact" / "SMS-Media-Converter-Linux"
    packaged = sorted(path.relative_to(package).as_posix() for path in package.rglob("*"))
    assert packaged == ["main.py"], packaged
print("Converter Linux package: tracked files only")
