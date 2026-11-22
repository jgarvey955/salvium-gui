Run the real-page regressions with Qt 5's QML test runner and its Quick Controls,
QtTest and GraphicalEffects modules installed:

```sh
QT_QPA_PLATFORM=offscreen QT_QUICK_BACKEND=software qmltestrunner \
    -input tests/qml -import tests/qml/mocks -import fonts
```

Use a temporary HOME and XDG configuration/runtime directories for isolation.
The tests inject a wallet object and do not connect to a daemon or open wallet
files. Standalone resource-image warnings are expected because this runner does
not load the application's resource bundle. See
`docs/AUDIT_IMPORT_AND_YIELD_FIXES.md` for the reviewed cases and validation scope.
