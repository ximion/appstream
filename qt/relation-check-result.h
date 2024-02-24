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
#include <optional>
#include "appstreamqt_export.h"

struct _AsRelationCheckResult;
namespace AppStream
{

class RelationCheckResultData;
class APPSTREAMQT_EXPORT RelationCheckResult
{
    Q_GADGET

public:
    RelationCheckResult();
    RelationCheckResult(_AsRelationCheckResult *relcr);
    RelationCheckResult(const RelationCheckResult &relcr);
    ~RelationCheckResult();

    RelationCheckResult &operator=(const RelationCheckResult &relcr);
    bool operator==(const RelationCheckResult &r) const;

    /**
     * \returns the internally stored AsRelationCheckResult
     */
    _AsRelationCheckResult *cPtr() const;

    enum Status {
        StatusUnknown,
        StatusError,
        StatusNotSatisfied,
        StatusSatisfied
    };
    Q_ENUM(Status)

    Status status() const;
    void setStatus(Status status);

    QString message() const;
    void setMessage(const QString &text);

private:
    QSharedDataPointer<RelationCheckResultData> d;
};

int compatibilityScoreFromRelationCheckResults(const QList<RelationCheckResult> &rcResults);
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::RelationCheckResult &relcr);
