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
#include <QAnyStringView>
#include "appstreamqt_export.h"

namespace AppStream {

namespace SPDX {
APPSTREAMQT_EXPORT bool isLicenseId(QAnyStringView license_id);
APPSTREAMQT_EXPORT bool isLicenseExceptionId(QAnyStringView exception_id);
APPSTREAMQT_EXPORT bool isLicenseExpression(QAnyStringView license);

APPSTREAMQT_EXPORT bool isMetadataLicense(QAnyStringView license);
APPSTREAMQT_EXPORT bool isFreeLicense(QAnyStringView license);

APPSTREAMQT_EXPORT QList<QAnyStringView> tokenizeLicense(QAnyStringView license);
APPSTREAMQT_EXPORT QAnyStringView detokenizeLicense(QList<QAnyStringView> license_tokens);

APPSTREAMQT_EXPORT QAnyStringView asSpdxId(QAnyStringView license);

APPSTREAMQT_EXPORT QAnyStringView licenseUrl(QAnyStringView license);

}

}

#endif
