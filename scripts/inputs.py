"""Fetch exact bytes; discovery records public metadata only and cannot build."""
from concurrent.futures import ThreadPoolExecutor
from datetime import datetime, timezone
import json
from pathlib import Path
import shutil
import sys
import urllib.parse
import urllib.request
from recipe import sha256_file, verify_file

mode = sys.argv[1]
lock = json.loads(Path("inputs.lock.json").read_text())
cache = Path("cache")
candidate = cache / "candidate.lock.json"


def fetch(row):
    p = cache / row["path"]
    p.parent.mkdir(parents=True, exist_ok=True)
    if not p.exists():
        with urllib.request.urlopen(row.get("resolved_url", row["url"]), timeout=120) as response:
            row["resolved_url"] = response.url
            with p.open("wb") as out:
                shutil.copyfileobj(response, out, 8 * 1024**2)
    if row.get("sha256"):
        verify_file(p, row["sha256"])
    else:
        row["sha256"] = sha256_file(p)
        row["observed_utc"] = datetime.now(timezone.utc).isoformat()
        row["observation"] = "First-use download on Windows runner; no independent authenticity claim"
    row["bytes"] = p.stat().st_size
    print(f"INPUT_OK {row['path']} {row['sha256']} {row['bytes']}", flush=True)
    return row


if mode == "fetch":
    discovery = any(not x.get("sha256") for x in lock["files"])
    with ThreadPoolExecutor(max_workers=3) as executor:
        lock["files"] = list(executor.map(fetch, lock["files"]))
    candidate.write_text(json.dumps(lock, indent=2) + "\n")
    # The PowerShell caller reads this flag, never a secret.
    (cache / "first-use.txt").write_text(str(discovery).lower())
elif mode == "record-layouts":
    lock = json.loads(candidate.read_text())
    existing = {x["path"] for x in lock["files"]}
    for kind, exe in [("adk", "adksetup.exe"), ("winpe", "adkwinpesetup.exe")]:
        setup = next(x for x in lock["files"] if x["path"] == f"{kind}/{exe}")
        base = setup["resolved_url"].rsplit("/", 1)[0] + "/"
        for p in sorted((cache / kind).rglob("*")):
            if not p.is_file() or p.relative_to(cache).as_posix() in existing:
                continue
            rel = p.relative_to(cache).as_posix()
            row = {"path": rel, "url": base + urllib.parse.quote(p.relative_to(cache / kind).as_posix()),
                   "sha256": sha256_file(p), "bytes": p.stat().st_size,
                   "observed_utc": datetime.now(timezone.utc).isoformat(),
                   "observation": "First-use ADK layout on Windows runner"}
            lock["files"].append(row)
    candidate.write_text(json.dumps(lock, indent=2) + "\n")
    for row in lock["files"]:
        print("WGI_INPUT " + json.dumps(row, separators=(",", ":")), flush=True)
else:
    raise SystemExit("Unknown mode")
