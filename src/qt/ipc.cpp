// Copyright (c) 2014-2024, The Monero Project
//
// All rights reserved.
//
// Redistribution and use in source and binary forms, with or without modification, are
// permitted provided that the following conditions are met:
//
// 1. Redistributions of source code must retain the above copyright notice, this list of
//    conditions and the following disclaimer.
//
// 2. Redistributions in binary form must reproduce the above copyright notice, this list
//    of conditions and the following disclaimer in the documentation and/or other
//    materials provided with the distribution.
//
// 3. Neither the name of the copyright holder nor the names of its contributors may be
//    used to endorse or promote products derived from this software without specific
//    prior written permission.
//
// THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY
// EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
// MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL
// THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
// SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
// PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
// INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
// STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF
// THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

#include <QCoreApplication>
#include <QLocalSocket>
#include <QLocalServer>
#include <QtNetwork>
#include <QDebug>
#include <QLockFile>
#ifdef Q_OS_UNIX
#include <fcntl.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>
#endif

#include "ipc.h"
#include "utils.h"

namespace
{
constexpr qint64 MAX_IPC_COMMAND_SIZE = 8192;

#ifdef Q_OS_UNIX
struct DirectoryHandle
{
    int fd = -1;
    explicit DirectoryHandle(const QString& path)
    {
        const QByteArray encoded = QFile::encodeName(path);
        fd = ::open(encoded.constData(), O_RDONLY | O_DIRECTORY | O_NOFOLLOW | O_CLOEXEC);
        struct stat info{};
        if (fd >= 0 && (::fstat(fd, &info) != 0 || !S_ISDIR(info.st_mode) ||
            info.st_uid != ::geteuid() || (info.st_mode & 0077) != 0))
        {
            ::close(fd);
            fd = -1;
        }
    }
    ~DirectoryHandle() { if (fd >= 0) ::close(fd); }
    DirectoryHandle(const DirectoryHandle&) = delete;
    DirectoryHandle& operator=(const DirectoryHandle&) = delete;
};

bool ownedSocket(const DirectoryHandle& directory, const QByteArray& name, struct stat& info)
{
    return directory.fd >= 0 &&
        ::fstatat(directory.fd, name.constData(), &info, AT_SYMLINK_NOFOLLOW) == 0 &&
        S_ISSOCK(info.st_mode) && info.st_uid == ::geteuid();
}

bool ownedPeer(const QLocalSocket& socket)
{
#ifdef Q_OS_LINUX
    struct ucred credentials{};
    socklen_t size = sizeof(credentials);
    return ::getsockopt(static_cast<int>(socket.socketDescriptor()), SOL_SOCKET,
        SO_PEERCRED, &credentials, &size) == 0 && size == sizeof(credentials) &&
        credentials.uid == ::geteuid();
#elif defined(Q_OS_DARWIN) || defined(Q_OS_FREEBSD) || defined(Q_OS_OPENBSD) || defined(Q_OS_NETBSD)
    uid_t uid;
    gid_t gid;
    return ::getpeereid(static_cast<int>(socket.socketDescriptor()), &uid, &gid) == 0 && uid == ::geteuid();
#else
    return false;
#endif
}
#endif
}

IPC::IPC(QObject* parent): QObject(parent)
{
#ifdef Q_OS_UNIX
    QString runtime = QFile::decodeName(qgetenv("XDG_RUNTIME_DIR"));
    if (runtime.isEmpty())
    {
        runtime = QDir::temp().filePath(QStringLiteral("salvium-gui-%1").arg(::geteuid()));
        // mkdir is atomic; an existing directory is accepted only after its
        // ownership, type and permissions have been checked below.
        ::mkdir(QFile::encodeName(runtime).constData(), 0700);
    }
    if (!QDir::isAbsolutePath(runtime) || DirectoryHandle(runtime).fd < 0)
    {
        qWarning() << "IPC disabled: no private runtime directory";
        return;
    }
    m_socketFile.setFile(QDir(runtime).filePath(QStringLiteral("salvium-gui-%1.sock").arg(::geteuid())));
#else
    m_socketFile.setFile(QDir::temp().filePath(QStringLiteral("salvium-gui_%1.sock").arg(getAccountName())));
#endif
}

// Start listening for incoming IPC commands on UDS (Unix) or named pipe (Windows)
void IPC::bind(){
    if (m_socketFile.filePath().isEmpty() || m_server)
        return;
    const QString path = m_socketFile.absoluteFilePath();
#ifdef Q_OS_UNIX
    DirectoryHandle directory(m_socketFile.absolutePath());
    if (directory.fd < 0)
        return;
    QLockFile bindLock(path + QStringLiteral(".lock"));
    if (!bindLock.tryLock(0))
        return;
#endif

    this->m_server = new QLocalServer(this);
#ifdef Q_OS_UNIX
    // The private runtime directory restricts Unix access. Qt's explicit
    // access options bind a temporary socket then rename it over the target,
    // which would replace a live socket before we can check for staleness.
#else
    this->m_server->setSocketOptions(QLocalServer::UserAccessOption);
#endif

    if(!this->m_server->listen(path)){
#ifdef Q_OS_UNIX
        const QByteArray name = QFile::encodeName(m_socketFile.fileName());
        struct stat original{}, current{};
        if (m_server->serverError() == QAbstractSocket::AddressInUseError &&
            ownedSocket(directory, name, original))
        {
            QLocalSocket probe;
            probe.connectToServer(path);
            const bool connected = probe.waitForConnected(1000);
            // Only a refused connection establishes staleness. Timeouts and
            // permission failures must never cause an existing path's removal.
            if (!connected && probe.error() == QLocalSocket::ConnectionRefusedError &&
                ownedSocket(directory, name, current) &&
                current.st_dev == original.st_dev && current.st_ino == original.st_ino &&
                ::unlinkat(directory.fd, name.constData(), 0) == 0)
            {
                m_server->listen(path);
            }
        }
#endif
        if (!m_server->isListening())
        {
            qWarning() << "IPC server unavailable:" << m_server->errorString();
            delete m_server;
            m_server = nullptr;
            return;
        }
    }

    connect(this->m_server, &QLocalServer::newConnection, this, &IPC::handleConnection);
}

// Process incoming IPC command. First check if monero-wallet-gui is
// already running. If it is, send it to that instance instead, if not,
// queue the command for later use inside our QML engine. Returns true
// when queued, false if sent to another instance, at which point we can
// kill the current process.
bool IPC::saveCommand(QString cmdString){
    const QByteArray buffer = cmdString.toUtf8();
    if (buffer.size() > MAX_IPC_COMMAND_SIZE)
        return true;
    SetQueuedCmd(cmdString);
    if (m_socketFile.filePath().isEmpty())
        return true;
#ifdef Q_OS_UNIX
    DirectoryHandle directory(m_socketFile.absolutePath());
    struct stat info{};
    if (!ownedSocket(directory, QFile::encodeName(m_socketFile.fileName()), info))
        return true;
#endif

    QLocalSocket ls;
    QString socketFilePath = this->socketFile().filePath();

    ls.connectToServer(socketFilePath, QIODevice::WriteOnly);
    if(ls.waitForConnected(1000)){
#ifdef Q_OS_UNIX
        if (!ownedPeer(ls))
            return true;
#endif
        if (ls.write(buffer) != buffer.size())
            return true;
        QElapsedTimer deadline;
        deadline.start();
        while (ls.bytesToWrite() != 0)
        {
            const qint64 remaining = 1000 - deadline.elapsed();
            if (remaining <= 0 || !ls.waitForBytesWritten(static_cast<int>(remaining)))
                return true;
        }

        m_queuedCmd.clear();
        return false;
    }

    if(ls.isOpen())
        ls.disconnectFromServer();

    // Queue for later
    return true;
}

bool IPC::saveCommand(const QUrl &url){
    return this->saveCommand(url.toString());
}

void IPC::handleConnection(){
    if (!m_server)
        return;
    QLocalSocket *clientConnection = this->m_server->nextPendingConnection();
    if (!clientConnection)
        return;
#ifdef Q_OS_UNIX
    if (!ownedPeer(*clientConnection))
    {
        delete clientConnection;
        return;
    }
#endif
    connect(clientConnection, &QLocalSocket::disconnected,
            clientConnection, &QLocalSocket::deleteLater);

    clientConnection->waitForReadyRead(100);
    const QByteArray command = clientConnection->read(MAX_IPC_COMMAND_SIZE + 1);
    if (command.size() > MAX_IPC_COMMAND_SIZE || clientConnection->bytesAvailable() > 0) {
        qWarning() << "Rejected oversized IPC command";
        clientConnection->close();
        delete clientConnection;
        return;
    }
    QString cmdString = QString::fromUtf8(command);

    this->parseCommand(cmdString);

    clientConnection->close();
    delete clientConnection;
}

void IPC::parseCommand(const QUrl &url){
    this->parseCommand(url.toString());
}

void IPC::parseCommand(QString cmdString){
    if (cmdString.size() > MAX_IPC_COMMAND_SIZE || !cmdString.contains(reURI))
        return;

    const QUrl url(cmdString, QUrl::StrictMode);
    if (!url.isValid() || url.scheme().compare(QStringLiteral("salvium"), Qt::CaseInsensitive) != 0 ||
        !url.userInfo().isEmpty() || !url.fragment().isEmpty())
        return;

    this->emitUriHandler(cmdString);
}

void IPC::emitUriHandler(QString uriString){
    emit uriHandler(uriString);
}
