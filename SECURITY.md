# Security model

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
