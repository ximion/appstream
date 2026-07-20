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

#include "appstream.h"
#include "agreement-section.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

class AppStream::AgreementSectionData : public QSharedData
{
public:
    AgreementSectionData()
    {
        m_section = as_agreement_section_new();
    }

    AgreementSectionData(AsAgreementSection *section)
        : m_section(section)
    {
        g_object_ref(m_section);
    }

    ~AgreementSectionData()
    {
        g_object_unref(m_section);
    }

    bool operator==(const AgreementSectionData &rd) const
    {
        return rd.m_section == m_section;
    }

    AsAgreementSection *m_section;
};

AgreementSection::AgreementSection()
    : d(new AgreementSectionData)
{
}

AgreementSection::AgreementSection(_AsAgreementSection *section)
    : d(new AgreementSectionData(section))
{
}

AgreementSection::AgreementSection(const AgreementSection &other) = default;

AgreementSection::~AgreementSection() = default;

AgreementSection &AgreementSection::operator=(const AgreementSection &other) = default;

bool AgreementSection::operator==(const AgreementSection &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsAgreementSection *AgreementSection::cPtr() const
{
    return d->m_section;
}

QString AgreementSection::kind() const
{
    return valueWrap(as_agreement_section_get_kind(d->m_section));
}

void AgreementSection::setKind(const QString &kind)
{
    as_agreement_section_set_kind(d->m_section, qPrintable(kind));
}

QString AgreementSection::name() const
{
    return valueWrap(as_agreement_section_get_name(d->m_section));
}

void AgreementSection::setName(const QString &name, const QString &lang)
{
    as_agreement_section_set_name(d->m_section,
                                  qPrintable(name),
                                  lang.isEmpty() ? NULL : qPrintable(lang));
}

QString AgreementSection::description() const
{
    return valueWrap(as_agreement_section_get_description(d->m_section));
}

void AgreementSection::setDescription(const QString &description, const QString &lang)
{
    as_agreement_section_set_description(d->m_section,
                                         qPrintable(description),
                                         lang.isEmpty() ? NULL : qPrintable(lang));
}

QDebug operator<<(QDebug s, const AppStream::AgreementSection &section)
{
    s.nospace() << "AppStream::AgreementSection(" << section.kind() << ":" << section.name() << ")";
    return s.space();
}
