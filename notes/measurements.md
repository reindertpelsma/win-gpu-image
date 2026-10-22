# Evidence

MEASURED, 2026-10-22, [run 1](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39826412920):
Windows Server 2022 10.0.20348; visible RAM 16,771,836 KiB, free RAM 14,554,572 KiB;
C: free 88,783,802,368 bytes, D: free 157,854,121,984 bytes. ADK directory present,
system DISM 10.0.20348.1, QEMU absent on PATH. HypervisorPlatform enabled,
HypervisorPresent true; QEMU acceleration support is not inferred from that alone.
ADK layout with feature selection failed exit 1001; no image built.

MEASURED, 2026-10-22, [run 2](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39827032414):
preinstalled ADK DISM 10.0.26100.4202. The documented `/quiet /layout` form succeeded.
318 first-use records totaled 9.101 GiB, including two locally generated
`UserExperienceManifest.xml` files which are not downloadable (HTTP 404) and are not
used by the recipe. The other 316 records form the pinned download lock. The run
deliberately stopped after recording hashes; no build, attestation, or publication.

MEASURED local, 2026-10-22: initial test commit intentionally failed with missing
implementation and workflow. Five implemented script/workflow checks subsequently pass.
PowerShell parser checks pass; all three native programs cross-compile using MinGW
(the actual Windows build uses MSVC `/MT`).
Historical WP1 package sizes are labeled in [IMAGE.md](../docs/IMAGE.md); they are not new CI measurements.
No NVIDIA binding claim follows from this repository's QEMU smoke test.

MEASURED, 2026-10-22, [run 3](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39827787651):
all 316 download pins matched; MSI administrative extraction failed exit 1603.

MEASURED, 2026-10-22, [run 4](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39828292923):
read-only MSI/CAB extraction succeeded, pinned DISM version 10.0.26100.9457;
all three programs compiled with MSVC. Stripping retained 1,929,774,713 bytes and
removed 763,072,674 bytes, matching the historical package measurements. Mounting
the evaluation WIM and PE WIM succeeded. `/Add-Driver` of the stripped source
failed error 2 (file not found); the image was discarded. QEMU, provenance and
publishing were not reached. The next run is the second and final servicing
attempt, with failure logs and a complete signed-package diagnostic control.

MEASURED read-only environment API, 2026-10-22: `push_to_image_store` exists;
`deployment_branch_policy` is null and `protection_rules` is empty. Owner must
configure the recommended `main`-only deployment branch rule; no settings changed.

MEASURED source package table from run 4 (before DISM; not a generated image):

| Component | Bytes | MiB |
| --- | ---: | ---: |
| DROP Linux/WSL | 489,313,152 | 466.645 |
| DROP control panel/Ansel/localization | 217,921,904 | 207.827 |
| DROP installer | 2,042,378 | 1.948 |
| DROP other OEM INFs | 53,795,240 | 51.303 |
| KEEP CUDA/OpenCL | 454,055,316 | 433.021 |
| KEEP D3D/compiler | 796,276,288 | 759.388 |
| KEEP GL/Vulkan | 88,501,319 | 84.401 |
| KEEP GSP firmware | 105,024,584 | 100.159 |
| KEEP INF/catalog/other dependencies | 369,161,942 | 352.060 |
| KEEP NVML/SMI | 6,194,120 | 5.907 |
| KEEP kernel | 110,561,144 | 105.439 |
| Retained total | 1,929,774,713 | 1840.377 |
| Removed total | 763,072,674 | 727.723 |

CODE READ, 2026-10-22: the unchanged INF lists 200 `SourceDisksFiles` entries.
Comparing the pinned archive listing with the stripping rules finds 82 removed
referenced files, totaling 702,161,478 bytes (669.633 MiB), including control-panel,
Ansel, and Linux/WSL files. This source analysis supports the missing-file
hypothesis; the Windows servicing control determines the actual outcome.

MEASURED, 2026-10-22, [run 5](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39829506920):
the second candidate servicing attempt reproduced error 2 / `0x80070002`.
`setupapi.offline.log` identifies the missing source
`work/driver/libnvidia-opticalflow.so.1` during the DriverStore copy queue.
The runner confirmed 82 removed INF source entries totaling 702,161,478 bytes.
The complete signed `Display.Driver` source then passed `/Add-Driver` as a
**diagnostic control only**; `/Get-Drivers` listed `oem0.inf`, `nv_dispi.inf`,
class `Display`, provider `NVIDIA`. The control was discarded, never exported.

MEASURED: five workflow runs total. ADK layout and extraction walls were cleared
on their second attempts; signed-driver stripping failed both candidate attempts
(runs 4 and 5), so work stopped at that wall under the lane rule. No generated ISO,
image size/hash, QEMU execution, image attestation, or store release exists from
this bootstrap. The final store-secret step was never reached. Seven local
helper/workflow tests pass. No trusted host, owner VM, paid runner, or rented box
was used; no repository settings or environment configuration were changed.

INFERRED: the requested stripping recipe conflicts with importing this unchanged
signed INF package: DISM requires source files that the recipe removes. Continuing
needs an owner decision on retaining the required signed-package files or choosing
a different valid signed minimal-driver design. A full-package control is not
permission to publish an image that violates the stripping requirement.

CODE READ: the implemented QEMU smoke test would prove only that PE reached its
startup marker. It remains unexecuted here; NVIDIA binding and the CUDA/D3D probes
are also unverified and require a GPU. Compilation and offline package registration
are not evidence of GPU operation.

CODE READ: the final documentation-only commit uses `[skip ci]`, as requested by
the owner after the servicing wall. README is 55 lines; detailed sections moved
verbatim into `docs/BUILD.md`, `docs/IMAGE.md`, `docs/TRUST.md`, and `docs/WHY-PE.md`,
with relative-link corrections. No workflow or build code changed in that commit.

CODE READ, 2026-10-22, owner decision after run 5: retain the complete unmodified
signed display package and resume. Only whole unrelated packages outside the
display directory may be excluded. The previous stripping blocker is superseded;
its measured failure remains part of the record. Run 6 will measure complete
source/DriverStore/WIM/ISO sizes, boot, and RAM. The original six-hour/12-run ceiling
continues (five runs used before resuming).

CODE READ: smoke memory sweep starts at 8192 MiB and, after a successful boot,
tests 6144 then 4096 MiB, stopping at the first lower-size failure. INFERRED: the
provisional Vast GPU-test allocation is at least 8192 MiB; it remains unmeasured
because no rented box or GPU is authorized in this lane. Boot-only memory and host
QEMU working set will be reported separately from GPU workload requirements.

MEASURED, 2026-10-22, [run 6](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39833982265):
the complete 2,692,847,387-byte source package was copied and hash-verified with
zero files removed. Offline `/Add-Driver` and `/Get-Drivers` passed (NVIDIA Display,
32.0.15.8088); scratch was set/read back as 512 MiB. The size-measurement query
then failed because wildcard directory enumeration with `-File` produced no files
and `Measure-Object` had no `Sum` property. The image was discarded before export.
Run 7 corrects that query using resolved literal directories and adds a regression
check covering nested NVIDIA files and exclusion of unrelated driver repositories.

MEASURED, 2026-10-22, [run 7](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39835048860):
the size-query correction passed. Imported NVIDIA DriverStore: 202 files,
2,631,764,223 bytes. Compressed WIM: 1,493,575,908 bytes; ISO: 1,549,445,120 bytes.
QEMU 11.1.0 TCG at 8192 MiB did not emit the serial marker in 901.84 seconds.
Peak QEMU working set: 8,640,118,784 bytes; peak sampled private bytes:
10,107,531,264; minimum host free physical memory: 6,057,200 KiB. These are process
and host counters, not unique physical memory or a successful boot minimum.
Attestation and publication were skipped; no image hash was recorded by packaging.

CODE READ: run 8 explicitly uses DISM `/Export-Image /Bootable`, validates the WIM
boot index/resource before ISO creation, and omits the optional BIOS CD key prompt.
Microsoft [documents the export boot flag](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/dism-image-management-command-line-options-s14#export-image).
Startup explicitly loads the stock serial-port INF. A fresh 64 MiB VHD provides
diagnostic guest logs; the host still requires the COM1 marker, never accepts a
disk-only marker as success, and uploads no result disk or vendor-containing file.
This is the second candidate QEMU boot attempt after run 7's serial timeout.

MEASURED, 2026-10-22, [run 8](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39839169848):
WIM boot index 1 and 965,212-byte boot metadata resource passed the new gate.
Compressed WIM: 1,493,573,555 bytes; ISO: 1,549,436,928 bytes. The complete NVIDIA
source remains 2,692,847,387 bytes, and DISM imported 202 files / 2,631,764,223 bytes.
The fresh 64 MiB FAT VHD was created, attached to QEMU, and read back successfully.
It contained `WGI_PE_BOOT_OK_V1` and the GPU probe log, proving PE reached startup.

MEASURED: COM1 open returned Win32 error 2 (file not found); loading stock
`msports.inf` returned `0x80070103`. The mandatory serial marker remained absent
for 900.36 seconds at 8192 MiB, so the job failed. That duration is the serial
assertion deadline, not a measured boot duration: startup time was not timestamped.
No lower-RAM trial ran. No image SHA-256, attestation, or store release was created
because those stages follow the required serial test. The store secret step was
never reached. No image/result disk/vendor file was uploaded as a public artifact.

| Run 8 memory observation | Value | Scope |
| --- | ---: | --- |
| Configured guest RAM | 8192 MiB | PE startup observed via disk; serial gate failed |
| Guest total physical bytes | 8,589,393,920 | Native guest API at startup |
| Guest available physical bytes | 6,651,428,864 | First startup log, before probes |
| Peak QEMU working set bytes | 8,643,964,928 | Host process counter; not unique physical memory |
| Peak sampled QEMU private bytes | 10,133,381,120 | Host process counter |
| Minimum host free physical memory | 5,905,160 KiB | Sampled on the Windows runner |
| Smallest successful serial allocation | Not established | Primary serial assertion failed |
| Actual lowest PE startup allocation | Not established | Only 8192 MiB tried |
| Vast GPU-test RAM minimum | Not measured | No rented box/GPU used |

MEASURED: the guest loaded the NVIDIA INF with return code 0. SMI returned 9;
DXGI enumerated Microsoft adapters (`0x1414`), and the D3D11 probe rejected them
with its `0x10DE` assertion (return code 2). The CUDA query returned 1 after
`LoadLibrary` error 126. These are not NVIDIA binding or GPU-compatibility results.

CODE READ: the pinned stock PE SYSTEM hive has Serial/Serenum services present,
with `Start=3` (demand start). The exact missing-port enumeration/mapping cause
is unresolved; the available drivers and a valid WIM do not establish a COM1 device.
INFERRED: continuing requires a separately authorized serial-port enumeration/init
fix or an owner change to the boot-test contract. The test was not weakened.

MEASURED: eight workflow runs total; runs 7 and 8 both failed the COM1 assertion.
Work stopped at that two-attempt wall under AGENTS.md, within the original six-hour
and twelve-run ceilings. The final documentation-only commit uses `[skip ci]`;
no ninth run is requested. README remains 55 lines, with the full signed-package
policy and measured sizes documented. All eight Python tests, the PowerShell
DriverStore regression, and PowerShell syntax checks pass locally.

MEASURED read-only environment API recheck, 2026-10-22: deployment branch policy
remains null and protection rules remain empty. Owner must configure `main`-only
environment deployment and complete the licence review; no settings were changed.

## Owner-approved result-disk gate (after run 8)

CODE READ: the owner superseded the serial requirement: CI now requires the exact
`WGI_PE_BOOT_OK_V1` in guest-written `wgi-boot.txt` on the result disk, plus the
offline DISM NVIDIA display-driver check. The fresh host-created disk contains
only `WGI_RESULTS.TAG` and `WGI_SMOKE.TAG`, never the boot marker. The CI sentinel
requests guest shutdown after probe logging so the runner can detach QEMU and read
the disk; ordinary test boots still idle. Serial/COM1 is best-effort only.

MEASURED in run 8: `COM1_OPEN_FAILED error=2`; no further msports/COM enumeration
experiments are scheduled. CODE READ: the owner approved continuing to hash,
attestation, and store publishing, with four runs remaining. The 8192 MiB guest
allocation is the tested default, not a minimum; no lower-RAM trials are scheduled.
Vast/GPU workload memory remains unmeasured, and no rented box is used.

CODE READ: source change `7d2813de323fe9f0e83167c08862a0dabb04e23c` starts
[run 9](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39845846088).
Eight Python tests, both PowerShell fixture tests (including absent/corrupt marker
rejection), and PowerShell syntax checks passed locally before pushing.

### Run 9: boot, hash consistency, and attestation pass; publisher CLI flag error

MEASURED: the exact guest-written result-disk marker and offline NVIDIA display
check passed at 8192 MiB. QEMU exited 0 after 95.19 seconds, including probes and
shutdown; this is not an exact boot duration or a minimum RAM measurement. Peak
QEMU working set was 8,645,144,576 bytes, peak sampled private bytes 10,127,003,648,
and minimum host free memory 6,042,052 KiB. The first guest API report gave
8,589,393,920 total physical bytes and 6,645,489,664 available bytes.

MEASURED: WIM 1,493,573,721 bytes; ISO 1,549,438,976 bytes; offline DriverStore
2,631,764,223 bytes/202 files. Hash/part-manifest consistency passed. The digest and
all 316 input hashes are in the job summary. Public
[attestation 58805308](https://github.com/reindertpelsma/win-gpu-image/attestations/58805308)
was created, without uploading image content. Publishing then failed before
release creation: `gh api` rejects combining `--slurp` with `--jq`.

CODE READ: publishing now uses `--paginate --slurp` alone and flattens pages in
PowerShell. MEASURED: the real read-only private-store list call succeeds; empty
and later-page fixture cases preserve exact tag/draft state. Eight Python tests,
three PowerShell fixture tests, and syntax checks pass locally. Commit
`320f7f83309eddfff8d557feb0c385232f675412` starts the tenth workflow run to retry
the full gated path; no serial/RAM experiment is added.

## Run 10: first green, attested image published to the private store

MEASURED: [run 10](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39848309657)
is green on `main` at `320f7f83309eddfff8d557feb0c385232f675412`. The freshly built
ISO passed the exact guest-written result-disk boot marker and offline NVIDIA
display-driver check, hash/manifest consistency, public provenance attestation,
and the final private-store publish. Ten of twelve allowed workflow runs were
used; work remained within the original six-hour ceiling. No lower-RAM trial or
additional serial-device experiment was run.

MEASURED: public [attestation 58809172](https://github.com/reindertpelsma/win-gpu-image/attestations/58809172)
records the ISO digest. The private store release is `img-a650252c82f1c09a` with
`image.iso.part00`, `manifest.json`, and `SHA256SUMS`, all uploaded and non-draft.
The image is 1,549,436,928 bytes; the WIM is 1,493,573,627 bytes; imported NVIDIA
DriverStore payload is 2,631,764,223 bytes/202 files. All 264 source display-package
files (2,692,847,387 bytes) are staged byte-for-byte; separate packages are omitted.

| Run 10 memory observation | Value | Scope |
| --- | ---: | --- |
| Configured guest RAM | 8192 MiB | Tested default, not a minimum |
| Guest total physical bytes | 8,589,393,920 | Native guest API at startup |
| Guest available physical bytes | 6,647,599,104 | First startup log, before probes |
| Peak QEMU working set bytes | 8,646,565,888 | Host process counter, not unique physical memory |
| Peak sampled QEMU private bytes | 10,122,506,240 | Host process counter |
| Minimum host free physical memory | 5,864,124 KiB | Sampled on the Windows runner |
| Observation duration | 101.32 seconds | Through probe execution and clean QEMU exit 0; not exact boot time |
| Lowest boot allocation / Vast GPU RAM | Not measured | No lower-RAM or real GPU trials |

MEASURED: `COM1_OPEN_FAILED error=2` remains visible in the result log and does
not fail CI. NVIDIA INF load returns 0; SMI returns 9, D3D11 rejects the Microsoft
adapter, and CUDA DLL loading fails without an NVIDIA GPU. CODE READ: the green
boot gate proves startup and offline package inclusion, not NVIDIA binding,
compute/graphics execution, performance, or compatibility.

MEASURED read-only owner-setup recheck: environment deployment branch policy is
still null and protection rules are empty; owner must configure `main`-only
deployment and complete licence review. Public releases and Actions artifacts
remain zero. Repository files are source/recipe/metadata only; no vendor binary
extension or tracked file larger than 1 MiB is present. No settings were changed.

MEASURED: downloaded all three private release assets with the existing human
read login. `verify-release.sh` passed part/manifest checks and reassembly. The
reassembled image SHA-256 matches the public run output; manifest commit, all 316
input records, and boot evidence match the recipe/run. `gh attestation verify
<image> --repo reindertpelsma/win-gpu-image` exited 0; JSON output confirms one
verified SLSA provenance attestation for the same digest. Downloaded vendor/image
bytes remain private and are removed after verification; only evidence metadata
is retained locally. This final documentation change is separate, docs-only, and
uses `[skip ci]` through a branch/PR after the first green run.
