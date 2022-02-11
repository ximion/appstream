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

#include "appstream.h"
#include "relation.h"

#include <QDebug>
#include "chelpers.h"

using namespace AppStream;

class AppStream::RelationData : public QSharedData {
public:
    RelationData()
    {
        m_relation = as_relation_new();
    }

    RelationData(AsRelation* cat) : m_relation(cat)
    {
        g_object_ref(m_relation);
    }

    ~RelationData()
    {
        g_object_unref(m_relation);
    }

    bool operator==(const RelationData& rd) const
    {
        return rd.m_relation == m_relation;
    }

    AsRelation *relation() const
    {
        return m_relation;
    }

    QString lastError;
    AsRelation* m_relation;
};


QString Relation::kindToString(Relation::Kind kind)
{
    return QString::fromUtf8(as_relation_kind_to_string(static_cast<AsRelationKind>(kind)));
}

Relation::Kind Relation::stringToKind(const QString &string)
{
    return static_cast<Kind>(as_relation_kind_from_string(qPrintable(string)));
}

QString Relation::itemKindToString(Relation::ItemKind ikind)
{
    return QString::fromUtf8(as_relation_item_kind_to_string(static_cast<AsRelationItemKind>(ikind)));
}

Relation::ItemKind Relation::stringToItemKind(const QString &string)
{
    return static_cast<ItemKind>(as_relation_item_kind_from_string(qPrintable(string)));
}

Relation::Compare Relation::stringToCompare(const QString &string)
{
    return static_cast<Compare>(as_relation_compare_from_string(qPrintable(string)));
}

QString Relation::compareToString(Relation::Compare cmp)
{
    return QString::fromUtf8(as_relation_compare_to_string(static_cast<AsRelationCompare>(cmp)));
}

QString Relation::compareToSymbolsString(Relation::Compare cmp)
{
    return QString::fromUtf8(as_relation_compare_to_symbols_string(static_cast<AsRelationCompare>(cmp)));
}

QString Relation::controlKindToString(Relation::ControlKind ckind)
{
    return QString::fromUtf8(as_control_kind_to_string(static_cast<AsControlKind>(ckind)));
}

Relation::ControlKind Relation::controlKindFromString(const QString &string)
{
    return static_cast<ControlKind>(as_control_kind_from_string(qPrintable(string)));
}

QString Relation::displaySideKindToString(Relation::DisplaySideKind kind)
{
    return QString::fromUtf8(as_display_side_kind_to_string(static_cast<AsDisplaySideKind>(kind)));
}

Relation::DisplaySideKind Relation::stringToDisplaySideKind(const QString &string)
{
    return static_cast<DisplaySideKind>(as_display_side_kind_from_string(qPrintable(string)));
}

QString Relation::displayLengthKindToString(Relation::DisplayLengthKind kind)
{
    return QString::fromUtf8(as_display_length_kind_to_string(static_cast<AsDisplayLengthKind>(kind)));
}

Relation::DisplayLengthKind Relation::stringToDisplayLengthKind(const QString &string)
{
    return static_cast<DisplayLengthKind>(as_display_length_kind_from_string(qPrintable(string)));
}

Relation::Relation()
    : d(new RelationData)
{}

Relation::Relation(_AsRelation* relation)
    : d(new RelationData(relation))
{}

Relation::Relation(const Relation &relation) = default;

Relation::~Relation() = default;

Relation& Relation::operator=(const Relation &relation) = default;

bool Relation::operator==(const Relation &other) const
{
    if(this->d == other.d) {
        return true;
    }
    if(this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsRelation* AppStream::Relation::asRelation() const
{
    return d->relation();
}

QDebug operator<<(QDebug s, const AppStream::Relation& relation)
{
    s.nospace() << "AppStream::Relation("
                << Relation::kindToString(relation.kind()) << ":"
                << Relation::itemKindToString(relation.itemKind()) << ":"
                << relation.valueStr() << ")";
    return s.space();
}

Relation::Kind Relation::kind() const
{
    return static_cast<Kind>(as_relation_get_kind (d->relation()));
}

void Relation::setKind(Relation::Kind kind)
{
    as_relation_set_kind(d->relation(), static_cast<AsRelationKind>(kind));
}

Relation::ItemKind Relation::itemKind() const
{
    return static_cast<ItemKind>(as_relation_get_item_kind (d->relation()));
}

void Relation::setItemKind(Relation::ItemKind kind)
{
    as_relation_set_item_kind(d->relation(), static_cast<AsRelationItemKind>(kind));
}

Relation::Compare Relation::compare() const
{
    return static_cast<Compare>(as_relation_get_compare (d->relation()));
}

void Relation::setCompare(Relation::Compare compare)
{
    as_relation_set_compare(d->relation(), static_cast<AsRelationCompare>(compare));
}

QString Relation::version() const
{
    return valueWrap(as_relation_get_version(d->relation()));
}

void Relation::setVersion(const QString &version)
{
    as_relation_set_version(d->relation(), qPrintable(version));
}

QString Relation::valueStr() const
{
    return valueWrap(as_relation_get_value_str(d->relation()));
}

void Relation::setValueStr(const QString &value)
{
    as_relation_set_value_str(d->relation(), qPrintable(value));
}

int Relation::valueInt() const
{
    return as_relation_get_value_int(d->relation());
}

void Relation::setValueInt(int value)
{
    as_relation_set_value_int(d->relation(), value);
}

Relation::ControlKind Relation::valueControlKind() const
{
    return static_cast<ControlKind>(as_relation_get_value_control_kind(d->relation()));
}

void Relation::setValueControlKind(Relation::ControlKind kind)
{
    as_relation_set_value_control_kind(d->relation(), static_cast<AsControlKind>(kind));
}

Relation::DisplaySideKind Relation::displaySideKind() const
{
    return static_cast<DisplaySideKind>(as_relation_get_display_side_kind(d->relation()));
}

void Relation::setDisplaySideKind(Relation::DisplaySideKind kind)
{
    as_relation_set_display_side_kind(d->relation(), static_cast<AsDisplaySideKind>(kind));
}

int Relation::valuePx() const
{
    return as_relation_get_value_px(d->relation());
}

void Relation::setValuePx(int logicalPx)
{
    as_relation_set_value_px(d->relation(), logicalPx);
}

Relation::DisplayLengthKind Relation::valueDisplayLengthKind() const
{
    return static_cast<DisplayLengthKind>(as_relation_get_value_display_length_kind(d->relation()));
}

void Relation::setValueDisplayLengthKind(Relation::DisplayLengthKind kind)
{
    as_relation_set_value_display_length_kind(d->relation(), static_cast<AsDisplayLengthKind>(kind));
}

bool Relation::versionCompare(const QString &version)
{
    g_autoptr(GError) error = NULL;
    bool ret = as_relation_version_compare(d->relation(), qPrintable(version), &error);
    if (!ret && error)
        d->lastError = QString::fromUtf8(error->message);
    return ret;
}

QString Relation::lastError() const
{
    return d->lastError;
}
