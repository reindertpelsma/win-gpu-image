# Security model

The workflow runs only on pushes to `main` and manual dispatch on `main`. It never
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
the recorded workflow context. It does not prove the inputs are legitimate,
licences permit use, a runner was uncompromised, the build is deterministic, or an
NVIDIA GPU binds and works. Hash validation and real hardware tests serve different
purposes. Report recipe vulnerabilities privately to the repository owner.
