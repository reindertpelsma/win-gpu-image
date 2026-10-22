# Why a PE instead of a full Windows install

A fresh full Windows installation and setup can take on the order of an hour per
machine (**estimate**, not measured here). PE can boot in about a minute:
**MEASURED in downstream testing**, roughly 50–70 seconds in a VM on a different
host; those measurements do not predict this CI runner or rented-machine timings.
The base PE is a few hundred MB plus the driver, compared with a multi-GB Windows
installation; the driver itself is substantial (see the [size table](IMAGE.md)). PE runs
from RAM so each boot starts clean, with no activation, OOBE, or account setup
steps, and can be rebuilt from a pinned recipe.

PE has no DWM, shell, or desktop, so it does not cover display-compositor paths,
real applications, or services. **It is not yet shown that the NVIDIA driver works
in PE at all.** This environment complements a full Windows installation for
desktop and application testing. A minimal-Windows disk image is the planned
middle option, with the same testing-only purpose.
