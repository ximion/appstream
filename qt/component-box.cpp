/*
 * Copyright (C) 2019-2024 Matthias Klumpp <matthias@tenstral.net>
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
#include "component-box.h"

#include <QSharedData>
#include <QDebug>

using namespace AppStream;

class AppStream::ComponentBoxData : public QSharedData
{
public:
    ComponentBoxData(ComponentBox::Flags flags)
    {
#if (QT_VERSION >= QT_VERSION_CHECK(6, 0, 0))
        m_cbox = as_component_box_new(static_cast<AsComponentBoxFlags>(flags.toInt()));
#else
        m_cbox = as_component_box_new((AsComponentBoxFlags) int(flags));
#endif
    }

    ComponentBoxData(AsComponentBox *cbox)
        : m_cbox(cbox)
    {
        g_object_ref(m_cbox);
    }

    ~ComponentBoxData()
    {
        g_object_unref(m_cbox);
    }

    bool operator==(const ComponentBoxData &rd) const
    {
        return rd.m_cbox == m_cbox;
    }

    AsComponentBox *componentBox() const
    {
        return m_cbox;
    }

    AsComponentBox *m_cbox;
};

Component ComponentBox::iterator::operator*() const
{
    AsComponent *cpt = as_component_box_index(data->d->m_cbox, index);
    Q_ASSERT(cpt != nullptr);
    return Component(cpt);
}

ComponentBox::ComponentBox(const ComponentBox &other)
    : d(other.d)
{
}

ComponentBox::ComponentBox(ComponentBox::Flags flags)
    : d(new ComponentBoxData(flags))
{
}

ComponentBox::ComponentBox(_AsComponentBox *cbox)
    : d(new ComponentBoxData(cbox))
{
}

ComponentBox::~ComponentBox() { }

ComponentBox &ComponentBox::operator=(const ComponentBox &other)
{
    this->d = other.d;
    return *this;
}

_AsComponentBox *AppStream::ComponentBox::cPtr() const
{
    return d->componentBox();
}

QList<Component> ComponentBox::toList() const
{
    QList<Component> res;
    res.reserve(as_component_box_len(d->m_cbox));
    for (uint i = 0; i < as_component_box_len(d->m_cbox); i++) {
        Component cpt(as_component_box_index(d->m_cbox, i));
        res.append(cpt);
    }

    return res;
}

uint ComponentBox::size() const
{
    return as_component_box_get_size(d->m_cbox);
}

bool ComponentBox::isEmpty() const
{
    return as_component_box_is_empty(d->m_cbox);
}

std::optional<Component> ComponentBox::indexSafe(uint index) const
{
    std::optional<Component> result;
    AsComponent *cpt = as_component_box_index_safe(d->m_cbox, index);
    if (cpt != nullptr)
        result = Component(cpt);
    return result;
}

void ComponentBox::sort()
{
    as_component_box_sort(d->m_cbox);
}

void ComponentBox::sortByScore()
{
    as_component_box_sort_by_score(d->m_cbox);
}

void ComponentBox::operator+=(const ComponentBox &other)
{
    for (uint i = 0; i < as_component_box_len(other.d->m_cbox); i++) {
        g_autoptr(GError) error = nullptr;
        AsComponent *c = as_component_box_index(other.d->m_cbox, i);
        as_component_box_add(d->m_cbox, c, &error);
        if (error) {
            qWarning() << "error adding component" << error->message;
        }
    }
}

ComponentBox::iterator ComponentBox::erase(iterator it)
{
    as_component_box_remove_at(it.data->d->m_cbox, it.index);
    return it;
}
