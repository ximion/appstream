/*
 * Copyright (C) 2016-2022 Matthias Klumpp <matthias@tenstral.net>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 * (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#pragma once

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "relation.h"
#include "appstreamqt_export.h"

struct _AsSystemInfo;
namespace AppStream {

class SystemInfoData;
class APPSTREAMQT_EXPORT SystemInfo {
    Q_GADGET
    public:
        SystemInfo();
        SystemInfo(_AsSystemInfo *sysInfo);
        SystemInfo(const SystemInfo& sysInfo);
        ~SystemInfo();

        SystemInfo& operator=(const SystemInfo& sysInfo);
        bool operator==(const SystemInfo& r) const;

        /**
         * \returns the internally stored AsSystemInfo
         */
        _AsSystemInfo *asSystemInfo() const;

        QString osId() const;
        QString osCid() const;
        QString osName() const;
        QString osVersion() const;
        QString osHomepage() const;

        QString kernelName() const;
        QString kernelVersion() const;

        ulong memoryTotal() const;

        QStringList modaliases() const;
        QString modaliasToSyspath(const QString &modalias);

        bool hasDeviceMatchingModalias(const QString &modaliasGlob);
        QString deviceNameForModalias(const QString &modalias, bool allowFallback);

        CheckResult hasInputControl(Relation::ControlKind kind);
        void setInputControl(Relation::ControlKind kind, bool found);

        ulong displayLength(Relation::DisplaySideKind kind);
        void setDisplayLength(Relation::DisplaySideKind kind, ulong valueDip);

        /**
         * \return The last error message received.
         */
        QString lastError() const;

    private:
        QSharedDataPointer<SystemInfoData> d;
};
}
