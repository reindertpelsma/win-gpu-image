# PE2: serial, GSP and compute verification

STATUS: PREPARATION, 2026-10-31. Real NVIDIA-in-PE operation is unverified.
CPU tests and a no-GPU boot smoke cannot establish GPU correctness. The old
image `img-a650252c82f1c09a` remains immutable; consumers keep its existing pin
until they independently approve a verified replacement. No vendor binary is
committed, patched or published publicly.

## Image contract

The new recipe must produce actual COM1 bytes from the guest. Readiness is
distinct from a GPU verdict: a boot/begin marker alone never passes GPU testing.
The launcher retains the complete serial transcript and observes a bounded
startup and terminal verdict. Missing COM1, short/failed writes, missing verdict,
timeout, nonzero child status or missing required probe result fails closed.
A result disk is supplementary evidence, not a substitute for COM1 acceptance.

Startup first uses a separate bounded diagnostic-only bootstrap: hold the
NVIDIA device stopped, run `wpeinit` so PnP can initialize the UART, and record
bootstrap completion. It emits no serial/GPU success. Only successful bootstrap
may enter the separate COM1-required GPU supervisor. Both phases use the existing
240-second owned-child bound; the host's startup, verdict and total smoke limits
remain unchanged. A bootstrap receipt never substitutes for a GPU verdict.
The child-job timers do not bound the surrounding diagnostic WinAPI calls;
the host's observation limits remain the backstop. Diagnostics tee to stdout
and a marked result disk when available, with another disk-open attempt after
PnP initialization; pre-PnP disk capture is not guaranteed. Structured UART/SCM
receipts and bounded runtime setup-log excerpts are collected separately so a
large generic log tail cannot hide earlier device evidence. Their read/write,
coverage and truncation states are diagnostic data, never GPU/serial acceptance.

The startup sequence holds the NVIDIA service/device stopped while PE initializes,
then installs the complete unmodified INF, obtains the exact display device's
driver software key, writes `EnableGpuFirmware=1` as DWORD into that class key
and `SYSTEM\CurrentControlSet\Services\nvlddmkm`, and reads both values back
before enabling the display device. The adapter index must be discovered;
`0000` is not a portable constant. The offline SYSTEM hive also records the
service value. An offline service value alone is not proof of runtime class
configuration or first-start ordering.

COM1 must contain `WGI_GSP class=1 service=1 readback_ok=1`. Any other readback,
failed install/disable/start or ambiguous adapter is failure. No DWM setting
belongs in PE because it has no desktop compositor. The helpers use documented
Windows device/registry APIs without requiring PowerShell in PE.

The CUDA probe loads the stock driver API, initializes it, creates a context,
loads an original tiny PTX kernel, launches it, synchronizes and reads back a
bounded buffer. Every expected word and each cleanup API status is checked.
Enumeration alone cannot emit a compute-success verdict. The D3D probe retains
its NVIDIA vendor assertion. Stock driver compatibility and actual execution on
every target die remain hardware questions.

The no-GPU build smoke requires guest serial boot evidence and a terminal GPU
FAIL outcome alongside the result-disk boot marker and offline driver listing.
It proves the observation path and negative result handling, not NVIDIA binding.
Actual new-image behavior and size come from the main build's recorded result,
not from the historical image's successful smoke.

The first PE2 [image build](https://github.com/reindertpelsma/win-gpu-image/actions/runs/41204715947)
passed source tests, pinned-input verification and offline image construction,
then failed the strict smoke with COM1 open error2 and no device mapping. It
published no image or attestation. The subsequent bootstrap
[image build](https://github.com/reindertpelsma/win-gpu-image/actions/runs/41207316430)
also failed with COM1 error2 and no mapping. Its last100 result-log lines omitted
the earlier UART/SCM snapshots behind an offline SetupAPI tail, so they establish
no UART binding cause. Separately bounded receipts repair that capture gap;
they do not claim to repair COM1. Another strict smoke must supply the missing
evidence; the diagnostic disk remains insufficient for acceptance.

The subsequent [capture build](https://github.com/reindertpelsma/win-gpu-image/actions/runs/41210940731)
retained complete UART/service receipts but still failed COM1. It observed a
started ACPI PNP0501 device bound to the stock serial driver, while its external
port name and COM1 mapping were missing. This supports investigating standard
Ports installation/naming; it does not prove an exclusive cause. The proposed
bounded preparation keeps the currently installed driver and uses the normal
registered installer only after exact-device and component checks. A missing
prerequisite, failed step, reboot requirement or absent final COM1 stops the
attempt. Actual serial smoke remains the acceptance test.

## Same-image native control, then virtual GPU

After separate resource approval, select one verified single-GPU VM-capable
rental with an explicit fixed maximum of three hours, a precreate watchdog,
settled host identity and a reviewed source/driver/cleanup profile. Capture and
verify all evidence before exact-ID destruction; verify API absence afterward.

First establish that the owned box can expose its actual physical NVIDIA device
to a PE control: a supported platform boot path or a separately reviewed nested
VFIO/IOMMU/reset path. KVM availability alone proves neither. A standard VGA VM,
CPU-only emulation or a native Linux CUDA result is not the native PE control.
If the control path is unavailable, stop as UNAVAILABLE without attributing a
virtualization defect. No host or other tenant's process/device may be disturbed.

Run at least two fresh native PE boots with the same image hash, driver package,
probe inputs and serial contract. Require successful GSP readbacks, CUDA init,
kernel/readback and required D3D probe. Then run at least two fresh boots with
the same image under the selected production virtual GPU. Retain failures and
all rows; no retry is silently substituted for a failed boot.

The consumer also requires objective virtual-GPU-side evidence that the GSP
firmware/RPC path was exercised, tied to the exact boot and source. Guest registry
readbacks or the absence of Code43 alone do not establish this. The consumer
must specify positive received GSP setup/phase records in its own logs without
making this standalone recipe depend on a downstream implementation.

The host-side outer boot limit is proposed at900 seconds per boot, within the
fixed lease clock; the guest helper has its separately bounded terminal-verdict
contract. Four900-second bounds alone consume one hour. Capture/teardown time
is reserved independently, and no internal wait extends a lease.

## Desktop remains a separate bar

PE has no DWM/shell. After PE controls pass, a separate full Windows11 LTSC
desktop test must install the unmodified580.88 display package with both GSP
keys, reach the desktop, pass nvidia-smi and a small D3D or compute check, and
show no TDR during a proposed900-second idle-plus-probe window. Download and
installation duration on a nested rental is unmeasured and may not fit three
hours. A cutoff retains PARTIAL/UNRUN; a duration exception requires explicit
approval. Native PE success does not imply desktop or workload stability.

## Delivery and trust

For this round, retain the existing private GitHub release store and its current
driver delivery. Builds only on reviewed `main` attest the exact image digest
and publish under a new content-derived ID. Image/vendor files are never public
workflow artifacts. A separate branch source workflow has read-only permission,
no publishing environment or credentials, and builds only original sources.

Record the successful build URL, recipe commit, image ID, exact bytes, SHA256,
attestation verification result and private-store availability in the operator
receipt. Verify the entire image stream against an independently reviewed pin
and public build attestation before booting it. Store metadata is not the trust
anchor; provenance does not prove licensing, GPU success or an uncompromised build.

R2 is proposed future transport, not an implemented publishing path here. A
reviewed uploader would stage only a private, immutable content-addressed object;
the broker would issue short-lived read links for an admitted consumer. The box
would receive no uploader credentials and would verify size/hash/provenance.
An existing private-release URL broker does not automatically support R2 or a
new PE payload. Do not reuse production broker R2/S3 credentials for preparation.
Do not change a consumer lock or broker until the new image is verified on hardware.

## Costs, promotion and open review

Historical image size is1,549,436,928 bytes; the new build may differ. At200Mbps
the ideal transfer floor is about62 seconds before protocol overhead, not a
measured ingress time. Provisioning, four boots and driver setup are provisionally
budgeted at one to two hours, with a full three-hour reservation. At an example
0.16USD/h quote, runtime is0.16–0.32USD for that estimate; the reservation must
also cover rate headroom, disk, transfer, margin, all shared liabilities and the
credit reserve. Desktop installation needs its own bounded budget and may be a
later day. Prices and feasibility must be admitted fresh; this plan grants no lease.

Promotion requires successful source/Windows compilation checks, a successful
serial smoke, a new immutable private artifact and verified attestation, then
the same-box two-native/two-virtual PE result and separate desktop result above.
Until those hardware bars pass, downstream Windows remains unverified. A later
raw-client stress arm joins the consumer's verification policy when implemented;
this recipe change does not implement or claim it. Public build success is not
permission to advance downstream master or replace its image pin.

Vendor licensing review remains open, including private convenience copies and
use on rented machines. A proposed follow-up prefers fetching the NVIDIA driver
at boot from its vendor with a reviewed SHA256 pin over retaining it in the
private image store; that delivery change is explicitly deferred in this round.
The owner must also decide whether Microsoft ValidationOS is an appropriate
licensed test base and whether its components meet the required probe contract.
No conclusion about either vendor's permissions is made by this recipe.
