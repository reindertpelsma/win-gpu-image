# Why a PE instead of a full Windows install

PE avoids a full Windows installation/setup for each test environment. Its base
is a few hundred MB plus the driver, compared with a multi-GB Windows installation;
the complete signed driver itself adds about 2.6 GB (see the [size table](IMAGE.md)).
PE runs from RAM so each boot starts clean, with no activation, OOBE, or account
setup steps, and can be rebuilt from a pinned recipe. This is a structural
comparison, with no boot-time or performance claim.

PE has no DWM, shell, or desktop, so it does not cover display-compositor paths,
real applications, or services. **It is not yet shown that the NVIDIA driver works
in PE at all.** This environment complements a full Windows installation for
desktop and application testing. A minimal-Windows disk image is the planned
middle option, with the same testing-only purpose.
