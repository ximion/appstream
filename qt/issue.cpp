/*
 * Copyright (C) 2026 Matthias Klumpp <matthias@tenstral.net>
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
#include "issue.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

QString Issue::kindToString(Issue::Kind kind)
{
    return as_issue_kind_to_string((AsIssueKind) kind);
}

Issue::Kind Issue::stringToKind(const QString &kindString)
{
    return Issue::Kind(as_issue_kind_from_string(qPrintable(kindString)));
}

class AppStream::IssueData : public QSharedData
{
public:
    IssueData()
    {
        m_issue = as_issue_new();
    }

    IssueData(AsIssue *issue)
        : m_issue(issue)
    {
        g_object_ref(m_issue);
    }

    ~IssueData()
    {
        g_object_unref(m_issue);
    }

    bool operator==(const IssueData &rd) const
    {
        return rd.m_issue == m_issue;
    }

    AsIssue *issue() const
    {
        return m_issue;
    }

    AsIssue *m_issue;
};

Issue::Issue()
    : d(new IssueData())
{
}

Issue::Issue(_AsIssue *issue)
    : d(new IssueData(issue))
{
}

Issue::Issue(const Issue &issue) = default;

Issue::~Issue() = default;

Issue &Issue::operator=(const Issue &issue) = default;

bool Issue::operator==(const Issue &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsIssue *AppStream::Issue::cPtr() const
{
    return d->issue();
}

Issue::Kind Issue::kind() const
{
    return Issue::Kind(as_issue_get_kind(d->m_issue));
}

void Issue::setKind(Kind kind)
{
    as_issue_set_kind(d->m_issue, (AsIssueKind) kind);
}

QString Issue::id() const
{
    return valueWrap(as_issue_get_id(d->m_issue));
}

void Issue::setId(const QString &id)
{
    as_issue_set_id(d->m_issue, qPrintable(id));
}

QString Issue::url() const
{
    return valueWrap(as_issue_get_url(d->m_issue));
}

void Issue::setUrl(const QString &url)
{
    as_issue_set_url(d->m_issue, qPrintable(url));
}

QString Issue::jsonUrl() const
{
    g_autofree gchar *res = as_issue_get_json_url(d->m_issue);
    return QString::fromUtf8(res);
}

QDebug operator<<(QDebug s, const AppStream::Issue &issue)
{
    s.nospace() << "AppStream::Issue(" << issue.id() << ")";
    return s.space();
}
