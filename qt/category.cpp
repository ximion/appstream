/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
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

using namespace AppStream;

class AppStream::CategoryData : public QSharedData {
public:
    CategoryData(AsCategory* cat) : m_category(cat)
    {
        g_object_ref(m_category);
    }

    ~CategoryData()
    {
        g_object_unref(m_category);
    }

    bool operator==(const CategoryData& rd) const
    {
        return rd.m_category == m_category;
    }

    AsCategory *category() const
    {
        return m_category;
    }

    AsCategory* m_category;
};

Category::Category(_AsCategory* category)
    : d(new CategoryData(category))
{}

Category::Category(const Category &category) = default;

Category::~Category() = default;

Category& Category::operator=(const Category &category) = default;

bool Category::operator==(const Category &other) const
{
    if(this->d == other.d) {
        return true;
    }
    if(this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsCategory * AppStream::Category::asCategory() const
{
    return d->category();
}

QString Category::id() const
{
    return valueWrap(as_category_get_id(d->m_category));
}

QString Category::name() const
{
    return valueWrap(as_category_get_name(d->m_category));
}

QString Category::summary() const
{
    return valueWrap(as_category_get_summary(d->m_category));
}

QString Category::icon() const
{
    return valueWrap(as_category_get_icon(d->m_category));
}

QList<Category> Category::children() const
{
    auto children = as_category_get_children(d->m_category);
    QList<Category> ret;
    ret.reserve(children->len);
    for(uint i = 0; i < children->len; i++) {
        auto ccat = AS_CATEGORY (g_ptr_array_index (children, i));
        ret << Category(ccat);
    }
    return ret;
}

QStringList Category::desktopGroups() const
{
    auto dgs = as_category_get_desktop_groups(d->m_category);
    QStringList ret;
    ret.reserve(dgs->len);
    for(uint i = 0; i < dgs->len; i++) {
        auto dg = (const gchar*) g_ptr_array_index (dgs, i);
        ret << valueWrap(dg);
    }
    return ret;
}

QDebug operator<<(QDebug s, const AppStream::Category& category)
{
    s.nospace() << "AppStream::Category(" << category.id() << ")";
    return s.space();
}

QList<Category> getDefaultCategories(bool withSpecial)
{
    auto cats = as_get_default_categories(withSpecial);
    QList<Category> ret;
    ret.reserve(cats->len);
    for(uint i = 0; i < cats->len; i++) {
        auto cat = AS_CATEGORY (g_ptr_array_index (cats, i));
        ret << Category(cat);
    }
    return ret;
}
