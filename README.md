# win-gpu-image

A standalone recipe for a Windows PE ISO with an offline-serviced NVIDIA display
driver, GPU probes, and a serial boot test. A minimal Windows disk image is future
work. The public repository contains source and recipes only. The private release
store holds a convenience copy; licence review of that copy is open.

**Bootstrap in progress:** no successful build or GPU compatibility claim yet.
See [measured results](notes/measurements.md) for evidence and run links.

Do not open pull requests that add binaries. We do not redistribute Microsoft or
NVIDIA files through this repository, public releases, or workflow artifacts.
Obtain vendor inputs directly under their respective terms. No licence grant for
vendor files is implied by this recipe. See [SECURITY.md](SECURITY.md).

## Build on Windows

Use an elevated PowerShell 7 terminal on a disposable Windows x64 machine with
at least 30 GiB free disk and 8 GiB RAM. Install Git, Python 3.11+, 7-Zip, and
Visual Studio 2022 Build Tools with the x64 C++ toolchain and Windows SDK. CI uses
the corresponding tools from `windows-2022`. Installing ADK tools changes the
local machine; read Microsoft's licence terms before using its silent installer.

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
dates, sizes and all payload hashes live in [inputs.lock.json](inputs.lock.json).
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

## Image and probes

The build stages only `Display.Driver`, keeps CUDA, NVML/SMI, D3D, OpenGL, Vulkan,
kernel and firmware dependencies, and excludes control panel, Ansel, PhysX, audio,
experience/installer, localization, Linux files and unrelated OEM INFs. INF and
catalog bytes stay unchanged. `dism /Add-Driver` services the WIM offline; a separate
`/Get-Drivers` check must identify the NVIDIA display package before commit.
The Windows evaluation ISO supplies matching Microsoft graphics DLLs for the
offscreen probe; this combination remains experimental rather than a supported OS.

Historical WP1 source measurements (MiB, 1 MiB = 1,048,576 bytes):

| Retained | MiB | Removed | MiB |
| --- | ---: | --- | ---: |
| D3D/compiler | 759.388 | NVCPL | 148.506 |
| CUDA/OpenCL | 433.021 | Ansel | 59.321 |
| GL/Vulkan | 84.401 | Linux | 466.645 |
| Kernel | 105.385 | OEM INFs | 51.303 |
| GSP | 100.159 | Installer | 1.948 |
| NVML/SMI | 5.907 | Total | 727.723 |
| Other + INF/catalog | 352.114 | PhysX/audio/experience | Not extracted |
| Total | 1840.377 | | |

Those are historical package measurements, not this build's image size. Each new
build prints its own component counts and byte sizes and places them in its manifest.

At startup PE initializes PnP, emits `WGI_PE_BOOT_OK_V1` to COM1 (115200 8N1),
loads the offline driver, prints `nvidia-smi`, and runs GPU probes from a second
disk when `WGI_TESTS.TAG` is present. A writable disk with `WGI_RESULTS.TAG` receives
`wgi-results.txt`; output also goes to serial. PE then idles. This supports a
physical GPU test without treating CI's emulated display as an NVIDIA adapter.

The D3D11 probe asserts VendorId `0x10DE` and renders/reads an offscreen texture.
Probes use MSVC `/MT` for a static CRT. An original CUDA driver-API query loads
`nvcuda.dll` dynamically; no CUDA SDK or NVIDIA `deviceQuery` binary is redistributed.
The upstream CUDA samples `deviceQuery` is deliberately not included: it requires
a separately licensed toolkit, and this recipe does not claim redistribution clearance.

## Verification and trust

The QEMU TCG smoke test boots the freshly built ISO without a GPU or network and
requires a fixed serial marker within a deadline. It proves PE reached its startup
code. It does **not** prove NVIDIA binding, CUDA, D3D, performance, or hardware support.
Those need a separate real GPU run. Hypotheses and falsifiers appear in test headers.

The final job computes the image SHA-256, verifies release-part reconstruction,
creates GitHub build provenance for the ISO digest, then publishes only to the
private store. Public provenance contains the digest and build identity, no image
content. Releases use `img-<first 16 hex of SHA-256>` and parts below 1.9 GiB,
plus `manifest.json` and `SHA256SUMS`. Reassemble parts in manifest order and verify:

```bash
./scripts/verify-release.sh /path/to/downloaded-release
gh attestation verify /path/to/reassembled/image.iso --repo reindertpelsma/win-gpu-image
```

Input pins detect byte changes after first use. Attestation links a digest to the
workflow/repo identity; it does not establish vendor authenticity, safety, licensing,
bit-for-bit reproducibility, or GPU correctness. WIM metadata and build timestamps
can change output hashes between builds. The recipe is reproducible as a procedure;
bit-for-bit reproducibility is not established. Trust includes Microsoft, NVIDIA,
QEMU's Windows distributor, hosted runner/toolchain, GitHub, action commits, and
reviewed recipe changes. See [SECURITY.md](SECURITY.md) for owner configuration.
