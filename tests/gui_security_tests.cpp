#include <QCoreApplication>
#include <QLocalSocket>
#include <QTemporaryDir>
#include <QThread>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJSEngine>
#include <QQmlEngine>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/un.h>
#include <unistd.h>

#include <cstring>
#include <functional>
#include <iostream>
#include <stdexcept>

#include "qt/ipc.h"
#include "libwalletqt/PassphraseHelper.h"
#include "libwalletqt/YieldInfo.h"
#include "p2pool/InstallFile.h"

namespace
{
void require(bool condition, const char* message)
{
    if (!condition) throw std::runtime_error(message);
}

struct YieldSnapshot final : Monero::YieldInfo
{
    explicit YieldSnapshot(bool& destroyed) : destroyed(destroyed) {}
    ~YieldSnapshot() override { destroyed = true; }
    bool& destroyed;
    bool failed = false;
    int status() const override { return failed ? Status_Error : Status_Ok; }
    std::string errorString() const override { return failed ? "Unavailable" : ""; }
    bool update() override { failed = true; return false; }
    uint64_t burnt() const override { return 9007199254740993ULL; }
    uint64_t locked() const override { return 2; }
    uint64_t supply() const override { return 3; }
    uint64_t ybi_data_size() const override { return 21; }
    uint64_t yield() const override { return 4; }
    uint64_t yield_per_stake() const override { return 5; }
    uint64_t total_accrued_from_past_completions() const override { return 7; }
    uint64_t currently_staked() const override { return 500; }
    uint64_t accrued_from_current_stake() const override { return 24; }
    uint64_t blockchain_height() const override { return 120; }
    uint64_t stake_lock_period() const override { return 20; }
    std::string period() const override { return "00:00:40:00"; }
    std::vector<std::tuple<size_t, std::string, std::string, uint64_t, uint64_t>> payouts() const override
    {
        return {{100, "transaction", "SAL1", 9007199254740993ULL, 11},
                {110, "quoted\"hash", "test\\asset", 300, 13}};
    }
};

void yieldBindings()
{
    bool destroyed = false;
    {
        YieldInfo info(new YieldSnapshot(destroyed));
        require(info.property("total_accrued_from_past_completions").toULongLong() == 7, "completed yield property missing");
        require(info.property("currently_staked").toULongLong() == 500, "current stake property missing");
        require(info.property("accrued_from_current_stake").toULongLong() == 24, "current yield property missing");
        require(info.property("blockchain_height").toULongLong() == 120, "snapshot height missing");
        require(info.property("stake_lock_period").toULongLong() == 20, "network stake period missing");
        const QJsonDocument doc = QJsonDocument::fromJson(info.payouts().toUtf8());
        require(doc.isArray() && doc.array().size() == 2, "invalid payout JSON");
        const auto first = doc.array()[0].toObject();
        require(first["asset_type"].toString() == "SAL1", "payout asset missing");
        require(first["burnt"].toString() == "9007199254740993", "atomic stake amount lost precision");
        require(first["burntFormatted"].toString() == "90071992.54740993", "displayed stake amount lost precision");
        QJSEngine engine;
        QQmlEngine::setObjectOwnership(&info, QQmlEngine::CppOwnership);
        engine.globalObject().setProperty("yieldInfo", engine.newQObject(&info));
        require(engine.evaluate("yieldInfo.burntFormatted").toString() == "90071992.54740993",
            "yield total lost precision crossing into JavaScript");
        require(doc.array()[1].toObject()["hash"].toString() == "quoted\"hash", "payout text not escaped");
        require(!info.update() && info.status() == YieldInfo::Status_Error && info.errorString() == "Unavailable",
            "yield refresh failure not forwarded");
    }
    require(destroyed, "core yield snapshot leaked");
}

void p2poolInstallation()
{
    QTemporaryDir directory;
    require(directory.isValid(), "temporary install directory failed");
    const QString source = directory.filePath("new-version");
    const QString target = directory.filePath("installed-version");
    const auto write = [](const QString& path, const QByteArray& data) {
        QFile file(path);
        require(file.open(QIODevice::WriteOnly) && file.write(data) == data.size(), "install fixture write failed");
    };
    const auto read = [](const QString& path) {
        QFile file(path);
        require(file.open(QIODevice::ReadOnly), "install fixture read failed");
        return file.readAll();
    };
    write(source, "new executable");
    write(target, "old executable");
    require(p2pool::installExecutable(source, target), "existing executable was not replaced");
    require(read(target) == "new executable", "old executable remained after update");
    require(QFileInfo(target).isExecutable(), "installed executable lost permissions");
    require(!p2pool::installExecutable(directory.filePath("missing"), target), "missing executable accepted");
    write(source, "");
    require(!p2pool::installExecutable(source, target), "empty executable accepted");
    require(read(target) == "new executable", "failed update damaged installed executable");
    const QString link = directory.filePath("symlink");
    require(QFile::link(target, link), "install symlink fixture failed");
    write(source, "replacement");
    require(!p2pool::installExecutable(source, link), "symlink target accepted");
    require(read(target) == "new executable", "symlink target overwritten");
    require(!p2pool::installExecutable(source, directory.path()), "directory target accepted");
}

struct Runtime
{
    QTemporaryDir directory;
    Runtime()
    {
        require(directory.isValid(), "temporary directory creation failed");
        require(::chmod(QFile::encodeName(directory.path()).constData(), 0700) == 0, "chmod failed");
        qputenv("XDG_RUNTIME_DIR", QFile::encodeName(directory.path()));
    }
};

struct BoundSocket
{
    int fd = -1;
    explicit BoundSocket(const QString& path)
    {
        const QByteArray name = QFile::encodeName(path);
        sockaddr_un address{};
        require(name.size() < static_cast<int>(sizeof(address.sun_path)), "socket path too long");
        address.sun_family = AF_UNIX;
        std::memcpy(address.sun_path, name.constData(), name.size() + 1);
        fd = ::socket(AF_UNIX, SOCK_STREAM, 0);
        require(fd >= 0 && ::bind(fd, reinterpret_cast<sockaddr*>(&address), sizeof(address)) == 0,
            "fixture socket creation failed");
    }
    ~BoundSocket() { if (fd >= 0) ::close(fd); }
};

void liveIpc()
{
    Runtime runtime;
    IPC first;
    const QString path = first.socketFile().absoluteFilePath();
    require(path.startsWith(runtime.directory.path() + '/'), "socket outside runtime directory");
    require(first.socketFile().fileName().contains(QString::number(::geteuid())), "socket name missing UID");
    first.bind();
    struct stat before{}, after{};
    require(::lstat(QFile::encodeName(path).constData(), &before) == 0 && S_ISSOCK(before.st_mode), "IPC did not bind");
    {
        IPC second;
        second.bind();
    }
    require(::lstat(QFile::encodeName(path).constData(), &after) == 0 && before.st_ino == after.st_ino,
        "second bind removed a live socket");
    QString received;
    QObject::connect(&first, &IPC::uriHandler, [&](QString uri) { received = uri; });
    IPC sender;
    const QString command = QStringLiteral("salvium://test-address?tx_amount=1");
    require(!sender.saveCommand(command), "command not forwarded to live instance");
    for (int i = 0; i < 50 && received.isEmpty(); ++i)
    {
        QCoreApplication::processEvents();
        QThread::msleep(2);
    }
    require(received == command, "forwarded command changed or disappeared");
    require(sender.queuedCmd().isEmpty(), "forwarded command retained in sender queue");
}

void staleIpc()
{
    Runtime runtime;
    IPC ipc;
    const QString path = ipc.socketFile().absoluteFilePath();
    { BoundSocket stale(path); }
    ipc.bind();
    QLocalSocket client;
    client.connectToServer(path);
    require(client.waitForConnected(1000), "owned stale socket was not recovered");
}

void unsafePaths()
{
    for (const bool symlink : {false, true})
    {
        Runtime runtime;
        IPC ipc;
        const QString path = ipc.socketFile().absoluteFilePath();
        const QString target = symlink ? runtime.directory.filePath("target") : path;
        QFile file(target);
        require(file.open(QIODevice::WriteOnly) && file.write("preserve") == 8, "file fixture failed");
        file.close();
        if (symlink)
            require(::symlink(QFile::encodeName(target).constData(), QFile::encodeName(path).constData()) == 0, "symlink fixture failed");
        require(ipc.saveCommand("salvium://test-address"), "unsafe path accepted for forwarding");
        ipc.bind();
        require(file.open(QIODevice::ReadOnly) && file.readAll() == "preserve", "non-socket path was modified");
        if (symlink) require(QFileInfo(path).isSymLink(), "symlink removed");
    }
    Runtime runtime;
    const QString real = runtime.directory.filePath("real");
    const QString link = runtime.directory.filePath("link");
    require(QDir().mkdir(real), "runtime fixture failed");
    require(::chmod(QFile::encodeName(real).constData(), 0700) == 0, "chmod failed");
    require(::symlink(QFile::encodeName(real).constData(), QFile::encodeName(link).constData()) == 0, "symlink failed");
    qputenv("XDG_RUNTIME_DIR", QFile::encodeName(link));
    IPC symlinkedRuntime;
    require(symlinkedRuntime.socketFile().filePath().isEmpty(), "symlinked runtime accepted");
    qputenv("XDG_RUNTIME_DIR", QFile::encodeName(real));
    require(::chmod(QFile::encodeName(real).constData(), 0777) == 0, "chmod failed");
    IPC sharedRuntime;
    require(sharedRuntime.socketFile().filePath().isEmpty(), "shared runtime accepted");
    require(sharedRuntime.saveCommand("salvium://test-address"), "URI not queued when IPC disabled");
    require(!sharedRuntime.queuedCmd().isEmpty(), "queued URI lost");
}

bool foreignOwner()
{
    // Run in an isolated container as root to exercise actual foreign ownership.
    if (::geteuid() != 0) return false;
    Runtime runtime;
    IPC ipc;
    const QString path = ipc.socketFile().absoluteFilePath();
    BoundSocket foreign(path);
    require(::listen(foreign.fd, 4) == 0, "listen failed");
    require(::chown(QFile::encodeName(path).constData(), 10001, 10001) == 0, "chown failed");
    require(ipc.saveCommand("salvium://test-address?recipient_name=private"), "foreign socket accepted");
    ipc.bind();
    struct stat info{};
    require(::lstat(QFile::encodeName(path).constData(), &info) == 0 && info.st_uid == 10001,
        "foreign socket removed or replaced");
    require(::chown(QFile::encodeName(runtime.directory.path()).constData(), 10001, 10001) == 0, "chown failed");
    IPC foreignRuntime;
    require(foreignRuntime.socketFile().filePath().isEmpty(), "foreign runtime accepted");
    return true;
}

struct Prompter : PassprasePrompter
{
    std::function<void()> prompt;
    void onWalletPassphraseNeeded(bool) override { prompt(); }
};

void passphrases()
{
    Prompter prompter;
    PassphraseHelper helper(&prompter);
    const QString samples[] = {QString{}, QStringLiteral("short"), QString(200, QChar('x')),
        QString::fromUtf8("Grüße\xf0\x9f\x94\x90"), QString(QChar(0xd800)), QString(QChar(0xdc00))};
    for (const auto& sample : samples)
    {
        prompter.prompt = [&] { helper.onPassphraseEntered(sample, false, false); };
        bool onDevice = true;
        bool received = false;
        helper.onDevicePassphraseRequest(onDevice, [&](const char* bytes, std::size_t size) {
            received = true;
            require(QByteArray(bytes, static_cast<int>(size)) == sample.toUtf8(), "UTF-8 passphrase changed");
        });
        require(received && !onDevice, "host passphrase missing");
    }
    prompter.prompt = [&] { helper.onPassphraseEntered("do not retain", false, true); };
    bool onDevice = false;
    bool aborted = false;
    try { helper.onDevicePassphraseRequest(onDevice, [](const char*, std::size_t) { throw std::logic_error("aborted passphrase delivered"); }); }
    catch (const std::runtime_error&) { aborted = true; }
    require(aborted, "abort not propagated");
    prompter.prompt = [&] { helper.onPassphraseEntered("do not retain", true, false); };
    helper.onDevicePassphraseRequest(onDevice, [](const char*, std::size_t) { throw std::runtime_error("device-only passphrase delivered"); });
    require(onDevice, "device entry flag missing");
    prompter.prompt = [&] { helper.onPassphraseEntered("temporary", false, false); };
    bool threw = false;
    try { helper.onDevicePassphraseRequest(onDevice, [](const char*, std::size_t) { throw std::runtime_error("receiver failed"); }); }
    catch (const std::runtime_error&) { threw = true; }
    require(threw, "receiver exception lost");
    prompter.prompt = [&] { helper.onPassphraseEntered("", false, false); };
    helper.onDevicePassphraseRequest(onDevice, [](const char*, std::size_t size) { require(size == 0, "previous passphrase reused"); });
}
}

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    try
    {
        liveIpc(); std::cout << "PASS: live IPC and URI forwarding\n";
        staleIpc(); std::cout << "PASS: owned stale IPC recovery\n";
        unsafePaths(); std::cout << "PASS: unsafe paths preserved, commands queued\n";
        std::cout << (foreignOwner() ? "PASS: foreign ownership checks\n" : "SKIP: foreign ownership checks require isolated root\n");
        passphrases(); std::cout << "PASS: passphrase handoff, UTF-8, cancellation and exceptions\n";
        yieldBindings(); std::cout << "PASS: yield properties, payout assets and precision, refresh errors and ownership\n";
        p2poolInstallation(); std::cout << "PASS: P2Pool executable replacement and failed-install preservation\n";
        return 0;
    }
    catch (const std::exception& error)
    {
        std::cerr << "FAIL: " << error.what() << '\n';
        return 1;
    }
}
