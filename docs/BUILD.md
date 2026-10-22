# Build on Windows

CODE READ: the owner resumed the build with the complete signed display package.
The earlier stripping failure and subsequent results are in [the evidence log](../notes/measurements.md).

Use an elevated PowerShell 7 terminal on a disposable Windows x64 machine with
50 GiB free disk (INFERRED recommendation) and 16 GiB host RAM for the default 8 GiB guest. Install Git, Python 3.11+, 7-Zip, and
Visual Studio 2022 Build Tools with the x64 C++ toolchain and Windows SDK. CI uses
the corresponding tools from `windows-2022`. ADK files are extracted from pinned
offline MSI packages, independently of any installed ADK. QEMU uses a silent
installer under `work/`. Read the vendors' licence terms before running the recipe.

```powershell
git clone https://github.com/reindertpelsma/win-gpu-image.git
cd win-gpu-image
python -m unittest discover -s tests -v
./scripts/Measure-Runner.ps1
./scripts/Fetch-Inputs.ps1
./scripts/Build-Image.ps1
./tests/Smoke-Boot.ps1 -MemoryMiB 8192 -MeasureMemoryFloor
python scripts/recipe.py package out/image.iso release (git rev-parse HEAD) local
python scripts/recipe.py verify release
```

During initial bootstrap, missing pins cause the fetch stage to record `WGI_INPUT`
metadata and deliberately fail before building. Only commit reviewed hash metadata
to `inputs.lock.json`; never the downloaded files. Subsequent builds enforce every
SHA-256 and refuse changed upstream bytes. Source URLs, resolved URLs, observation
dates, sizes and all payload hashes live in [inputs.lock.json](../inputs.lock.json).
The Windows evaluation ISO, ADK WinPE fwlink, and NVIDIA 580.88 URL come from the
2026-10-22 WP1 reproduction record. NVIDIA SHA-256 is
`90c49f925b41ee062e02cc8094a16b6d057abfde4e85235a76d63ffe267f63a9`;
the evaluation ISO SHA-256 is
`67cec5865eaa037a72ddc633a717a10a2bed50778862267223ddb9c60ef5da68`.
Other hashes are first-use observations, not independently authenticated hashes.

QEMU comes from the Windows distributor [linked by QEMU](https://www.qemu.org/download/),
with a versioned URL and pinned SHA-256. ADK installation follows Microsoft's
[offline layout instructions](https://learn.microsoft.com/en-us/windows-hardware/get-started/adk-offline-install).
DISM scratch is explicitly 512 MiB, the largest
[supported setting](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/dism-windows-pe-servicing-command-line-options).

See [measured results](../notes/measurements.md) for the current build status.

## VM RAM

CODE READ: CI first requires an 8192 MiB guest to reach the serial boot marker,
then measures 6144 and 4096 MiB until a lower size fails. `out/smoke.json`, the
manifest, and the job summary record the smallest successful tested allocation,
boot times and peak QEMU working set; guest available RAM is printed in the serial
log. This is a boot-only
minimum among tested sizes; it does not measure NVIDIA binding or GPU workloads.

INFERRED: provision at least 8192 MiB guest RAM for rented Vast GPU tests initially,
and leave additional host RAM for QEMU, the host OS, and the virtualization stack.
That GPU test minimum is not measured in this lane: no rented machine or GPU is
used. The full driver, compressed WIM, boot memory, 512 MiB scratch, and runtime
copies make the RAM requirement larger than stock PE. See the [image sizes](IMAGE.md)
and [measured memory results](../notes/measurements.md).

CODE READ: WIM export explicitly sets `/Bootable` and verifies boot index 1 plus
the boot metadata resource before ISO creation. The optional BIOS CD key prompt
is omitted for unattended boot. CI also attaches a fresh 64 MiB FAT result VHD;
only guest text logs are printed, and the VHD is never uploaded. A result-disk
marker cannot substitute for the required serial marker.
