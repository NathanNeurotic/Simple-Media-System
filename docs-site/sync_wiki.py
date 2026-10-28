#!/usr/bin/env python3
"""Preview or copy the generated SMS wiki into its existing clean Git checkout."""
import argparse
import importlib.util
import json
from pathlib import Path
import subprocess

HERE = Path(__file__).resolve().parent


def git(checkout, *args):
    return subprocess.check_output(["git", "-C", str(checkout), *args], text=True).strip()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--checkout", type=Path, required=True)
    parser.add_argument("--apply", action="store_true")
    args = parser.parse_args()
    checkout = args.checkout.resolve(strict=True)
    if Path(git(checkout, "rev-parse", "--show-toplevel")).resolve() != checkout:
        raise SystemExit("--checkout must point to the wiki Git root")
    remote = git(checkout, "remote", "get-url", "origin")
    if remote not in {"git@github.com:NathanNeurotic/Simple-Media-System.wiki.git", "https://github.com/NathanNeurotic/Simple-Media-System.wiki.git"}:
        raise SystemExit("Refusing to sync an unexpected wiki remote: " + remote)
    if git(checkout, "status", "--porcelain"):
        raise SystemExit("Wiki checkout has local changes; preserve/reconcile them before syncing")
    spec = importlib.util.spec_from_file_location("sms_docs", HERE / "build.py")
    build = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(build)
    manifest = json.loads((HERE / "manifest.json").read_text(encoding="utf-8"))
    products, search = build.products(manifest)
    build.validate(products, search, manifest)
    names = {p.name: data for p, data in products.items() if p.parent == build.WIKI}
    if args.apply:
        for name, data in names.items():
            (checkout / name).write_bytes(data)
    changes = [name for name, data in names.items() if not (checkout / name).is_file() or (checkout / name).read_bytes() != data]
    print(("Copied" if args.apply else "Would update") + f" {len(names) if args.apply else len(changes)} generated wiki files at {checkout}")
    if not args.apply:
        print("\n".join(sorted(changes)))
    else:
        print("Review git diff, commit and push normally. No files were deleted or pushed.")


if __name__ == "__main__":
    main()
