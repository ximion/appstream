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

#include "appstream.h"
#include "systeminfo.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

class AppStream::SystemInfoData : public QSharedData {
public:
    SystemInfoData()
    {
        sysInfo = as_system_info_new();
    }

    SystemInfoData(AsSystemInfo *systemInfo)
        : sysInfo(systemInfo)
    {
        g_object_ref(sysInfo);
    }

    ~SystemInfoData()
    {
        g_object_unref(sysInfo);
    }

    bool operator==(const SystemInfoData& rd) const
    {
        return rd.sysInfo == sysInfo;
    }

    AsSystemInfo* sysInfo;
    QString lastError;
};

SystemInfo::SystemInfo()
    : d(new SystemInfoData())
{}

SystemInfo::SystemInfo(_AsSystemInfo* sysInfo)
    : d(new SystemInfoData(sysInfo))
{}

SystemInfo::SystemInfo(const SystemInfo &sysInfo) = default;

SystemInfo::~SystemInfo() = default;

SystemInfo& SystemInfo::operator=(const SystemInfo &sysInfo) = default;

bool SystemInfo::operator==(const SystemInfo &other) const
{
    if(this->d == other.d) {
        return true;
    }
    if(this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsSystemInfo * AppStream::SystemInfo::asSystemInfo() const
{
    return d->sysInfo;
}

QAnyStringView SystemInfo::osId() const
{
    return valueWrap(as_system_info_get_os_id(d->sysInfo));
}

QAnyStringView SystemInfo::osCid() const
{
    return valueWrap(as_system_info_get_os_cid(d->sysInfo));
}

QAnyStringView SystemInfo::osName() const
{
    return valueWrap(as_system_info_get_os_name(d->sysInfo));
}

QAnyStringView SystemInfo::osVersion() const
{
    return valueWrap(as_system_info_get_os_version(d->sysInfo));
}

QAnyStringView SystemInfo::osHomepage() const
{
    return valueWrap(as_system_info_get_os_homepage(d->sysInfo));
}

QAnyStringView SystemInfo::kernelName() const
{
    return valueWrap(as_system_info_get_kernel_name(d->sysInfo));
}

QAnyStringView SystemInfo::kernelVersion() const
{
    return valueWrap(as_system_info_get_kernel_version(d->sysInfo));
}

ulong SystemInfo::memoryTotal() const
{
    return as_system_info_get_memory_total(d->sysInfo);
}

QList<QAnyStringView> SystemInfo::modaliases() const
{
    return valueWrap(as_system_info_get_modaliases(d->sysInfo));
}

QAnyStringView SystemInfo::modaliasToSyspath(QAnyStringView modalias)
{
    return valueWrap(as_system_info_modalias_to_syspath(d->sysInfo, stringViewToChar(modalias)));
}

bool SystemInfo::hasDeviceMatchingModalias(QAnyStringView modaliasGlob)
{
    return as_system_info_has_device_matching_modalias(d->sysInfo, stringViewToChar(modaliasGlob));
}

QAnyStringView SystemInfo::deviceNameForModalias(QAnyStringView modalias, bool allowFallback)
{
    g_autoptr(GError) error = nullptr;
    QAnyStringView result;

    result = valueWrap(as_system_info_get_device_name_for_modalias(d->sysInfo,
                                                                   stringViewToChar(modalias),
                                                                   allowFallback,
                                                                   &error));
    if (error != nullptr)
        d->lastError = QString::fromUtf8(error->message);
    return result;
}

CheckResult SystemInfo::hasInputControl(Relation::ControlKind kind)
{
    g_autoptr(GError) error = nullptr;
    CheckResult result;

    result = static_cast<CheckResult>(as_system_info_has_input_control(d->sysInfo,
                                                                       static_cast<AsControlKind>(kind),
                                                                       &error));
    if (error != nullptr)
        d->lastError = QString::fromUtf8(error->message);
    return result;
}

void SystemInfo::setInputControl(Relation::ControlKind kind, bool found)
{
    as_system_info_set_input_control(d->sysInfo,
                                     static_cast<AsControlKind>(kind),
                                     found);
}

ulong SystemInfo::displayLength(Relation::DisplaySideKind kind)
{
    return as_system_info_get_display_length(d->sysInfo,
                                             static_cast<AsDisplaySideKind>(kind));
}

void SystemInfo::setDisplayLength(Relation::DisplaySideKind kind, ulong valueDip)
{
    as_system_info_set_display_length (d->sysInfo,
                                       static_cast<AsDisplaySideKind>(kind),
                                       valueDip);
}

QAnyStringView SystemInfo::lastError() const
{
    return d->lastError;
}
