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
