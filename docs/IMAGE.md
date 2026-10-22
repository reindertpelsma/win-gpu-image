# Image and probes

MEASURED: PE startup was observed at 8192 MiB through its result disk in run 8.
The mandatory COM1 assertion failed; NVIDIA GPU behavior remains unverified.
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
| Imported DriverStore payload | 2,631,764,223 | MEASURED run 7; 202 files |
| Compressed WIM | 1,493,573,555 | MEASURED run 8; boot index 1 verified |
| ISO candidate | 1,549,436,928 | MEASURED run 8; PE starts, serial gate fails; never published |

The roughly 2.6 GB driver payload makes the PE WIM/ramdisk large. CI starts at
8192 MiB guest RAM and attempts lower sizes only after serial success. Run 8's
result disk proves startup at 8192 MiB, with 8,589,393,920 guest physical bytes
reported and 6,651,428,864 bytes available at startup. That is an observed
allocation, **not a measured minimum**; 6144/4096 MiB were not tried because COM1
was unavailable. A serial-qualified boot minimum has not been established.

INFERRED: start Vast GPU tests with at least 8192 MiB guest RAM provisionally;
the GPU-test minimum remains unmeasured. No rented machine or GPU was used.
See [build and memory instructions](BUILD.md#vm-ram) and the complete component
and process-memory tables in [the evidence log](../notes/measurements.md).

CODE READ: startup initializes PnP, loads the stock serial INF, and attempts to
emit `WGI_PE_BOOT_OK_V1` to COM1 (115200 8N1). A writable disk with
`WGI_RESULTS.TAG` receives `wgi-results.txt`; this path was verified in run 8.
COM1 open returned error 2 and the stock serial INF load returned `0x80070103`,
so the serial test remains red despite the result-disk boot marker.

Startup loads the offline NVIDIA INF, prints `nvidia-smi`, and runs bundled probes
or probes on a second disk with `WGI_TESTS.TAG`, then idles. Run 8 captured NVIDIA
INF load return code 0, SMI return code 9, the D3D11 NVIDIA-vendor assertion failure,
and CUDA `LoadLibrary` error 126. There is no NVIDIA GPU in CI: these results do
not establish binding, CUDA execution, or graphics rendering on NVIDIA hardware.

The D3D11 probe asserts VendorId `0x10DE` and renders/reads an offscreen texture.
Probes use MSVC `/MT` for a static CRT. An original CUDA driver-API query loads
`nvcuda.dll` dynamically; no CUDA SDK or NVIDIA `deviceQuery` binary is redistributed.
The upstream CUDA samples `deviceQuery` is deliberately not included: it requires
a separately licensed toolkit, and this recipe does not claim redistribution clearance.

See [measured results](../notes/measurements.md) for this repository's servicing results and sizes.
