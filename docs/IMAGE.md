# Image and probes

MEASURED: [run 10](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39848309657)
passed the result-disk boot gate and offline NVIDIA display-driver check at
8192 MiB, then attested and published to the private store. COM1 returned error 2
and remains optional logging. NVIDIA GPU behavior remains unverified.
See [the evidence log](../notes/measurements.md).

The build preserves **the entire unmodified `Display.Driver` directory**, including
all files referenced by the signed INF/catalog. Control-panel, Ansel, localization,
and Linux/WSL files inside that directory remain present. Every staged file is
checked against its source SHA-256. No INF, catalog, driver, or user-mode binary is
patched. `dism /Add-Driver` services the WIM offline; `/Get-Drivers` must identify
the NVIDIA display package before commit.

Whole separate archive packages outside `Display.Driver` are not extracted. This
can exclude standalone HDAudio, PhysX, experience, and control-panel installers;
it never removes dependencies referenced from inside the display package. This
policy supersedes the unsuccessful stripping experiment in runs 4 and 5.

The Windows evaluation ISO supplies Microsoft graphics DLLs for the offscreen
probe; this combination remains experimental rather than a supported OS.

| Payload | Bytes | Status |
| --- | ---: | --- |
| Complete Display.Driver source | 2,692,847,387 | MEASURED source inventory in run 4; now all retained |
| Formerly stripped subset | 1,929,774,713 | Historical, rejected by DISM |
| Files restored by the owner decision | 763,072,674 | Includes 702,161,478 bytes referenced by the INF |
| Imported DriverStore payload | 2,631,764,223 | MEASURED run 10; 202 files |
| Compressed WIM | 1,493,573,627 | MEASURED run 10; boot index 1 verified |
| Published ISO | 1,549,436,928 | MEASURED run 10; boot gate, hash consistency, attestation and private publish passed |

The roughly 2.6 GB driver payload makes the PE WIM/ramdisk large. CI uses
**8192 MiB guest RAM as its tested default, not a minimum**. Run 10's
result disk proves startup at that allocation, **not a measured minimum**. No
lower-RAM trials ran, and none are scheduled. QEMU reached the startup marker and
shut down after probes; this is a pass/fail boot check. Runtime observations are
kept only in the operational evidence log, with no performance claim.

INFERRED: start Vast GPU tests with 8192 MiB guest RAM provisionally;
the GPU-test minimum remains unmeasured. No rented machine or GPU was used.
See [build and memory instructions](BUILD.md#vm-ram) and the complete component
and process-memory tables in [the evidence log](../notes/measurements.md).

CODE READ: after PnP initialization, startup writes `WGI_PE_BOOT_OK_V1` to
`wgi-boot.txt` on a writable disk with `WGI_RESULTS.TAG`. The host checks this exact
guest-written file and the offline DISM NVIDIA display-driver listing. Diagnostic
`wgi-results.txt` and COM1 (115200 8N1) are best-effort logging. MEASURED in run 8:
COM1 open returned error 2 and stock `msports.inf` loading returned `0x80070103`.
The owner approved the result-disk gate and ended COM enumeration experiments.

Startup loads the offline NVIDIA INF, prints `nvidia-smi`, and runs bundled probes
or probes on a second disk with `WGI_TESTS.TAG`, then idles (or shuts down when the
CI-only `WGI_SMOKE.TAG` is present). Run 8 captured NVIDIA
INF load return code 0, SMI return code 9, the D3D11 NVIDIA-vendor assertion failure,
and CUDA `LoadLibrary` error 126. There is no NVIDIA GPU in CI: these results do
not establish binding, CUDA execution, or graphics rendering on NVIDIA hardware.

The D3D11 probe asserts VendorId `0x10DE` and renders/reads an offscreen texture.
Probes use MSVC `/MT` for a static CRT. An original CUDA driver-API query loads
`nvcuda.dll` dynamically; no CUDA SDK or NVIDIA `deviceQuery` binary is redistributed.
The upstream CUDA samples `deviceQuery` is deliberately not included: it requires
a separately licensed toolkit, and this recipe does not claim redistribution clearance.

See [measured results](../notes/measurements.md) for this repository's servicing results and sizes.
