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

#ifndef APPSTREAMQT_ISSUE_H
#define APPSTREAMQT_ISSUE_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsIssue;
namespace AppStream
{

class IssueData;
class APPSTREAMQT_EXPORT Issue
{
    Q_GADGET

public:
    Issue();
    Issue(_AsIssue *issue);
    Issue(const Issue &issue);
    ~Issue();

    Issue &operator=(const Issue &issue);
    bool operator==(const Issue &r) const;

    /**
     * \returns the internally stored AsIssue
     */
    _AsIssue *cPtr() const;

    enum Kind {
        KindUnknown,
        KindGeneric,
        KindCve,
        KindGcve
    };
    Q_ENUM(Kind)

    static Kind stringToKind(const QString &kindString);
    static QString kindToString(Kind kind);

    /**
     * \return the issue kind.
     */
    Kind kind() const;
    void setKind(Kind kind);

    /**
     * \return the issue ID, e.g. a bug number or (G)CVE identifier.
     */
    QString id() const;
    void setId(const QString &id);

    /**
     * \return a URL with details about this issue.
     * For CVE and GCVE issues, a link to the respective entry in the
     * Global CVE Allocation System (GCVE) database is synthesized if
     * no explicit URL was set.
     */
    QString url() const;
    void setUrl(const QString &url);

    /**
     * \return a URL to fetch machine-readable JSON data about this issue
     * from the Global CVE Allocation System (GCVE) database, if this issue
     * references a CVE or GCVE entry.
     */
    QString jsonUrl() const;

private:
    QSharedDataPointer<IssueData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Issue &issue);

#endif // APPSTREAMQT_ISSUE_H
