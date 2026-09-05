// Copyright (c) 2026, Salvium
// SPDX-License-Identifier: BSD-3-Clause
#pragma once

#include <QFile>
#include <QFileInfo>
#include <QSaveFile>

namespace p2pool
{
// The source is extracted from the verified archive into a private directory.
// Failed reads, writes or renames must preserve the installed executable.
inline bool installExecutable(const QString& source, const QString& destination)
{
    const QFileInfo sourceInfo(source);
    if (!sourceInfo.isFile() || sourceInfo.isSymLink() || sourceInfo.size() == 0 ||
        QFileInfo(destination).isSymLink())
        return false;
    QFile input(source);
    QSaveFile output(destination);
    output.setDirectWriteFallback(false);
    if (!input.open(QIODevice::ReadOnly) || !output.open(QIODevice::WriteOnly))
        return false;
    while (!input.atEnd())
    {
        const QByteArray chunk = input.read(65536);
        if (input.error() != QFileDevice::NoError || chunk.isEmpty() ||
            output.write(chunk) != chunk.size())
            return false;
    }
    const auto permissions = QFileDevice::ReadOwner | QFileDevice::WriteOwner | QFileDevice::ExeOwner |
        QFileDevice::ReadGroup | QFileDevice::ExeGroup | QFileDevice::ReadOther | QFileDevice::ExeOther;
    return output.setPermissions(permissions) && output.commit();
}
}
