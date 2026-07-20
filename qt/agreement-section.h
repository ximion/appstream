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

struct _AsAgreementSection;

namespace AppStream
{

class AgreementSectionData;

/**
 * A section of an \ref Agreement.
 */
class APPSTREAMQT_EXPORT AgreementSection
{
    Q_GADGET

public:
    AgreementSection();
    AgreementSection(_AsAgreementSection *section);
    AgreementSection(const AgreementSection &other);
    ~AgreementSection();

    AgreementSection &operator=(const AgreementSection &other);
    bool operator==(const AgreementSection &other) const;

    /**
     * \returns the internally stored AsAgreementSection
     */
    _AsAgreementSection *cPtr() const;

    QString kind() const;
    void setKind(const QString &kind);

    QString name() const;
    void setName(const QString &name, const QString &lang = {});

    QString description() const;
    void setDescription(const QString &description, const QString &lang = {});

private:
    QSharedDataPointer<AgreementSectionData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::AgreementSection &section);
