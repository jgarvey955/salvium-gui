# Private wallet audit

This repository's `audit` branch starts from `master`. Daemon, CLI and shared
wallet verification belong to the core repository's separate `audit` branch.
The `salvium` submodule pins the audit wallet API used by this GUI.

Open **Audit**, check the wallet, and export private evidence at the snapshot
height supplied by the coordinator. Share the file privately with the reviewer.
Import the signed recipient result using the developer public key obtained
independently. The wallet verifies the signature, exported proofs, snapshot and
revision before applying a clearance or developer decision.

The page covers every account and subaddress. Evidence can disclose amounts and
spend links. It never contains seeds or spend secrets. Audit verdicts preserve
ordinary transaction validation and stake locks. Unresolved evidence remains
under review with an explicit reason; the developer can sign CLEAR or DENY.

Build with `make release-static` in the normal build directory. For development,
the CMake cache may set `SALVIUM_CORE_SOURCE_DIR` to the sibling core audit source
and `MANUAL_SUBMODULES=ON`. Release packaging must use the reviewed audit core
pin. The core's `docs/AUDIT_PRIVATE_REVIEW.md` defines reviewer commands,
NonKYC reconciliation and the version 3 transport for third-party wallets.
