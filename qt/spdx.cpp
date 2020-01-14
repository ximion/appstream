/*
 * Copyright(C) 2019 Aleix Pol Gonzalez <aleixpol@kde.rog>
 *
 * Licensed under the GNU Lesser General Public License Version 2.1
 *
 * This library is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 2.1 of the license, or
 *(at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with this library.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "spdx.h"

#include "appstream.h"
#include "chelpers.h"

bool AppStream::SPDX::isLicenseId(const QString &license_id)
{
    return as_is_spdx_license_id(qPrintable(license_id));
}

bool AppStream::SPDX::isLicenseExceptionId(const QString &exception_id)
{
    return as_is_spdx_license_exception_id(qPrintable(exception_id));
}

bool AppStream::SPDX::isLicenseExpression(const QString &license)
{
    return as_is_spdx_license_expression(qPrintable(license));
}

bool AppStream::SPDX::isMetadataLicense(const QString &license)
{
    return as_license_is_metadata_license(qPrintable(license));
}

bool AppStream::SPDX::isFreeLicense(const QString &license)
{
    return as_license_is_free_license(qPrintable(license));
}

QStringList AppStream::SPDX::tokenizeLicense(const QString &license)
{
    g_auto(GStrv) strv = as_spdx_license_tokenize(qPrintable(license));
    return AppStream::valueWrap(strv);
}

QString AppStream::SPDX::detokenizeLicense(const QStringList &license_tokens)
{
    g_autofree gchar *res = NULL;
    g_auto(GStrv) tokens = NULL;

    tokens = AppStream::stringListToCharArray(license_tokens);
    res = as_spdx_license_detokenize(tokens);
    return QString::fromUtf8(res);
}

QString AppStream::SPDX::asSpdxId(const QString &license)
{
    g_autofree gchar *res = as_license_to_spdx_id(qPrintable(license));
    return QString::fromUtf8(res);
}

QString AppStream::SPDX::licenseUrl(const QString &license)
{
    g_autofree gchar *res = as_get_license_url(qPrintable(license));
    return QString::fromUtf8(res);
}
