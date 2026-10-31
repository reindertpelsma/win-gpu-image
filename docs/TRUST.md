# Verification and trust

The PE2 QEMU TCG smoke boots the freshly built ISO without a GPU or network.
It requires the exact guest-written `WGI_PE_BOOT_OK_V1` result-disk marker, the
offline DISM NVIDIA display-driver check, actual COM1 boot/contract markers and
the terminal GPU FAIL result expected without a GPU, within bounded time. COM1
is required; a result-disk marker alone no longer passes. Historical run10 used
the older result-disk-only contract. See [PE2_PLAN.md](PE2_PLAN.md). This proves PE reached its startup code and the package was serviced
offline. It does **not** prove NVIDIA binding, CUDA, D3D, performance, or hardware support.
Those need a separate real GPU run. Hypotheses and falsifiers appear in test headers.

Microsoft Validation OS (a test-oriented Windows image) is a possible alternative base to evaluate; WinPE is used here only because it is small.

The final job computes the image SHA-256, verifies release-part reconstruction,
creates GitHub build provenance for the ISO digest, prints the digest and input
hashes in the job summary, then publishes only to the private store. The attestation
is the sole public record. Public provenance contains the digest and build identity, no image
content. Releases use `img-<first 16 hex of SHA-256>` and parts below 1.9 GiB,
plus `manifest.json` and `SHA256SUMS`. Those files in the store are convenience
copies: a writer could replace both a blob and its checksums. Obtain the expected
SHA-256 from the consumer's **reviewed lock file**, never from the store. Reassemble
parts in manifest order, verify transport consistency, check that independent pin,
and verify public provenance:

```bash
./scripts/verify-release.sh /path/to/downloaded-release
echo '<expected SHA-256 from consumer lock file>  /path/to/downloaded-release/image.iso' | sha256sum -c -
gh attestation verify /path/to/downloaded-release/image.iso --repo reindertpelsma/win-gpu-image
```

Input pins detect byte changes after first use. Attestation links a digest to the
workflow/repo identity; it does not establish vendor authenticity, safety, licensing,
bit-for-bit reproducibility, or GPU correctness. WIM metadata and build timestamps
can change output hashes between builds. The recipe is reproducible as a procedure;
bit-for-bit reproducibility is not established. Trust includes Microsoft, NVIDIA,
QEMU's Windows distributor, hosted runner/toolchain, GitHub, action commits, and
reviewed recipe changes. See [SECURITY.md](../SECURITY.md) for owner configuration.

An attestation can exist even if the later store-publish step fails (as in run 9).
It records the build digest/context, not the overall job's final conclusion or
successful availability in the store. Review the linked workflow run as well.

## The private store

A standalone recipe for a Windows PE ISO with an offline-serviced NVIDIA display
driver, GPU probes, and a result-disk boot test. A minimal Windows disk image is future
work. The public repository contains source and recipes only. The private release
store holds a convenience copy so **rented Vast.ai test boxes** can download a
prebuilt image for GPU tests and sweeps, whether CI-triggered or manual. Licence
review of that copy is open. Workflow steps use files on the same runner; the
store is not used to transfer data between CI jobs.

Do not open pull requests that add binaries. We do not redistribute Microsoft or
NVIDIA files through this repository, public releases, or workflow artifacts.
Obtain vendor inputs directly under their respective terms. No licence grant for
vendor files is implied by this recipe. See [SECURITY.md](../SECURITY.md).

## Workflow rules and owner environment setup

The credentialed image-build workflow runs only on pushes to `main` and manual dispatch on `main`. It never
runs on pull requests because image publishing uses a credential for a private
store. Unreviewed contributions must not enter that execution path. Fork users
can follow the local Windows build instructions without a store credential.

The single Windows job has only `contents: read`, `id-token: write`, and
`attestations: write`; workflow-level permissions are empty. Checkout does not
persist credentials. Actions are pinned to full commit SHAs. The private-store PAT
is referenced only in the final publish step's environment after boot, driver,
hash consistency and attestation checks succeed. The job uses environment
`push_to_image_store`; the environment does not expose its secret to prior steps.
Review all code that runs before publishing: code in the same job can affect
later execution. The step boundary is not a sandbox against a malicious recipe.

Owner configuration (not changed by this recipe):

- Restrict environment deployments to `main` using its deployment branch rule.
- Consider required reviewers and prevent self-review for environment deployment.
- Protect `main` and require review for recipe/workflow changes after bootstrap.
- Keep the store private and scope its PAT to that repository, contents write only.
- Keep public GitHub-hosted runners available; do not route this job to paid runners.
- Review vendor licences, including private convenience copies, before adopting images.

No image, vendor download, WIM, extracted DLL, or intermediate is uploaded as a
public artifact or committed to Git. Logs contain public hashes/URLs, metadata,
and test output. The workflow does not read or print token values. Store release
notes contain only hashes and links. Failed runs publish nothing.

Build provenance proves that GitHub signed a statement linking an image digest to
the recorded workflow context. This is the sole public record; image and input hashes
also appear as text in the job summary. The private store serves rented Vast.ai
test boxes for CI-triggered or manual GPU tests and sweeps. Its checksum and
manifest copies are conveniences, not a trust anchor: expected SHA-256 comes from
the consumer's reviewed lock file. Provenance does not prove the inputs are legitimate,
licences permit use, a runner was uncompromised, the build is deterministic, or an
NVIDIA GPU binds and works. Hash verification and real hardware tests serve different
purposes. Report recipe vulnerabilities privately to the repository owner.

## Credential-free source checks

A separate source workflow runs on pushes to main and codex branches, or manual
dispatch. It has only contents-read permission, no publishing environment or
secrets, and pinned checkout with credentials disabled. Linux runs CPU mock/recipe
tests; Windows compiles original helper/probe sources and tests recipe functions.
It does not fetch vendor files, build/boot an image, attest, publish, upload an
artifact or run GPU work. No pull-request event reaches either workflow.
A green source workflow permits review of compilation/CPU behavior only.
