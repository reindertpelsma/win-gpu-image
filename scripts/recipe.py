"""Small, standard-library-only helpers shared by local builds and CI."""
import argparse
import csv
from datetime import datetime, timezone
import hashlib
import json
import os
from pathlib import Path
import re
import shutil
import struct

PART_LIMIT = 2_040_000_000  # Below 1.9 GiB and GitHub's 2 GiB asset limit.


def sha256_file(path):
    with Path(path).open("rb") as f:
        return hashlib.file_digest(f, "sha256").hexdigest()


def verify_file(path, expected):
    if not re.fullmatch(r"[a-f0-9]{64}", expected or ""):
        raise ValueError(f"Missing/invalid SHA-256 pin: {path}")
    if sha256_file(path) != expected:
        raise ValueError(f"SHA-256 mismatch: {path}")


def verify_boot_wim(path):
    """Check the WinPE boot image index/resource in the standard WIM header."""
    path = Path(path)
    with path.open("rb") as f:
        header = f.read(208)
    if len(header) < 208 or header[:8] != b"MSWIM\0\0\0":
        raise ValueError("Invalid/truncated WIM header")
    images = struct.unpack_from("<I", header, 44)[0]
    boot_index = struct.unpack_from("<I", header, 120)[0]
    resource_size_flags, resource_offset = struct.unpack_from("<QQ", header, 96)
    resource_size = resource_size_flags & ((1 << 56) - 1)
    if images != 1 or boot_index != 1 or not resource_size or resource_offset + resource_size > path.stat().st_size:
        raise ValueError(f"WinPE WIM is not marked bootable: images={images} boot_index={boot_index}")
    return {"images": images, "boot_index": boot_index, "boot_metadata_bytes": resource_size,
            "image_bytes": path.stat().st_size}


def split_file(source, directory, limit=PART_LIMIT):
    if not 0 < limit <= PART_LIMIT:
        raise ValueError("Invalid part limit")
    source, directory = Path(source), Path(directory)
    directory.mkdir(parents=True, exist_ok=True)
    parts = []
    with source.open("rb") as src:
        index = 0
        while True:
            first = src.read(min(limit, 8 * 1024**2))
            if not first:
                break
            p = directory / f"{source.name}.part{index:02d}"
            with p.open("wb") as dst:
                dst.write(first)
                left = limit - len(first)
                while left:
                    block = src.read(min(left, 8 * 1024**2))
                    if not block:
                        break
                    dst.write(block)
                    left -= len(block)
            parts.append(p)
            index += 1
    if not parts:
        raise ValueError("Empty image")
    return parts


def verify_manifest(manifest, directory):
    h = hashlib.sha256()
    total = 0
    names = set()
    if not manifest.get("parts"):
        raise ValueError("No image parts")
    for part in manifest["parts"]:
        name = part["name"]
        if not re.fullmatch(r"image\.[a-z0-9]+(?:\.part[0-9]{2,})?", name) or name in names:
            raise ValueError("Unsafe or duplicate image part name")
        names.add(name)
        path = Path(directory) / name
        if path.stat().st_size != part["bytes"] or part["bytes"] > PART_LIMIT:
            raise ValueError("Part size mismatch")
        verify_file(path, part["sha256"])
        with path.open("rb") as f:
            while block := f.read(8 * 1024**2):
                h.update(block)
                total += len(block)
    if total != manifest["image_bytes"] or h.hexdigest() != manifest["image_sha256"]:
        raise ValueError("Reconstructed image differs from manifest")


def driver_component(relative, cpl_files=()):
    name = Path(relative).name.lower()
    parts = [x.lower() for x in Path(relative).parts]
    if name.endswith(".inf") and name != "nv_dispi.inf":
        return "other OEM INFs"
    if any(x in parts for x in ("nvcpl", "nvcamera", "localization")) or name in cpl_files:
        return "control panel/Ansel/localization"
    if (name.startswith("lib") and ".so" in name) or name in ("nvidia-smi", "nvidia-ngx-updater"):
        return "Linux/WSL"
    if name.endswith(".nvi") or name in ("setup.exe", "displaydriverext.dll"):
        return "installer"
    if re.search(r"physx|hdaudio|gfexperience|geforceexperience", str(relative), re.I):
        return "PhysX/audio/experience"
    if name.endswith(".sys"):
        return "kernel"
    if name.startswith("gsp_"):
        return "GSP firmware"
    if any(x in name for x in ("cuda", "ptx")) or name.startswith(("nvvm", "nvcubin", "nvopencl")):
        return "CUDA/OpenCL"
    if name.startswith(("nvml", "nvidia-smi")):
        return "NVML/SMI"
    if name.startswith(("nvoglv", "vulkan", "nv-vk", "nvvksc")):
        return "GL/Vulkan"
    if name.startswith(("nvd3d", "nvwgf", "nvdx", "nvgpucomp", "nvldumd", "nvpresent", "nvrtum", "nvdirectsr", "nvdlpp")):
        return "D3D/compiler"
    return "INF/catalog/other dependencies"


def stage_driver(source, destination, sizes):
    """Preserve every byte in Display.Driver; component names are accounting only."""
    source, destination = Path(source), Path(destination)
    cpl = set()
    section = ""
    for line in (source / "nv_dispi.inf").read_text(encoding="utf-8-sig").splitlines():
        line = line.split(";", 1)[0].strip()
        if line.startswith("["):
            section = line.lower()
        elif section.startswith("[nv_cpl") and "copy" in section and line and "=" not in line:
            row = next(csv.reader([line], skipinitialspace=True))
            cpl.add((row[1] if len(row) > 1 and row[1].strip() else row[0]).strip().lower())
    groups = {}
    records = []
    for p in sorted(source.rglob("*")):
        if not p.is_file():
            continue
        rel = p.relative_to(source)
        group = driver_component(rel, cpl)
        key = "KEEP " + group
        groups[key] = groups.get(key, 0) + p.stat().st_size
        digest = sha256_file(p)
        dest = destination / rel
        dest.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(p, dest)
        verify_file(dest, digest)
        records.append({"file": rel.as_posix(), "bytes": p.stat().st_size, "keep": True, "group": group, "sha256": digest})
    missing = missing_inf_sources(source / "nv_dispi.inf", source, destination)
    if missing:
        raise ValueError("Unmodified display package lost INF-referenced source files")
    value = {"policy": "Complete unmodified Display.Driver; separate archive packages not extracted",
             "components_bytes": groups, "files": records,
             "retained_bytes": sum(x["bytes"] for x in records if x["keep"]),
             "removed_bytes": sum(x["bytes"] for x in records if not x["keep"])}
    Path(sizes).write_text(json.dumps(value, indent=2) + "\n")
    print(json.dumps({k: v for k, v in value.items() if k != "files"}, sort_keys=True))


def missing_inf_sources(inf, source, staged):
    """Report original SourceDisksFiles entries absent from the staged package."""
    source, staged = Path(source), Path(staged)
    originals = {p.relative_to(source).as_posix().lower(): p for p in source.rglob("*") if p.is_file()}
    kept = {p.relative_to(staged).as_posix().lower() for p in staged.rglob("*") if p.is_file()}
    section = ""
    missing = []
    for line in Path(inf).read_text(encoding="utf-8-sig").splitlines():
        line = line.split(";", 1)[0].strip()
        if line.startswith("["):
            section = line.lower()
        elif section.startswith("[sourcedisksfiles") and "=" in line:
            name, value = line.split("=", 1)
            fields = next(csv.reader([value], skipinitialspace=True))
            subdir = fields[1].strip().replace("\\", "/") if len(fields) > 1 else ""
            relative = (Path(subdir) / name.strip()).as_posix().lower()
            if relative in originals and relative not in kept:
                p = originals[relative]
                missing.append({"file": p.relative_to(source).as_posix(), "bytes": p.stat().st_size})
    return missing


def main():
    parser = argparse.ArgumentParser()
    sub = parser.add_subparsers(dest="command", required=True)
    p = sub.add_parser("stage-driver")
    p.add_argument("source"); p.add_argument("destination"); p.add_argument("sizes")
    p = sub.add_parser("package")
    p.add_argument("image"); p.add_argument("directory"); p.add_argument("commit"); p.add_argument("run_url")
    p = sub.add_parser("verify")
    p.add_argument("directory")
    p = sub.add_parser("driver-closure")
    p.add_argument("source"); p.add_argument("staged")
    p = sub.add_parser("verify-wim")
    p.add_argument("image")
    args = parser.parse_args()
    if args.command == "stage-driver":
        stage_driver(args.source, args.destination, args.sizes)
    elif args.command == "verify-wim":
        print("WIM_BOOT_METADATA_OK " + json.dumps(verify_boot_wim(args.image), sort_keys=True))
    elif args.command == "driver-closure":
        missing = missing_inf_sources(Path(args.source) / "nv_dispi.inf", args.source, args.staged)
        print("REMOVED_INF_SOURCE_FILES " + json.dumps(missing, separators=(",", ":")))
        print(f"REMOVED_INF_SOURCE_COUNT={len(missing)} BYTES={sum(x['bytes'] for x in missing)}")
    elif args.command == "verify":
        d = Path(args.directory)
        verify_manifest(json.loads((d / "manifest.json").read_text()), d)
        print("MANIFEST_OK")
    else:
        image = Path(args.image)
        d = Path(args.directory)
        parts = split_file(image, d)
        lock = json.loads(Path("inputs.lock.json").read_text())
        manifest = {"schema": 1, "image_sha256": sha256_file(image), "image_bytes": image.stat().st_size,
                    "repo_commit": args.commit, "run_url": args.run_url,
                    "build_date": datetime.now(timezone.utc).isoformat(), "input_hashes": lock["files"],
                    "purpose": "Prebuilt GPU tests and sweeps on rented Vast.ai test boxes, CI-triggered or manual",
                    "trust_anchor": "Public GitHub build attestation plus expected SHA-256 in consumer reviewed lock file; store metadata is convenience only",
                    "memory_measurements": json.loads(Path("out/smoke.json").read_text()),
                    "build_measurements": json.loads(Path("out/build-metrics.json").read_text()),
                    "parts": [{"name": x.name, "bytes": x.stat().st_size, "sha256": sha256_file(x)} for x in parts],
                    "sizes": json.loads(Path("out/sizes.json").read_text())}
        verify_manifest(manifest, d)
        (d / "manifest.json").write_text(json.dumps(manifest, indent=2) + "\n")
        assets = parts + [d / "manifest.json"]
        (d / "SHA256SUMS").write_text("".join(f"{sha256_file(x)}  {x.name}\n" for x in assets))
        print(f"IMAGE_SHA256={manifest['image_sha256']} IMAGE_BYTES={manifest['image_bytes']}")
        print(f"RELEASE_TAG=img-{manifest['image_sha256'][:16]}")
        if os.environ.get("GITHUB_STEP_SUMMARY"):
            lines = ["## Windows PE build", "", f"Image SHA-256: `{manifest['image_sha256']}`",
                     f"Image bytes: {manifest['image_bytes']}", "",
                     "Expected SHA-256 must come from the consumer's reviewed lock file.",
                     "The public attestation is the provenance record; the store is a convenience copy.", "",
                     "| Input | SHA-256 |", "| --- | --- |"]
            lines += [f"| `{row['path']}` | `{row['sha256']}` |" for row in lock["files"]]
            lines += ["", "| Component | Bytes | MiB |", "| --- | ---: | ---: |"]
            lines += [f"| {name} | {size} | {size / 1048576:.3f} |"
                      for name, size in manifest["sizes"]["components_bytes"].items()]
            with open(os.environ["GITHUB_STEP_SUMMARY"], "a", encoding="utf-8") as summary:
                summary.write("\n".join(lines) + "\n")


if __name__ == "__main__":
    main()
