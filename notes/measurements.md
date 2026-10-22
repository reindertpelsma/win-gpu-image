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
Historical WP1 package sizes are labeled in README; they are not new CI measurements.
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
