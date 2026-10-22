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
