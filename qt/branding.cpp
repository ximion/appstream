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

#include "branding.h"
#include "appstream.h"
#include "chelpers.h"

using namespace AppStream;

class AppStream::BrandingData : public QSharedData
{
public:
    BrandingData()
    {
        brandingp = as_branding_new();
    }

    BrandingData(AsBranding *branding)
        : brandingp(branding)
    {
        g_object_ref(brandingp);
    }

    ~BrandingData()
    {
        g_object_unref(brandingp);
    }

    bool operator==(const BrandingData &rd) const
    {
        return rd.brandingp == brandingp;
    }

    AsBranding *brandingp;
};

Branding::Branding()
    : d(new BrandingData)
{
}

Branding::Branding(_AsBranding *brandingp)
    : d(new BrandingData(brandingp))
{
}

Branding::Branding(const Branding &branding) = default;

Branding::~Branding() = default;

Branding &Branding::operator=(const Branding &branding) = default;

bool Branding::operator==(const Branding &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsBranding *Branding::cPtr() const
{
    return d->brandingp;
}

QString Branding::colorKindToString(Branding::ColorKind colorKind)
{
    return valueWrap(as_color_kind_to_string(static_cast<AsColorKind>(colorKind)));
}

Branding::ColorKind Branding::colorKindfromString(const QString &string)
{
    return static_cast<Branding::ColorKind>(as_color_kind_from_string(qPrintable(string)));
}

QString Branding::colorSchemeToString(ColorSchemeKind colorScheme)
{
    return valueWrap(as_color_scheme_kind_to_string(static_cast<AsColorSchemeKind>(colorScheme)));
}

Branding::ColorSchemeKind Branding::colorSchemefromString(const QString &string)
{
    return static_cast<Branding::ColorSchemeKind>(as_color_scheme_kind_from_string(qPrintable(string)));
}

void Branding::setColor(Branding::ColorKind kind, Branding::ColorSchemeKind scheme, const QString &color)
{
    as_branding_set_color(d->brandingp, static_cast<AsColorKind>(kind), static_cast<AsColorSchemeKind>(scheme), qPrintable(color));
}

void Branding::removeColor(Branding::ColorKind kind, Branding::ColorSchemeKind scheme)
{
    as_branding_remove_color(d->brandingp, static_cast<AsColorKind>(kind), static_cast<AsColorSchemeKind>(scheme));
}

QString Branding::color(Branding::ColorKind kind, Branding::ColorSchemeKind scheme)
{
    return valueWrap(as_branding_get_color(d->brandingp, static_cast<AsColorKind>(kind), static_cast<AsColorSchemeKind>(scheme)));
}
