/*
 * Copyright (C) 2016-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include "appstreamqt_export.h"

struct _AsReference;

namespace AppStream
{

class ReferenceData;

/**
 * A reference to external resources, e.g. a DOI or a registry entry.
 */
class APPSTREAMQT_EXPORT Reference
{
    Q_GADGET

public:
    enum Kind {
        KindUnknown,
        KindDoi,
        KindCitationCff,
        KindRegistry
    };
    Q_ENUM(Kind)

    static Kind stringToKind(const QString &kindString);
    static QString kindToString(Kind kind);

    Reference();
    Reference(_AsReference *reference);
    Reference(const Reference &other);
    ~Reference();

    Reference &operator=(const Reference &other);
    bool operator==(const Reference &other) const;

    /**
     * \returns the internally stored AsReference
     */
    _AsReference *cPtr() const;

    Kind kind() const;
    void setKind(Kind kind);

    QString value() const;
    void setValue(const QString &value);

    QString registryName() const;
    void setRegistryName(const QString &name);

private:
    QSharedDataPointer<ReferenceData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Reference &reference);
