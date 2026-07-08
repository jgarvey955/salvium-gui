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

#include "P2PoolManager.h"
#include "net/http_client.h"
#include "common/util.h"
#include "qt/utils.h"
#include <QElapsedTimer>
#include <QFile>
#include <QMutexLocker>
#include <QFileInfo>
#include <QDir>
#include <QDebug>
#include <QUrl>
#include <QtConcurrent/QtConcurrent>
#include <QApplication>
#include <QProcess>
#include <QMap>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QTextStream>

#if defined(Q_OS_MACOS) && defined(__aarch64__) && !defined(Q_OS_MACOS_AARCH64)
#define Q_OS_MACOS_AARCH64
#endif

namespace
{
    const QString P2POOL_PROJECT_ID = "80288850";
    const QString P2POOL_PACKAGE = "p2pool-salvium";
    const QString P2POOL_DEFAULT_VERSION = "v4.27";

    QString p2poolArchiveSuffix()
    {
    #ifdef Q_OS_WIN
        return "windows-x64.zip";
    #elif defined(Q_OS_LINUX)
        return "linux-x64-static.tar.gz";
    #elif defined(Q_OS_MACOS_AARCH64)
        return "macos-aarch64.tar.gz";
    #elif defined(Q_OS_MACOS)
        return "macos-x64.tar.gz";
    #else
        return "";
    #endif
    }

    QString p2poolArchiveName(const QString &version)
    {
        const QString suffix = p2poolArchiveSuffix();
        if (suffix.isEmpty()) {
            return "";
        }
        return QString("%1-%2-%3").arg(P2POOL_PACKAGE, version, suffix);
    }

    QUrl p2poolDownloadUrl(const QString &version)
    {
        const QString archiveName = p2poolArchiveName(version);
        if (archiveName.isEmpty()) {
            return {};
        }
        return QUrl(QString("https://gitlab.com/api/v4/projects/%1/packages/generic/%2/%3/%4")
                        .arg(P2POOL_PROJECT_ID, P2POOL_PACKAGE, version, archiveName));
    }

    QUrl p2poolLatestReleaseUrl()
    {
        return QUrl(QString("https://gitlab.com/api/v4/projects/%1/releases?per_page=1").arg(P2POOL_PROJECT_ID));
    }

    std::string requestTarget(const QUrl &url)
    {
        QString target = url.path();
        const QString query = url.query(QUrl::FullyEncoded);
        if (!query.isEmpty()) {
            target += "?" + query;
        }
        return target.toStdString();
    }

    bool httpGet(const QUrl &url, epee::net_utils::http::http_simple_client &http_client, const epee::net_utils::http::http_response_info **response)
    {
        std::string userAgent = randomUserAgent().toStdString();
        std::chrono::milliseconds timeout = std::chrono::seconds(10);
        http_client.set_server(url.host().toStdString(), "443", {});
        return http_client.invoke_get(requestTarget(url), timeout, {}, response, {{"User-Agent", userAgent}});
    }
}

void P2PoolManager::download() {
    downloadVersion(P2POOL_DEFAULT_VERSION);
}

void P2PoolManager::update() {
    QString latestVersion;
    {
        QMutexLocker locker(&m_latestVersionMutex);
        latestVersion = m_latestVersion;
    }

    const QString version = latestVersion.isEmpty() ? P2POOL_DEFAULT_VERSION : latestVersion;
    downloadVersion(version);
}

void P2PoolManager::downloadVersion(const QString &version) {
    m_scheduler.run([this, version] {
        QUrl url = p2poolDownloadUrl(version);
        const QString archiveName = p2poolArchiveName(version);
        const QString fileName = m_p2poolPath + "/" + archiveName;
        if (!url.isValid() || archiveName.isEmpty()) {
            emit p2poolDownloadFailure(BinaryNotAvailable);
            return;
        }

        QFile file(fileName);
        epee::net_utils::http::http_simple_client http_client;
        const epee::net_utils::http::http_response_info* response = NULL;
        bool success = httpGet(url, http_client, std::addressof(response));
        if (success && response->m_response_code == 404) {
            emit p2poolDownloadFailure(BinaryNotAvailable);
            return;
        } else if (success && response->m_response_code == 302) {
            epee::net_utils::http::fields_list fields = response->m_header_info.m_etc_fields;
            for (std::pair<std::string, std::string> i : fields) {
                if (i.first == "Location") {
                    url = QString::fromStdString(i.second);
                    http_client.set_server(url.host().toStdString(), "443", {});
                    http_client.wipe_response();
                    success = httpGet(url, http_client, std::addressof(response));
                }
            }
        }
        if (!success) {
            emit p2poolDownloadFailure(ConnectionIssue);
        }
        else {
            std::string stringData = response->m_body;
            QByteArray data(stringData.c_str(), stringData.length());
            if (!file.open(QIODevice::WriteOnly)) {
                emit p2poolDownloadFailure(InstallationFailed);
                return;
            }

            file.write(data);
            file.close();

            // Optional integrity check (disabled):
            // QByteArray hashData = QCryptographicHash::hash(data, QCryptographicHash::Sha256);
            // QString hash = hashData.toHex();
            // if (hash != validHash) {
            //     emit p2poolDownloadFailure(HashVerificationFailed);
            //     return;
            // }

            int extractResult;
            if (fileName.endsWith(".zip")) {
                extractResult = QProcess::execute("tar", {"-xf", fileName, "-C", m_p2poolPath});
            } else {
                extractResult = QProcess::execute("tar", {"-xzf", fileName, "-C", m_p2poolPath});
            }

            QFile::remove(fileName);

            if (extractResult != 0) {
                emit p2poolDownloadFailure(InstallationFailed);
                return;
            }

            #ifdef Q_OS_WIN
            if (!QFileInfo(m_p2pool).isFile()) {
                const QString extractedP2Pool = m_p2poolPath + "/Release/p2pool-salvium.exe";
                if (QFileInfo(extractedP2Pool).isFile()) {
                    QFile::remove(m_p2pool);
                    QFile::rename(extractedP2Pool, m_p2pool);
                }
            }
            #endif

            if (isInstalled()) {
                QFile versionFile(m_p2poolPath + "/p2pool-salvium.version");
                if (versionFile.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
                    QTextStream out(&versionFile);
                    out << version << "\n";
                }
                emit p2poolDownloadSuccess();
            }
            else {
                emit p2poolDownloadFailure(InstallationFailed);
            }
        }
    });
    return;
}

void P2PoolManager::checkForUpdates() {
    m_scheduler.run([this] {
        epee::net_utils::http::http_simple_client http_client;
        const epee::net_utils::http::http_response_info* response = NULL;
        bool success = httpGet(p2poolLatestReleaseUrl(), http_client, std::addressof(response));
        if (!success || !response || response->m_response_code != 200) {
            emit p2poolUpdateCheckFailure();
            return;
        }

        const QJsonDocument doc = QJsonDocument::fromJson(QByteArray::fromStdString(response->m_body));
        if (!doc.isArray() || doc.array().isEmpty() || !doc.array().first().isObject()) {
            emit p2poolUpdateCheckFailure();
            return;
        }

        const QString latestVersion = doc.array().first().toObject().value("tag_name").toString();
        if (latestVersion.isEmpty()) {
            emit p2poolUpdateCheckFailure();
            return;
        }

        {
            QMutexLocker locker(&m_latestVersionMutex);
            m_latestVersion = latestVersion;
        }

        const QString installedVersion = currentVersion();
        if (installedVersion != latestVersion) {
            emit p2poolUpdateAvailable(installedVersion, latestVersion);
        }
        else {
            emit p2poolUpdateNotAvailable(installedVersion);
        }
    });
}

bool P2PoolManager::isInstalled() const {
    if (!QFileInfo(m_p2pool).isFile())
    {
        return false;
    }
    return true;
}

QString P2PoolManager::currentVersion() const {
    QFile versionFile(m_p2poolPath + "/p2pool-salvium.version");
    if (versionFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        const QString version = QString::fromUtf8(versionFile.readAll()).trimmed();
        if (!version.isEmpty()) {
            return version;
        }
    }

    if (!isInstalled()) {
        return "not installed";
    }

    QProcess versionProcess;
    versionProcess.start(m_p2pool, {"--version"});
    if (versionProcess.waitForFinished(3000)) {
        const QString output = QString::fromUtf8(versionProcess.readAllStandardOutput())
            + QString::fromUtf8(versionProcess.readAllStandardError());
        const QRegularExpression versionRegex("(v\\d+\\.\\d+)");
        const QRegularExpressionMatch match = versionRegex.match(output);
        if (match.hasMatch()) {
            return match.captured(1);
        }
    }

    return "unknown";
}

void P2PoolManager::getStatus() {
    QString statsPath = m_p2poolPath + "/stats/local/miner";
    bool status = true;
    if (!QFileInfo(statsPath).isFile() || !started)
    {
        status = started;
        emit p2poolStatus(status, 0);
        return;
    }
    QFile statsFile(statsPath);
    statsFile.open(QIODevice::ReadOnly);
    QTextStream statsOut(&statsFile);
    QByteArray data;
    statsOut >> data;
    statsFile.close();
    QJsonDocument json = QJsonDocument::fromJson(data);
    QJsonObject jsonObj = json.object();
    int hashrate = jsonObj.value("current_hashrate").toInt();
    emit p2poolStatus(status, hashrate);
    return;
}

bool P2PoolManager::start(const QString &flags, const QString &address, const QString &chain, const QString &threads)
{
    // prepare command line arguments and pass to p2pool
    QStringList arguments;

    // Custom startup flags for p2pool
    foreach (const QString &str, flags.split(" ")) {
          qDebug() << QString(" [%1] ").arg(str);
          if (!str.isEmpty())
            arguments << str;
    }

    if (!arguments.contains("--local-api")) {
        arguments << "--local-api";
    }

    if (!arguments.contains("--data-api")) {
        QDir dir;
        QString dirName = m_p2poolPath + "/stats/";
        QDir statsDir(dirName);
        if (dir.exists(dirName)) {
            statsDir.removeRecursively();
        }
        dir.mkdir(dirName);
        arguments << "--data-api" << dirName;
    }

    if (!arguments.contains("--start-mining")) {
        arguments << "--start-mining" << threads;
    }

    if (chain == "nano") {
        arguments << "--nano";
    }

    if (chain == "mini") {
        arguments << "--mini";
    }

    if (!arguments.contains("--wallet")) {
        arguments << "--wallet" << address;
    }

    qDebug() << "starting p2pool " + m_p2pool;
    qDebug() << "With command line arguments " << arguments;

    QMutexLocker locker(&m_p2poolMutex);

    if (m_p2poold && m_p2poold->state() != QProcess::NotRunning) {
        return true;
    }

    m_p2poold.reset(new QProcess(this));
    connect(m_p2poold.get(), qOverload<int, QProcess::ExitStatus>(&QProcess::finished),
            this, [this](int, QProcess::ExitStatus) {
                started = false;
                emit p2poolStatus(false, 0);
            });

    // Set program parameters
    m_p2poold->setProgram(m_p2pool);
    m_p2poold->setArguments(arguments);
    m_p2poold->setWorkingDirectory(m_p2poolPath);
    m_p2poold->setStandardOutputFile(QProcess::nullDevice());
    m_p2poold->setStandardErrorFile(QProcess::nullDevice());

    // Start p2pool
    m_p2poold->start();
    started = m_p2poold->waitForStarted(5000);

    if (!started) {
        qDebug() << "P2Pool start error: " + m_p2poold->errorString();
        emit p2poolStartFailure();
        return false;
    }

    return true;
}

void P2PoolManager::exit()
{
    qDebug("P2PoolManager: exit()");
    {
        QMutexLocker locker(&m_p2poolMutex);
        if (m_p2poold && m_p2poold->state() != QProcess::NotRunning) {
            m_p2poold->terminate();
            if (!m_p2poold->waitForFinished(5000)) {
                m_p2poold->kill();
                m_p2poold->waitForFinished(5000);
            }
        }

        if (started) {
        #ifdef Q_OS_WIN
            QProcess::execute("taskkill",  {"/F", "/IM", QFileInfo(m_p2pool).fileName()});
        #else
            QProcess::execute("pkill", {"-x", QFileInfo(m_p2pool).fileName()});
        #endif
        }

        started = false;
        m_p2poold.reset();
    }

    QString dirName = m_p2poolPath + "/stats/";
    QDir dir(dirName);
    dir.removeRecursively();
    emit p2poolStatus(false, 0);
}

P2PoolManager::P2PoolManager(QObject *parent)
    : QObject(parent)
    , m_scheduler(this)
{
    started = false;
    // Platform dependent path to p2pool
#ifdef Q_OS_WIN
    m_p2poolPath = QApplication::applicationDirPath() + "/p2pool";
    if (!QDir(m_p2poolPath).exists()) {
        QDir().mkdir(m_p2poolPath);
    }
    m_p2pool = m_p2poolPath + "/p2pool-salvium.exe";
#elif defined(Q_OS_UNIX)
    m_p2poolPath = QApplication::applicationDirPath();
    m_p2pool = m_p2poolPath + "/p2pool-salvium";
#endif
    if (m_p2pool.length() == 0) {
        qCritical() << "no p2pool binary defined for current platform";
    }
}

P2PoolManager::~P2PoolManager() {
    m_scheduler.shutdownWaitForFinished();
}
