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
#include <QList>
#include <QObject>
#include <optional>
#include "appstreamqt_export.h"
#include "agreement-section.h"

struct _AsAgreement;

namespace AppStream
{

class AgreementData;

/**
 * An agreement (e.g. an EULA or privacy statement) associated with a \ref Component.
 */
class APPSTREAMQT_EXPORT Agreement
{
    Q_GADGET

public:
    enum Kind {
        KindUnknown,
        KindGeneric,
        KindEula,
        KindPrivacy
    };
    Q_ENUM(Kind)

    static Kind stringToKind(const QString &kindString);
    static QString kindToString(Kind kind);

    Agreement();
    Agreement(_AsAgreement *agreement);
    Agreement(const Agreement &other);
    ~Agreement();

    Agreement &operator=(const Agreement &other);
    bool operator==(const Agreement &other) const;

    /**
     * \returns the internally stored AsAgreement
     */
    _AsAgreement *cPtr() const;

    Kind kind() const;
    void setKind(Kind kind);

    QString versionId() const;
    void setVersionId(const QString &versionId);

    std::optional<AppStream::AgreementSection> sectionDefault() const;
    QList<AppStream::AgreementSection> sections() const;
    void addSection(const AppStream::AgreementSection &section);

private:
    QSharedDataPointer<AgreementData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Agreement &agreement);
