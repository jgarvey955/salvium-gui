# Stake display review corrections

The yield page accepts a native snapshot containing each payout's asset label,
exact atomic amount strings, formatted display strings, maturity block and the
shared core completion result. It uses the current payout row even when there
is only one payout. Summary amounts are also formatted in C++ before crossing
the JavaScript boundary. Completion uses the wallet-scanned height; the daemon's
height does not make an unscanned maturity block complete.

Snapshot ownership is explicit and the page disposes its snapshot after loading.
Errors clear old results. Refresh notifications schedule a later reload to avoid
calling the yield API while the refresh callback holds the wallet lock. The
staking page refreshes its displayed balance on wallet events and uses native
integer parsing and balance comparison for the entered amount.

The submodule is pinned to the matching core `audit` correction, including its
import-completion and shared stake-maturity changes.

Validation on 2026-11-22:

- The real yield and staking pages passed 14 QML checks on each branch using an
  injected wallet. Cases include exactly one payout, asset labels, an atomic
  amount above 2^53, snapshot errors/disposal, refresh events, balance changes,
  page reentry and wallet reopening.
- The instrumented `gui_security_tests` built from the identical native yield
  binding sources and identical core YieldInfo interface passed ASan/UBSan. It
  checks amount strings through the JavaScript engine, native maturity metadata,
  error forwarding and snapshot destruction. The core's four boundary/overflow
  yield tests passed independently on both branches.
- These focused checks do not constitute a complete GUI platform release build.
  Full GUI and dependency qualification remain part of the wider audit2 review.

No test opens a production wallet or starts a daemon. Historical classifications,
production clearance approval and the final financial report are separate from
this display correction.
