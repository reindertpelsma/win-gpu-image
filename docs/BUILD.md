# Build on Windows

MEASURED: offline NVIDIA servicing of the stripped package fails; see
[the evidence log](../notes/measurements.md) before running these commands.

Use an elevated PowerShell 7 terminal on a disposable Windows x64 machine with
at least 30 GiB free disk and 8 GiB RAM. Install Git, Python 3.11+, 7-Zip, and
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
./tests/Smoke-Boot.ps1
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
