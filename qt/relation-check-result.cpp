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
#include "relation-check-result.h"

#include <QDebug>
#include <QString>
#include "chelpers.h"

using namespace AppStream;

class AppStream::RelationCheckResultData : public QSharedData
{
public:
    RelationCheckResultData()
    {
        m_rcr = as_relation_check_result_new();
    }

    RelationCheckResultData(AsRelationCheckResult *relcr)
        : m_rcr(relcr)
    {
        g_object_ref(m_rcr);
    }

    ~RelationCheckResultData()
    {
        g_object_unref(m_rcr);
    }

    bool operator==(const RelationCheckResultData &rcrd) const
    {
        return rcrd.m_rcr == m_rcr;
    }

    AsRelationCheckResult *RelationCheckResult() const
    {
        return m_rcr;
    }

    AsRelationCheckResult *m_rcr;
};

RelationCheckResult::RelationCheckResult()
    : d(new RelationCheckResultData())
{
}

RelationCheckResult::RelationCheckResult(_AsRelationCheckResult *relcr)
    : d(new RelationCheckResultData(relcr))
{
}

RelationCheckResult::RelationCheckResult(const RelationCheckResult &relcr) = default;

RelationCheckResult::~RelationCheckResult() = default;

RelationCheckResult &RelationCheckResult::operator=(const RelationCheckResult &relcr) = default;

bool RelationCheckResult::operator==(const RelationCheckResult &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsRelationCheckResult *AppStream::RelationCheckResult::cPtr() const
{
    return d->m_rcr;
}

RelationCheckResult::Status RelationCheckResult::status() const
{
    return static_cast<RelationCheckResult::Status>(as_relation_check_result_get_status(d->m_rcr));
}

void RelationCheckResult::setStatus(Status status)
{
    as_relation_check_result_set_status(d->m_rcr, static_cast<AsRelationStatus>(status));
}

QString RelationCheckResult::message() const
{
    return QString::fromUtf8(as_relation_check_result_get_message(d->m_rcr));
}

void RelationCheckResult::setMessage(const QString &text)
{
    as_relation_check_result_set_message(d->m_rcr, "%s", qPrintable(text));
}

QDebug operator<<(QDebug s, const AppStream::RelationCheckResult &rcr)
{
    s.nospace() << "AppStream::RelationCheckResult(" << rcr.status() << rcr.message() << ")";
    return s.space();
}

int AppStream::compatibilityScoreFromRelationCheckResults(
    const QList<RelationCheckResult> &rcResults)
{
    g_autoptr(GPtrArray) rcrs = g_ptr_array_new();
    for (const auto &rcr : rcResults)
        g_ptr_array_add(rcrs, rcr.cPtr());

    return as_relation_check_results_get_compatibility_score(rcrs);
}
