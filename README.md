# win-gpu-image

A standalone recipe for a throwaway Windows PE ISO for GPU validation.
The images contain modified Microsoft and NVIDIA binaries that copyright does not
allow us to publish, so the **build is public** but the **output stays in a private
store** (a convenience copy for rented Vast.ai test boxes); anyone else rebuilds
from the recipe.

## Purpose and intended use

This repository builds a throwaway Windows PE environment used only to test GPU
drivers and GPU virtualization stacks: boot it in a VM, load a GPU driver, run
small compute and graphics probes, and report pass or fail over serial. It is a
test environment, not a general-purpose operating system, not a deployment or
production image, and not a desktop. Images are rebuilt from vendor downloads
whenever the recipe or inputs change and are not meant to be kept or used for
anything else. Downstream projects (for example GPU virtualization tools) may run
the resulting image in CI and test sweeps on rented machines. Pass or fail here
says only that these probes ran. It makes no performance claim and no compatibility
guarantee.

Non-goals: running real workloads, hosting services, daily use, redistributing the
image or any vendor file.

Any future minimal-Windows disk image has the same testing-only purpose. Licence
review is open; this repository makes no claim that this use complies with
Microsoft or NVIDIA terms.

## Why a PE instead of a full Windows install

Full installation/setup can take about an hour (**estimate**); PE boot was about
50–70 s (**MEASURED in downstream testing**, in a VM on a different host).
PE starts clean from RAM without activation/OOBE/accounts; the complete signed driver adds about 2.6 GB.
NVIDIA-in-PE remains unproven; it complements full Windows. [Full comparison and planned middle option](docs/WHY-PE.md).

## Build it yourself

Use an elevated Windows PowerShell 7 terminal; tools, exact local commands,
316 pinned input hashes, and QEMU/ADK details are in [BUILD.md](docs/BUILD.md).

```powershell
git clone https://github.com/reindertpelsma/win-gpu-image.git
cd win-gpu-image
# Follow docs/BUILD.md, including VM RAM requirements and measured results.
```

## Details and status

- [Image contents, component sizes, and probes](docs/IMAGE.md)
- [Verification, attestation, consumer hash pins, and the private store](docs/TRUST.md)
- [Workflow security and owner environment setup](SECURITY.md)
- [Measured results and run URLs](notes/measurements.md)

**Build resumed:** keep the complete signed display package; [sizes and RAM requirements](docs/IMAGE.md).
Do not open pull requests that add binaries.
