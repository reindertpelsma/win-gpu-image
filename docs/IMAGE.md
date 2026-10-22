# Image and probes

CODE READ: this section describes the implemented recipe and probes. Image startup
and GPU behavior remain unverified; see [the evidence log](../notes/measurements.md).

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

See [measured results](../notes/measurements.md) for this repository's servicing results and sizes.
