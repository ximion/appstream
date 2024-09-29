/*
 * Copyright (C) 2024 Carl Schwan <carl@carlschwan.eu>
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

#include <QObject>
#include <QSharedDataPointer>
#include "appstreamqt_export.h"

struct _AsBranding;

namespace AppStream
{

class BrandingData;

class APPSTREAMQT_EXPORT Branding
{
    Q_GADGET

public:
    enum class ColorSchemeKind {
        Unknown,
        Light,
        Dark,
    };

    enum class ColorKind {
        Unknow,
        Primary,
    };

    Branding();
    Branding(_AsBranding *);
    Branding(const Branding &branding);
    ~Branding();

    Branding &operator=(const Branding &branding);
    bool operator==(const Branding &r) const;

    /**
     * Converts the ColorKind enumerated value to an text representation.
     */
    static QString colorKindToString(ColorKind colorKind);

    /**
     * Converts the text representation to an ColorKind enumerated value.
     */
    static ColorKind colorKindfromString(const QString &string);

    /**
     * Converts the ColorScheme enumerated value to an text representation.
     */
    static QString colorSchemeToString(ColorSchemeKind colorScheme);

    /**
     * Converts the text representation to an ColorScheme enumerated value.
     */
    static ColorSchemeKind colorSchemefromString(const QString &string);

    /**
     * Sets a new accent color. If a color of the given kind with the given scheme preference already exists,
     * it will be overriden with the new color code.
     */
    void setColor(ColorKind kind, ColorSchemeKind scheme, const QString &color);

    /**
     * Deletes a color that matches the given type and scheme preference.
     */
    void removeColor(ColorKind kind, ColorSchemeKind scheme);

    /**
     * Retrieve a color of the given @kind that matches @scheme_kind.
     * If a color has no scheme preference defined, it will be returned for either scheme type,
     * unless a more suitable color was found.
     */
    QString color(ColorKind kind, ColorSchemeKind scheme);

    /**
     * \returns the internally stored AsBranding
     */
    _AsBranding *cPtr() const;

private:
    QSharedDataPointer<BrandingData> d;
};

};
