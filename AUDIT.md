# Private wallet audit

This repository's `audit` branch starts from `master`. Daemon, CLI and shared
wallet verification belong to the core repository's separate `audit` branch.
The `salvium` submodule pins the audit wallet API used by this GUI.

The wallet owner opens **Audit**, checks the wallet, and chooses **Request audit
(export)** at the snapshot height supplied by the main developer. Share the
file privately with the main developer, who processes and signs the results.
Import the signed recipient result using the developer public key obtained
independently. The wallet verifies the signature, exported proofs, snapshot and
revision before applying a clearance or developer decision.

The main developer will supply the public authority key during private release review.
The core intentionally leaves it unset; production result acceptance stays
disabled until then. Owners cannot substitute their own signing authority.

The page covers every account and subaddress. Evidence can disclose amounts and
spend links. It never contains seeds or spend secrets. Audit verdicts preserve
ordinary transaction validation and stake locks. Unresolved evidence remains
under review with an explicit reason; the developer can sign CLEAR or DENY.

Build with `make release-static` in the normal build directory. For development,
the CMake cache may set `SALVIUM_CORE_SOURCE_DIR` to the sibling core audit source
and `MANUAL_SUBMODULES=ON`. Release packaging must use the reviewed audit core
pin. The core's `docs/AUDIT_PRIVATE_REVIEW.md` defines reviewer commands,
NonKYC reconciliation and the version 3 transport for third-party wallets.

The audit submodule URL points to the review fork so a fresh checkout can fetch
the pinned audit commit before the core PR merges. After merge, the main developer
can repin to the reviewed official core and restore the official source URL.
