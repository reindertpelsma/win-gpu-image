"""Extract pinned CAB members using read-only MSI tables, without executing MSI."""
import json
from pathlib import Path
import subprocess
import sys


def file_plan(tables):
    directories = {x["Directory"]: x for x in tables["Directory"]}
    components = {x["Component"]: x["Directory_"] for x in tables["Component"]}
    media = sorted(tables["Media"], key=lambda x: int(x["LastSequence"]))

    def directory(key):
        if not key:
            return Path()
        row = directories[key]
        # MSI target:source and short|long forms; use the target long name.
        name = row["DefaultDir"].split(":", 1)[0].split("|")[-1]
        if key == "TARGETDIR" or name in (".", "SourceDir"):
            name = ""
        return directory(row["Directory_Parent"]) / name

    plan = []
    for row in tables["File"]:
        target = directory(components[row["Component_"]]) / row["FileName"].split("|")[-1]
        if "amd64" not in target.parts:
            continue
        if target.is_absolute() or ".." in target.parts:
            raise ValueError("Unsafe MSI path")
        cabinet = next(x["Cabinet"] for x in media if int(x["LastSequence"]) >= int(row["Sequence"]))
        if not cabinet.endswith(".cab") or cabinet.startswith("#") or Path(cabinet).name != cabinet:
            raise ValueError("Expected an external cabinet from the pinned layout")
        plan.append({"cabinet": cabinet, "member": row["File"], "target": target.as_posix(), "bytes": int(row["FileSize"])})
    return plan


def main():
    metadata, source, destination, seven = sys.argv[1:]
    plan = file_plan(json.loads(Path(metadata).read_text(encoding="utf-8-sig")))
    for cab in sorted({x["cabinet"] for x in plan}):
        extracted = Path("work/cabinets") / Path(cab).stem
        extracted.mkdir(parents=True, exist_ok=True)
        if not any(extracted.iterdir()):
            subprocess.run([seven, "x", "-y", "-bd", f"-o{extracted.resolve()}", str(Path(source) / cab)], check=True, stdout=subprocess.DEVNULL)
        for row in [x for x in plan if x["cabinet"] == cab]:
            src = extracted / row["member"]
            if src.stat().st_size != row["bytes"]:
                raise ValueError(f"MSI member size mismatch: {src}")
            dest = Path(destination) / row["target"]
            dest.parent.mkdir(parents=True, exist_ok=True)
            import shutil
            shutil.copyfile(src, dest)
    print(f"CAB_EXTRACT_OK amd64_files={len(plan)} bytes={sum(x['bytes'] for x in plan)}")


if __name__ == "__main__":
    main()
