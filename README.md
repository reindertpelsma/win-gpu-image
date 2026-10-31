# win-gpu-image

A standalone recipe for a throwaway Windows PE ISO.

**Status: PE GPU execution is parked.** The existing recipe and technical work
are retained. Future Windows GPU checks use a full Windows 11 Enterprise
Evaluation installation; this repository does not implement that replacement.
The images contain Microsoft and NVIDIA binaries that copyright does not
allow us to publish, so the **build is public** but the **output stays in a private
store** (a convenience copy for rented Vast.ai test boxes); anyone else rebuilds
from the recipe.

## Purpose and intended use

The retained recipe contains small compute and graphics probes and a serial
reporting contract. Their presence and any historical test result do not establish
permission to use PE for GPU testing. Licence review remains open; this repository
makes no claim that the old use complies with Microsoft or NVIDIA terms.

No new PE GPU execution or replacement PE image is planned. Existing merged
sources and immutable historical artifacts are preserved. Do not treat a build,
a serial marker or a private image copy as a deployment recommendation.

Non-goals: running real workloads, hosting services, daily use, redistributing the
image or any vendor file. Building from a public recipe does not grant rights to
its third-party inputs or output.

## Historical PE design

PE starts each test clean from RAM without activation/OOBE/accounts.
The base is small; the complete signed driver adds about 2.6 GB.
PE lacks DWM, a shell and a desktop; NVIDIA-in-PE remains unproven.
[Earlier comparison and alternative-image proposal](docs/WHY-PE.md) is historical;
an alternative minimal Windows base is not the current execution plan.

## Build it yourself

Use an elevated Windows PowerShell 7 terminal; tools, exact local commands,
316 pinned input hashes, and QEMU/ADK details are in [BUILD.md](docs/BUILD.md).

```powershell
git clone https://github.com/reindertpelsma/win-gpu-image.git
cd win-gpu-image
# Follow docs/BUILD.md, including prerequisites and licence review.
```

## Details and status

- [Image contents, component sizes, and probes](docs/IMAGE.md)
- [Windows 11 test-image and R2 design (not implemented)](docs/WINDOWS11.md)
- [Verification, attestation, consumer hash pins, and the private store](docs/TRUST.md)
- [Workflow security and owner environment setup](SECURITY.md)
- [Historical records and run URLs](notes/measurements.md)
- [Retained PE2 serial/GSP/compute design](docs/PE2_PLAN.md)

**Historical image:** [run 10](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39848309657), 8192 MiB tested (not a minimum); [sizes and limits](docs/IMAGE.md).
The retained PE2 design requires real serial markers, both GSP registry readbacks
and a compute-kernel/readback probe. Its no-GPU smoke tests observation only;
NVIDIA-in-PE operation remains unverified. The PE2 execution lane is parked.
Do not open pull requests that add binaries.
