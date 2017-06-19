/*
 * Copyright (C) 2016 Aleix Pol Gonzalez <aleixpol@kde.org>
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

#ifndef APPSTREAMQT_SUGGESTED_H
#define APPSTREAMQT_SUGGESTED_H

#include <QSharedDataPointer>
#include <QObject>
#include "appstreamqt_export.h"

class QUrl;
class QString;
struct _AsSuggested;
namespace AppStream {

class SuggestedData;

/**
 * This class provides a list of other component-ids suggested by a software component, as well
 * as an origin of the suggestion (manually suggested by the upstream project, or
 * automatically determined by heuristics)..
 */
class APPSTREAMQT_EXPORT Suggested {
    Q_GADGET
    public:
        enum Kind {
            KindUnknown,
            KindUpstream,
            KindHeuristic
        };
        Q_ENUM(Kind)

        Suggested();
        Suggested(_AsSuggested *suggested);
        Suggested(const Suggested& other);
        ~Suggested();

        Suggested& operator=(const Suggested& other);

        /**
         * \returns the internally stored AsSuggested
         */
        _AsSuggested *suggested() const;

        /**
         * \return the kind of suggestion
         */
        Kind kind() const;
        void setKind(Kind kind);

        /**
         * \return the local or remote url for this suggested
         */
        const QStringList ids() const;
        void addSuggested(const QString &id);

    private:
        QSharedDataPointer<SuggestedData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Suggested& suggested);

#endif // APPSTREAMQT_SUGGESTED_H
