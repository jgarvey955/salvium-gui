Build with `make release-static BUILD_GUI_TESTS=ON`, then run
`build/Linux/release/gui_security_tests` (or the corresponding native build path).
The tests use temporary runtime directories and never launch a daemon or access
a hardware wallet. Run in an isolated container as root to include foreign-UID
ownership checks; normal user runs skip those checks.

The yield checks cover the QML-facing totals, network maturity settings, payout
asset labels and amount precision, refresh errors, and snapshot ownership. The
CLI unit suite separately checks stake maturity boundaries, total overflow and
stakes stored at the first wallet transfer index.

P2Pool installation checks cover replacing an existing executable, failed-update
preservation and symlink rejection. Yield checks also pass formatted amounts
through the JavaScript engine to catch rounding at the QML boundary.

The staking page regressions run against Qt 5's dynamic QML test runner (install
`qml-module-qttest` alongside the GUI's QML dependencies):

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software qmltestrunner \
    -input tests/qml -import tests/qml/mocks -import fonts
```

These load the real staking page with a mock wallet whose balance changes only
emit wallet events. They check balance refresh, amount validation, page reentry,
wallet reopening, and SAL1 selection independently of the sidebar's asset.
They never connect to a daemon or open a wallet file. Run in a network namespace
or a container with networking disabled for isolation. Resource-image warnings
are expected because the standalone runner does not load the GUI's resource bundle.
