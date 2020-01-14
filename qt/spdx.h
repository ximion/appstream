/*
 * Copyright (C) 2019 Aleix Pol Gonzalez <aleixpol@kde.rog>
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

#ifndef APPSTREAMQT_SPDX_H
#define APPSTREAMQT_SPDX_H

#include <QStringList>
#include "appstreamqt_export.h"

namespace AppStream {

namespace SPDX {
APPSTREAMQT_EXPORT bool isLicenseId(const QString &license_id);
APPSTREAMQT_EXPORT bool isLicenseExceptionId(const QString &exception_id);
APPSTREAMQT_EXPORT bool isLicenseExpression(const QString &license);

APPSTREAMQT_EXPORT bool isMetadataLicense(const QString &license);
APPSTREAMQT_EXPORT bool isFreeLicense(const QString &license);

APPSTREAMQT_EXPORT QStringList tokenizeLicense(const QString &license);
APPSTREAMQT_EXPORT QString detokenizeLicense(const QStringList &license_tokens);

APPSTREAMQT_EXPORT QString asSpdxId(const QString &license);

APPSTREAMQT_EXPORT QString licenseUrl(const QString &license);

}

}

#endif
