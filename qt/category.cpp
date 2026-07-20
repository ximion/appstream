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
#include "category.h"

#include <QDebug>
#include "chelpers.h"
#include "component.h"

using namespace AppStream;

class AppStream::CategoryData : public QSharedData
{
public:
    CategoryData()
    {
        m_category = as_category_new();
    }

    CategoryData(AsCategory *cat)
        : m_category(cat)
    {
        g_object_ref(m_category);
    }

    ~CategoryData()
    {
        g_object_unref(m_category);
    }

    bool operator==(const CategoryData &rd) const
    {
        return rd.m_category == m_category;
    }

    AsCategory *category() const
    {
        return m_category;
    }

    AsCategory *m_category;
};

Category::Category()
    : d(new CategoryData)
{
}

Category::Category(_AsCategory *category)
    : d(new CategoryData(category))
{
}

Category::Category(const Category &category) = default;

Category::~Category() = default;

Category &Category::operator=(const Category &category) = default;

bool Category::operator==(const Category &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsCategory *AppStream::Category::cPtr() const
{
    return d->category();
}

QString Category::id() const
{
    return valueWrap(as_category_get_id(d->m_category));
}

void Category::setId(const QString &id)
{
    as_category_set_id(d->m_category, qPrintable(id));
}

QString Category::name() const
{
    return valueWrap(as_category_get_name(d->m_category));
}

void Category::setName(const QString &name)
{
    as_category_set_name(d->m_category, qPrintable(name));
}

QString Category::summary() const
{
    return valueWrap(as_category_get_summary(d->m_category));
}

void Category::setSummary(const QString &summary)
{
    as_category_set_summary(d->m_category, qPrintable(summary));
}

QString Category::icon() const
{
    return valueWrap(as_category_get_icon(d->m_category));
}

void Category::setIcon(const QString &icon)
{
    as_category_set_icon(d->m_category, qPrintable(icon));
}

bool Category::hasChildren() const
{
    return as_category_has_children(d->m_category);
}

void Category::addChild(const Category &subcat)
{
    as_category_add_child(d->m_category, subcat.cPtr());
}

void Category::removeChild(const Category &subcat)
{
    as_category_remove_child(d->m_category, subcat.cPtr());
}

QList<AppStream::Component> Category::components() const
{
    auto cpts = as_category_get_components(d->m_category);
    QList<AppStream::Component> ret;
    ret.reserve(cpts->len);
    for (uint i = 0; i < cpts->len; i++) {
        auto cpt = AS_COMPONENT(g_ptr_array_index(cpts, i));
        ret << Component(cpt);
    }
    return ret;
}

void Category::addComponent(const AppStream::Component &cpt)
{
    as_category_add_component(d->m_category, cpt.cPtr());
}

bool Category::hasComponent(const AppStream::Component &cpt) const
{
    return as_category_has_component(d->m_category, cpt.cPtr());
}

QList<Category> Category::children() const
{
    auto children = as_category_get_children(d->m_category);
    QList<Category> ret;
    ret.reserve(children->len);
    for (uint i = 0; i < children->len; i++) {
        auto ccat = AS_CATEGORY(g_ptr_array_index(children, i));
        ret << Category(ccat);
    }
    return ret;
}

QStringList Category::desktopGroups() const
{
    auto dgs = as_category_get_desktop_groups(d->m_category);
    QStringList ret;
    ret.reserve(dgs->len);
    for (uint i = 0; i < dgs->len; i++) {
        auto dg = (const gchar *) g_ptr_array_index(dgs, i);
        ret << valueWrap(dg);
    }
    return ret;
}

void Category::addDesktopGroup(const QString &groupName)
{
    as_category_add_desktop_group(d->m_category, qPrintable(groupName));
}

QDebug operator<<(QDebug s, const AppStream::Category &category)
{
    s.nospace() << "AppStream::Category(" << category.id() << ")";
    return s.space();
}

QList<Category> getDefaultCategories(bool withSpecial)
{
    auto cats = as_get_default_categories(withSpecial);
    QList<Category> ret;
    ret.reserve(cats->len);
    for (uint i = 0; i < cats->len; i++) {
        auto cat = AS_CATEGORY(g_ptr_array_index(cats, i));
        ret << Category(cat);
    }
    return ret;
}
