# Image and probes

CODE READ: this section describes the implemented recipe and probes. Image startup
and GPU behavior remain unverified; see [the evidence log](../notes/measurements.md).

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
| Compressed WIM | 1,493,575,908 | MEASURED run 7; before explicit boot-metadata correction |
| Bootable ISO candidate | 1,549,445,120 | MEASURED run 7; failed serial boot test, never published |

The roughly 2.6 GB driver payload makes the PE WIM/ramdisk large. CI starts at
8192 MiB guest RAM and tests lower allocations; the smallest successful tested
boot size will be recorded in the manifest and evidence log. INFERRED: start Vast
GPU tests with at least 8192 MiB guest RAM; a GPU-test minimum is not measured here.
See [build and memory instructions](BUILD.md#vm-ram). The [evidence log](../notes/measurements.md)
retains the earlier component table and will record the full-package measurements.
MEASURED: run 7 timed out at 8192 MiB without a serial marker, so no successful
boot minimum is established yet. Run 8 validates boot metadata and captures a
guest result disk as diagnostics; COM1 remains the mandatory success criterion.

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

See [measured results](../notes/measurements.md) for this repository's servicing results and sizes.
