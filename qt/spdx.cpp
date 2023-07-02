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

bool AppStream::SPDX::isLicenseId(QAnyStringView license_id)
{
    return as_is_spdx_license_id(stringViewToChar(license_id));
}

bool AppStream::SPDX::isLicenseExceptionId(QAnyStringView exception_id)
{
    return as_is_spdx_license_exception_id(stringViewToChar(exception_id));
}

bool AppStream::SPDX::isLicenseExpression(QAnyStringView license)
{
    return as_is_spdx_license_expression(stringViewToChar(license));
}

bool AppStream::SPDX::isMetadataLicense(QAnyStringView license)
{
    return as_license_is_metadata_license(stringViewToChar(license));
}

bool AppStream::SPDX::isFreeLicense(QAnyStringView license)
{
    return as_license_is_free_license(stringViewToChar(license));
}

QList<QAnyStringView> AppStream::SPDX::tokenizeLicense(QAnyStringView license)
{
    g_auto(GStrv) strv = as_spdx_license_tokenize(stringViewToChar(license));
    return AppStream::valueWrap(strv);
}

QAnyStringView AppStream::SPDX::detokenizeLicense(QList<QAnyStringView> license_tokens)
{
    g_autofree gchar *res = NULL;
    g_auto(GStrv) tokens = NULL;

    tokens = AppStream::stringListToCharArray(license_tokens);
    res = as_spdx_license_detokenize(tokens);
    return valueWrap(res);
}

QAnyStringView AppStream::SPDX::asSpdxId(QAnyStringView license)
{
    g_autofree gchar *res = as_license_to_spdx_id(stringViewToChar(license));
    return valueWrap(res);
}

QAnyStringView AppStream::SPDX::licenseUrl(QAnyStringView license)
{
    g_autofree gchar *res = as_get_license_url(stringViewToChar(license));
    return valueWrap(res);
}
