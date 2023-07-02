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
#include <QAnyStringView>
#include <QObject>
#include "appstreamqt_export.h"

struct _AsRelation;

namespace AppStream {

enum class CheckResult {
    Error,
    Unknown,
    False,
    True
};

class Pool;
class SystemInfo;
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

        static QAnyStringView kindToString(Kind kind);
        static Kind stringToKind(QAnyStringView string);

        static QAnyStringView itemKindToString(ItemKind ikind);
        static ItemKind stringToItemKind(QAnyStringView string);

        static Compare stringToCompare(QAnyStringView string);
        static QAnyStringView compareToString(Compare cmp);
        static QAnyStringView compareToSymbolsString(Compare cmp);

        static QAnyStringView controlKindToString(ControlKind ckind);
        static ControlKind controlKindFromString(QAnyStringView string);

        static QAnyStringView displaySideKindToString(DisplaySideKind kind);
        static DisplaySideKind stringToDisplaySideKind(QAnyStringView string);

        static QAnyStringView displayLengthKindToString(DisplayLengthKind kind);
        static DisplayLengthKind stringToDisplayLengthKind(QAnyStringView string);

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

        QAnyStringView version() const;
        void setVersion(QAnyStringView version);

        QAnyStringView valueStr() const;
        void setValueStr(QAnyStringView value);

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

        bool versionCompare(QAnyStringView version);

        CheckResult isSatisfied(SystemInfo *sysInfo,
                                Pool *pool,
                                QString *message);

        QAnyStringView lastError() const;

    private:
        QSharedDataPointer<RelationData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Relation& relation);
