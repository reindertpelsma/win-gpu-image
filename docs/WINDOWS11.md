STATUS: DESIGN + MEASUREMENT ONLY. Runner probe completed; full Windows 11 image and R2 publisher are not implemented. Licence and evaluation-clock questions remain open.

# Full Windows 11 test image and R2 storage

This is the planned full Windows companion to the existing PE. It supplies a
desktop, DWM and normal application/service paths for GPU driver tests and probes.
It has the same throwaway, testing-only purpose: no production use, real workloads,
compatibility guarantee or performance claim. The current PE recipe and its
private GitHub release remain unchanged. R2 is the planned replacement storage
for both image types; the private GitHub store becomes legacy after a verified
cutover. Its README deprecation note is deferred until that cutover.

## Contents and input pins

| Component | Planned contents and source |
| --- | --- |
| OS | The already-pinned Windows 11 Enterprise LTSC evaluation ISO, subject to edition/terms review; otherwise an owner-approved properly licensed source with a new reviewed pin. Full Windows supplies its own graphics stack. |
| NVIDIA | Unmodified, signed 580.88 `Display.Driver`, including every INF/catalog dependency; `nvidia-smi` and NVML included. Whole unrelated packages may be omitted. |
| Virtio | Pinned, signed Windows 11 x64 storage, network and serial drivers from the [virtio-win project](https://github.com/virtio-win/virtio-win-pkg-scripts). Balloon/guest-agent components are optional and recorded explicitly. No test-signing mode. |
| PowerShell/app matrix | In-box PowerShell plus a pinned PowerShell 7 package, owner-reviewed app-matrix scripts, configuration/schema and dependency lock; test logs and structured pass/fail results go to a separate result disk. Tool source commit and new package hashes are still to be selected. |
| Other probes | This repository's original D3D11 offscreen and CUDA driver-API query programs, plus OS diagnostic tools; static CRT builds as in PE. |
| Unigine Heaven | The unmodified Windows installer and required runtime installers, with reviewed version/hash and automation/use terms. Target is a ready-to-run installation after first-boot setup; prestage the installer rather than booting the master to install it. |
| SPECviewperf | **NOT inside the image**: no installer, viewset or download cache. A separate pinned object-set manifest selects vendor-approved objects on a separate data disk. |

The existing ISO and NVIDIA pins are in [BUILD.md](BUILD.md) and
[inputs.lock.json](../inputs.lock.json). New virtio, PowerShell, app-matrix,
Heaven/runtime and SPEC object pins must be reviewed before an implementation
fetches them; no placeholder hash is an accepted build pin. Record source URL,
version/commit, SHA-256, bytes, observation date and applicable licence notes.

Microsoft offers a [90-day Enterprise evaluation](https://www.microsoft.com/en-us/evalcenter/evaluate-windows-11-enterprise).
Its availability does not establish permission for this image's packaging,
cloning or rented-machine use. [Heaven's vendor page](https://benchmark.unigine.com/heaven)
distinguishes automation/commercial editions; a free download alone is not the
required rights review. Review [SPEC's chosen release and terms](https://gwpg.spec.org/)
before storing its separate object set. Licence review remains open for every
vendor component and even private convenience copies. Nothing here grants rights
or claims compliance. No vendor binary is committed or published publicly.

## Proposed Windows-runner build

The following is a design, not an implemented or tested build script. Use the
pinned Windows deployment tools and a disposable task-owned disk file, never a
physical runner disk or its boot configuration. Microsoft documents
[applying Windows to VHDX](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/deploy-windows-on-a-vhd--native-boot?view=windows-11)
and [creating boot files with BCDBoot](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/bcdboot-command-line-options-techref-di?view=windows-11).

1. Verify input pins; inspect `DISM /Get-WimInfo` and select the reviewed x64 LTSC
   evaluation index by edition metadata, not an assumed fixed index. Require a
   vendor-generalized, pre-OOBE source; a customized, already-booted source needs
   the separate Sysprep route below. Preflight actual free space on the work
   volume; fail before downloads if insufficient.
2. Create a dynamic VHDX with a provisional 96 GiB virtual capacity. Attach only
   that explicit file and create GPT partitions: 260 MiB FAT32 EFI system, 16 MiB
   MSR, then NTFS Windows. Assign temporary free letters such as `S:` and `W:`.
   The future launcher must satisfy supported Windows 11 firmware/TPM/hardware
   requirements; the PE boot result is not evidence of those requirements.
3. Apply the vendor image and add the complete display package plus the selected
   signed virtio drivers offline. Illustrative commands, after safe disk setup:

   ```powershell
   dism /Apply-Image /ImageFile:<pinned-install.wim> /Index:<reviewed-index> /ApplyDir:W:\ /CheckIntegrity /Verify
   dism /Image:W:\ /Add-Driver /Driver:<complete-Display.Driver>\nv_dispi.inf
   dism /Image:W:\ /Add-Driver /Driver:<selected-signed-virtio-directory> /Recurse
   dism /Image:W:\ /Get-Drivers /Format:List /English
   ```

4. Stage the PowerShell/app-matrix tree, original probes, pinned Heaven installer
   and its complete runtime installers. Full Windows uses its native D3D/DXGI
   stack; the PE recipe's separate evaluation-ISO DLL copying is not this design.
   Install non-portable tools on the first consumer boot, then require their
   installation and GPU probe results before a test run starts.
5. Place a schema-checked `Windows\Panther\Unattend.xml` for supported specialize
   and OOBE settings, per-instance setup and tool installation. Offline settings
   can use [DISM /Apply-Unattend](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/dism-unattended-servicing-command-line-options?view=windows-11);
   that applies `offlineServicing`, not the future specialize/OOBE passes. Do not
   bake a shared password, product key, R2 token, machine identity or activation
   state into the image. Runtime account inputs belong on a per-instance config
   disk. Required licence acceptance/account/activation steps are not assumed waived.
6. Use a compatible BCDBoot on the applied Windows tree:
   `bcdboot W:\Windows /s S: /f UEFI`. Verify the task VHDX's EFI files and BCD,
   not the host's BCD. Remove only task-owned download/build caches; preserve
   signed components, servicing, activation and normal OS security mechanisms.
7. Detach the VHDX, record its exact bytes/hash and build/input manifest, and keep
   it immutable. No first boot occurs on the builder in the preferred route.
   First consumer boot installs the staged tools and reports setup/probe status.
   A writable per-run disk or overlay keeps the verified base clean. If the
   launcher needs raw format, verify the attested VHDX first and then convert
   with a pinned converter; the resulting raw file has a different digest.

An alternative customization route that must boot a reference Windows VM would
need supported Sysprep `/generalize /oobe /shutdown` **inside that reference VM**,
then an offline capture without booting it again. This is not a command to run
against the hosted runner. It is not part of this stretch and is not evidence of
a fresh evaluation period. See [Microsoft's generalization procedure](https://learn.microsoft.com/en-us/windows-hardware/manufacture/desktop/sysprep--generalize--a-windows-installation?view=windows-11).

## Evaluation clock: understanding to verify

**Owner's understanding, to verify, not an established fact here:** an evaluation
image captured after first boot already has its 90-day timer running. Delivering
a generalized, pre-OOBE image with an unattend is intended to let the evaluation
clock start on the first boot of each fresh Vast box. The preferred construction
therefore applies the vendor WIM offline and never boots the master. Whether the
chosen LTSC evaluation actually has that per-instance behavior must be checked;
neither a Sysprep flag nor a pre-OOBE label proves it, and Sysprep is not assumed
to reset or renew an evaluation period. No timer-reset/expiry workaround is proposed.

Offline checks can establish the pinned source/edition/build, driver inventories,
answer-file structure, staged tool hashes, partitions/BCD and that this build path
never launched the guest. Setup state, Panther/Sysprep logs and registry state can
be inspected for consistency and evidence of prior customization. Those are
useful checks, not proof of remaining evaluation days, online activation, licence
entitlement or permitted cloning.

A later authorized experiment must first boot two fresh instances from the same
base, record UTC/setup/activation status and `slmgr /dlv` plus `slmgr /xpr`, and
compare the reported expiry with each first boot. Keep activation identifiers
private. Check applicable Microsoft terms as well as observable behavior before
relying on the assumption. This stretch does not boot Windows or rent a Vast box.

## Size and cleanliness budget

These are **INFERRED planning ranges**, not a measured Windows 11 build. File
inventory/capacity is separate from execution performance. The complete NVIDIA
source was measured previously at 2,692,847,387 bytes; its imported full-Windows
footprint still needs measurement. See [the PE inventory](IMAGE.md).

| Item | Provisional size/budget |
| --- | --- |
| Applied Windows plus normal servicing state | 20–35 GiB |
| NVIDIA source payload | About 2.51 GiB; complete package retained |
| Selected virtio drivers | 0.1–0.5 GiB |
| PowerShell/app matrix/probes | 0.3–1 GiB, excluding application datasets |
| Installed Heaven and required runtimes | 1–2 GiB |
| Sealed dynamic VHDX | Roughly 25–45 GiB file length; 96 GiB virtual capacity |
| Compressed transfer object | Roughly 15–30 GiB; compressor/content dependent |
| Peak local build working space | Roughly 60–100 GiB, including source WIM, expanded drivers, disk and scratch |
| SPECviewperf/object-set data | Separate; a 150 GB input scenario is not part of the OS image |

Keep test results, application datasets and credentials on separate per-run disks.
Keep only needed complete driver packages and reviewed test tools. Use supported
component cleanup only; do not strip Windows DLLs, signed INF dependencies,
licensing state or installed-software servicing caches. First-boot tool installers
can remove their own temporary staging after verified installation. Pin updates
and tools in the recipe; arbitrary online updates would change the test input.

Streaming publishing avoids a second full image/transport copy. It does **not**
make DISM a zero-local-disk builder: applying and servicing Windows needs a local,
random-access source WIM and target disk. A missing free-space budget is a real
blocker even when a 150 GB dataset can pass through a bounded network stream.

## R2 streaming and trust design

The destination is a private R2 bucket for rented Vast test machines. Owner-created
environment credentials will be supplied later; no R2 bucket, token, upload or
settings change is needed for this design/measurement. The eventual publisher
uses bucket-scoped R2 S3 credentials, exposed only to its final upload step in an
owner-protected main-only environment. Store endpoint/account/bucket separately
from credentials. An R2 API token must supply the S3 Access Key ID and Secret
Access Key expected by the chosen SDK; see [R2 authentication](https://developers.cloudflare.com/r2/api/tokens/).

1. Seal and hash the existing local build disk first, without copying it. Define
   `image_sha256` as SHA-256 of the **exact uncompressed VHDX file**. That known
   full 64-hex digest determines the R2 object key `img-<sha256>`. The transport
   encoding is recorded separately, not confused with the image identity.
2. Run the same pinned provenance action as PE, with the local image as subject.
   Its [pinned action definition](https://github.com/actions/attest-build-provenance/blob/977bb373ede98d70efdf65b84cb5f73e068dcc2a/action.yml)
   also supports `subject-name` plus `subject-digest: sha256:<hex>` for a future
   verified streaming producer. Attestation publishes a digest/context, no blob.
   Keep empty workflow permissions and only contents-read/OIDC/attestation writes
   on the future image job, as for PE. No public hash release or image artifact.
3. In the final credentialed step, read the sealed disk once through a pinned
   compressor into multipart upload. Recompute the image digest before compression
   and compute a separate SHA-256 for the transmitted compressed byte stream.
   Use binary I/O rather than PowerShell text pipelines for payload bytes.
   No full compressed file or second disk image is staged. Start with two bounded
   64 MiB part buffers; retain each until its upload is acknowledged for retries.
4. R2 [multipart rules](https://developers.cloudflare.com/r2/objects/upload-objects/)
   require uniform non-final parts, 5 MiB–5 GiB per part, with at most 10,000 parts.
   Record upload ID, ordered part numbers/ETags and bytes. Retry bounded parts,
   abort failures and configure incomplete-upload expiry later. An ETag is not
   the image SHA-256. Replaying a failed compressed stream may require reading
   and encoding the sealed disk again; arbitrary seek/resume is not assumed.
5. Complete only if the recomputed source digest matches the sealed image and all
   parts are acknowledged. HEAD-check object size, then write
   `img-<sha256>.manifest.json` last as the readiness marker. Include image/transport
   hashes and sizes, format/encoding, input pins, tool set, repo commit, run URL,
   build date and public attestation URL. Readers ignore objects without a complete
   reviewed manifest. Existing-key collisions or incomplete metadata fail closed;
   never overwrite an image referenced by a consumer lock file.
6. The consumer lock pins the reviewed **image SHA-256**, repo/build identity and
   separate object-set manifest digest. Stream download/decompression, hash the
   reconstructed VHDX, compare the lock pin, then run
   `gh attestation verify <image.vhdx> --repo reindertpelsma/win-gpu-image` before
   conversion or first boot. R2 manifests/checksums are convenience metadata,
   not the trust anchor. Test boxes use short-lived read links, no uploader key.

Large external inputs use the same bounded multipart path directly from their
reviewed sources to R2; no full 150 GB local copy. Their expected hashes/key names
must already be pinned. Check the complete stream hash before publishing the
object-set readiness manifest; include per-object bytes/hashes/version/licence
notes. Range resume needs a stable, verified source/version; otherwise restart
or replay. SPECviewperf's manifest and objects remain separate from the disk and
from its image attestation. A copied input is not falsely described as software
built by this repository. Consumer locks pin the separate object-set digest too.

For a producer whose final digest is unknown until streaming ends, content-addressed
naming requires a prior digest pass over an existing local image or a private
temporary object and a later server-side promotion. This design chooses the prior
hash pass; it does not pretend an unknown digest can name a one-pass upload.
Actual SDK retry/checksum behavior and complete readback still need implementation
and tests when the owner supplies R2 access.

## Hosted-runner measurement and the 150 GB question

The dispatch-only [measurement workflow](../.github/workflows/measure-runner.yml)
has empty permissions, no checkout/actions, no environment/secrets, and a bounded
20 GiB public ingress sample. It records CPU count, RAM, per-drive free bytes and
network bytes/time/rate; the file payload is discarded through a 4 MiB buffer.
This is hosted-runner networking/resource evidence; no PE guest or GPU is timed.

MEASURED on 2026-10-23 in the single authorized
[run 39876173005](https://github.com/reindertpelsma/win-gpu-image/actions/runs/39876173005),
commit `d686ba053ddfe86db096887f54f932364e5f9655`, Windows Server 2022
`10.0.20348`, runner image `win22` / `20261012.333.1`. The job passed; no image
build, guest boot, R2 access, secret use or payload artifact occurred.

| Runner/resource observation | MEASURED value |
| --- | --- |
| Logical CPUs / reported cores | 4 / 2; AMD EPYC 7763 model string |
| Physical/visible RAM | 17,174,360,064 bytes (about 16 GiB) |
| Free RAM before download | 14,618,484,736 bytes |
| C: volume / free before / free after | 274,284,392,448 / 88,565,571,584 / 88,518,975,488 bytes |
| D: volume / free before / free after | 161,059,172,352 / 157,867,724,800 / 157,867,700,224 bytes |
| Public input | Two sequential reads of [OVH's 10 GiB test file](https://proof.ovh.net/files/10Gb.dat), both HTTP 200 |
| Received bytes | 21,474,836,480 (20 GiB; 21.47 decimal GB) |
| Transfer elapsed time | 196.422 seconds for the hosted-runner download only |
| Aggregate rate | 109.330 decimal MB/s; 104.265 MiB/s |
| First / second request | 97.719 / 98.703 seconds; 109.881 / 108.785 MB/s |
| Buffer / payload disk writes | 4 MiB read buffer; no payload file |

MEASURED: D: free space decreased by 24,576 bytes; C: by 46,596,096 bytes across
the job. These are volume snapshots, not an attribution of every background write.
The workflow discards the payload and retains only small code/metadata/log files.
The test reads the same public object twice from one runner and origin; it does
not exercise 150 GB of unique objects, another vendor's rate limit or R2 egress/ingress.

INFERRED: at this sample's aggregate rate, downloading 150 decimal GB alone would
take about 1,372 seconds (22.9 minutes). That is a rate projection, not a measured
150 GB transfer. D: had only about 7.87 decimal GB beyond a 150 GB payload before
any image/source/scratch budget; avoid staging that dataset alongside the disk
on the same volume. Retain bounded streaming and explicit per-volume preflights.

GitHub documents a [six-hour hosted-job limit](https://docs.github.com/en/actions/reference/limits)
and [standard runner resources](https://docs.github.com/en/actions/reference/runners/github-hosted-runners).
The advertised Windows public-runner storage is 14 GB; an observed larger work
volume is evidence for that VM, not a reservation or future guarantee. Do not
assume a 150 GB allowance or an undocumented network quota from a 20 GiB sample.
150 decimal GB in six hours needs about 6.94 MB/s for download alone; building,
compression, upload, retries and startup also consume that budget. A rate-based
projection is INFERRED, not a measured 150 GB transfer or an R2 throughput claim.

Follow-up work is conditional on the observed disk budget, source-specific rates,
reviewed tool pins/terms, first-boot clock checks and a working R2 readback test.
The next step is implementation planning with those findings, not a claim that a
Windows 11 image has already been built or that 150 GB fits one hosted job.
