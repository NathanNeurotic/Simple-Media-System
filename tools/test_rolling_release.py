"""Execute the real rolling-release shell step against a local GitHub CLI fake."""
import json
import os
from pathlib import Path
import shutil
import subprocess
import sys
import tempfile


ROOT = Path(__file__).resolve().parents[1]
SHA = "a" * 40
OLD = "b" * 40
WORKFLOW = Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / ".github/workflows/compilation.yml"
workflow = WORKFLOW.read_text(encoding="utf-8")
step = workflow.split("    - name: Update rolling pre-release\n", 1)[1]
step = step.split("    # --- Stable release:", 1)[0].split("      run: |\n", 1)[1]
script = "\n".join(line[8:] if line.startswith("        ") else line for line in step.splitlines())
bash = os.environ.get("BASH_EXECUTABLE") or shutil.which("bash")
if os.name == "nt" and Path("C:/Program Files/Git/bin/bash.exe").is_file():
    bash = os.environ.get("BASH_EXECUTABLE", "C:/Program Files/Git/bin/bash.exe")
if not bash:
    raise SystemExit("Bash is required to test the GitHub Actions shell step")

FAKE = r'''
import json, os, sys
from pathlib import Path
p = Path(os.environ["SMS_RELEASE_STATE"])
s = json.loads(p.read_text())
a = sys.argv[1:]
s["calls"].append(a)
code, output = 0, ""
if a[0] == "api":
    endpoint = next(v for v in a if v.startswith("repos/"))
    if endpoint.endswith("/git/ref/heads/master"):
        code = 1 if s.get("master_error") else 0
        output = s["master"]
    elif endpoint.endswith("/git/refs/tags/rolling"):
        assert a[1:3] == ["--method", "PATCH"], a
        assert a[-4:] == ["-f", "sha=" + os.environ["GITHUB_SHA"], "-F", "force=false"], a
        code = 1 if s.get("patch_error") else 0
        if not code:
            s["tag"] = os.environ["GITHUB_SHA"]
    elif endpoint.endswith("/git/ref/tags/rolling"):
        code = 1 if s.get("verify_error") else 0
        output = "c" * 40 if s.get("verify_mismatch") else s["tag"]
    else:
        raise AssertionError(a)
elif a[:3] == ["release", "view", "rolling"]:
    code = 0 if s["exists"] else 1
elif a[:3] == ["release", "create", "rolling"]:
    s["exists"] = True
    s["tag"] = os.environ["GITHUB_SHA"]
elif a[:3] == ["release", "edit", "rolling"]:
    # GitHub ignores target_commitish when the tag already exists.
    pass
elif a[:3] == ["release", "upload", "rolling"]:
    code = 1 if s.get("upload_error") else 0
else:
    raise AssertionError(a)
p.write_text(json.dumps(s))
print(output)
sys.exit(code)
'''


def run_case(name, expected, **overrides):
    with tempfile.TemporaryDirectory(prefix="sms-release-") as td:
        tmp = Path(td)
        state = {"master": SHA, "tag": OLD, "exists": True, "calls": []}
        state.update(overrides)
        state_file = tmp / "state.json"
        state_file.write_text(json.dumps(state))
        (tmp / "fake.py").write_text(FAKE)
        (tmp / "dist").mkdir()
        for asset in ("Simple-Media-System.elf", "Simple-Media-System-packed.elf",
                      "Simple-Media-System-DIAG.elf", "Simple-Media-System.zip",
                      "Simple-Media-System-languages.zip"):
            (tmp / "dist" / asset).write_bytes(b"fixture")
        # Use fail-fast Bash behavior as on GitHub Actions.
        # A shell function also works with Python's Windows path under Git Bash.
        prefix = 'gh() { "$SMS_RELEASE_PYTHON" "$SMS_RELEASE_FAKE" "$@"; }\n'
        (tmp / "step.sh").write_bytes((prefix + script + "\n").encode("utf-8"))
        env = dict(os.environ, GITHUB_SHA=SHA, GITHUB_REPOSITORY="example/SMS",
                   SMS_RELEASE_STATE=state_file.as_posix(), SMS_RELEASE_FAKE=(tmp / "fake.py").as_posix(),
                   SMS_RELEASE_PYTHON=Path(sys.executable).as_posix())
        result = subprocess.run([bash, "--noprofile", "--norc", "-eo", "pipefail", "step.sh"],
                                cwd=tmp, env=env, text=True, capture_output=True)
        state = json.loads(state_file.read_text())
        assert (result.returncode == 0) == expected, (name, result.returncode, result.stdout, result.stderr)
        calls = state["calls"]
        if name == "superseded":
            assert len(calls) == 1 and state["tag"] == OLD, calls
        elif expected:
            assert state["tag"] == SHA, (name, state)
            assert sum(a[:3] == ["release", "create", "rolling"] for a in calls) == (not overrides.get("exists", True))
            assert any(a[:3] == ["release", "upload", "rolling"] for a in calls), calls
            assert calls[-1][1].endswith("/git/ref/tags/rolling"), calls
        if overrides.get("master_error") or name in ("invalid-master-identity", "empty-master-identity"):
            assert len(calls) == 1, calls
        if overrides.get("upload_error"):
            assert not any("PATCH" in a for a in calls), calls
        print("PASS", name)


run_case("existing-stale-tag", True)
run_case("first-release", True, exists=False)
run_case("already-current", True, tag=SHA)
run_case("superseded", True, master="d" * 40)
run_case("master-api-failure", False, master_error=True)
run_case("invalid-master-identity", False, master="null")
run_case("empty-master-identity", False, master="")
run_case("upload-failure", False, upload_error=True)
run_case("tag-update-failure", False, patch_error=True)
run_case("tag-read-failure", False, verify_error=True)
run_case("tag-mismatch", False, verify_mismatch=True)
print("Rolling release workflow: 11 publication/failure controls passed")
