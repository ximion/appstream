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
#include "agreement.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

static_assert(static_cast<int>(Agreement::KindPrivacy) + 1 == AS_AGREEMENT_KIND_LAST,
              "Agreement::Kind is out of sync with AsAgreementKind");

class AppStream::AgreementData : public QSharedData
{
public:
    AgreementData()
    {
        m_agreement = as_agreement_new();
    }

    AgreementData(AsAgreement *agreement)
        : m_agreement(agreement)
    {
        g_object_ref(m_agreement);
    }

    ~AgreementData()
    {
        g_object_unref(m_agreement);
    }

    bool operator==(const AgreementData &rd) const
    {
        return rd.m_agreement == m_agreement;
    }

    AsAgreement *m_agreement;
};

Agreement::Kind Agreement::stringToKind(const QString &kindString)
{
    return static_cast<Agreement::Kind>(as_agreement_kind_from_string(qPrintable(kindString)));
}

QString Agreement::kindToString(Agreement::Kind kind)
{
    return valueWrap(as_agreement_kind_to_string(static_cast<AsAgreementKind>(kind)));
}

Agreement::Agreement()
    : d(new AgreementData)
{
}

Agreement::Agreement(_AsAgreement *agreement)
    : d(new AgreementData(agreement))
{
}

Agreement::Agreement(const Agreement &other) = default;

Agreement::~Agreement() = default;

Agreement &Agreement::operator=(const Agreement &other) = default;

bool Agreement::operator==(const Agreement &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsAgreement *Agreement::cPtr() const
{
    return d->m_agreement;
}

Agreement::Kind Agreement::kind() const
{
    return static_cast<Agreement::Kind>(as_agreement_get_kind(d->m_agreement));
}

void Agreement::setKind(Agreement::Kind kind)
{
    as_agreement_set_kind(d->m_agreement, static_cast<AsAgreementKind>(kind));
}

QString Agreement::versionId() const
{
    return valueWrap(as_agreement_get_version_id(d->m_agreement));
}

void Agreement::setVersionId(const QString &versionId)
{
    as_agreement_set_version_id(d->m_agreement, qPrintable(versionId));
}

std::optional<AgreementSection> Agreement::sectionDefault() const
{
    auto section = as_agreement_get_section_default(d->m_agreement);
    if (section == nullptr)
        return std::nullopt;
    return AgreementSection(section);
}

QList<AgreementSection> Agreement::sections() const
{
    QList<AgreementSection> res;

    auto sections = as_agreement_get_sections(d->m_agreement);
    res.reserve(sections->len);
    for (uint i = 0; i < sections->len; i++) {
        auto section = AS_AGREEMENT_SECTION(g_ptr_array_index(sections, i));
        res.append(AgreementSection(section));
    }
    return res;
}

void Agreement::addSection(const AppStream::AgreementSection &section)
{
    as_agreement_add_section(d->m_agreement, section.cPtr());
}

QDebug operator<<(QDebug s, const AppStream::Agreement &agreement)
{
    s.nospace() << "AppStream::Agreement(" << Agreement::kindToString(agreement.kind()) << ":"
                << agreement.versionId() << ")";
    return s.space();
}
