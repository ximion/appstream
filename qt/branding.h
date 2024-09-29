// SPDX-FileCopyrightText: 2024 Carl Schwan <carl@carlschwan.eu>
// SPDX-License-Identifier: LGPL-2.1-or-later

#pragma once

#include <QObject>
#include <QSharedDataPointer>
#include "appstreamqt_export.h"

class _AsBranding;

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
    Branding(const Branding &devp);
    ~Branding();

    Branding &operator=(const Branding &devp);
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
