/*
 * Copyright (C) 2021-2022 Matthias Klumpp <matthias@tenstral.net>
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

struct _AsRelation;

namespace AppStream {

class RelationData;

/**
 * Description of relations a software component has with other components and entities.
 */
class APPSTREAMQT_EXPORT Relation {
    Q_GADGET
    public:
        enum Kind {
            KindUnknown,
            KindRequires,
            KindRecommends
        };
        Q_ENUM(Kind)

        enum ItemKind {
            ItemKindUnknown,
            ItemKindId,
            ItemKindModalias,
            ItemKindKernel,
            ItemKindMemory,
            ItemKindFirmware,
            ItemKindControl,
            ItemKindDisplayLength
        };
        Q_ENUM(ItemKind)

        enum Compare {
            CompareUnknown,
            CompareEq,
            CompareNe,
            CompareLt,
            CompareGt,
            CompareLe,
            CompareGe
        };
        Q_ENUM(Compare)

        enum ControlKind {
            ControlKindUnknown,
            ControlKindPointing,
            ControlKindKeyboard,
            ControlKindConsole,
            ControlKindTouch,
            ControlKindGamepad,
            ControlKindVoice,
            ControlKindVision,
            ControlKindTvRemote
        };
        Q_ENUM(ControlKind)

        enum DisplaySideKind {
            DisplaySideKindUnknown,
            DisplaySideKindShortest,
            DisplaySideKindLongest
        };
        Q_ENUM(DisplaySideKind)

        enum DisplayLengthKind {
            DisplayLengthKindUnknown,
            DisplayLengthKindXSmall,
            DisplayLengthKindSmall,
            DisplayLengthKindMedium,
            DisplayLengthKindLarge,
            DisplayLengthKindXLarge
        };
        Q_ENUM(DisplayLengthKind)

        static QString kindToString(Kind kind);
        static Kind stringToKind(const QString &string);

        static QString itemKindToString(ItemKind ikind);
        static ItemKind stringToItemKind(const QString &string);

        static Compare stringToCompare(const QString &string);
        static QString compareToString(Compare cmp);
        static QString compareToSymbolsString(Compare cmp);

        static QString controlKindToString(ControlKind ckind);
        static ControlKind controlKindFromString(const QString &string);

        static QString displaySideKindToString(DisplaySideKind kind);
        static DisplaySideKind stringToDisplaySideKind(const QString &string);

        static QString displayLengthKindToString(DisplayLengthKind kind);
        static DisplayLengthKind stringToDisplayLengthKind(const QString &string);

        Relation();
        Relation(_AsRelation* relation);
        Relation(const Relation& relation);
        ~Relation();

        Relation& operator=(const Relation& relation);
        bool operator==(const Relation& r) const;

        /**
         * \returns the internally stored AsRelation
         */
        _AsRelation *asRelation() const;

        Kind kind() const;
        void setKind(Kind kind);

        ItemKind itemKind() const;
        void setItemKind(ItemKind kind);

        Compare compare() const;
        void setCompare(Compare compare);

        QString version() const;
        void setVersion(const QString &version);

        QString valueStr() const;
        void setValueStr(const QString &value);

        int valueInt() const;
        void setValueInt(int value);

        ControlKind valueControlKind() const;
        void setValueControlKind(ControlKind kind);

        DisplaySideKind displaySideKind() const;
        void setDisplaySideKind(DisplaySideKind kind);

        int valuePx() const;
        void setValuePx(int logicalPx);

        DisplayLengthKind valueDisplayLengthKind() const;
        void setValueDisplayLengthKind(DisplayLengthKind kind);

        bool versionCompare(const QString &version);

        QString lastError() const;

    private:
        QSharedDataPointer<RelationData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Relation& relation);
