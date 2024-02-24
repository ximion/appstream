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

#pragma once

#include <QSharedDataPointer>
#include <QObject>
#include "appstreamqt_export.h"

#include "component.h"

class QString;
struct _AsComponentBox;
namespace AppStream
{

class ComponentBoxData;

/**
 * A container for AppStream::Component components.
 */
class APPSTREAMQT_EXPORT ComponentBox
{
    Q_GADGET

public:
    class iterator
    {
    public:
        iterator(const iterator &it)
            : index(it.index),
              data(it.data)
        {
        }
        bool operator==(iterator other) const
        {
            return index == other.index && data == other.data;
        }
        bool operator!=(iterator other) const
        {
            return index != other.index || data != other.data;
        }
        iterator &operator++()
        {
            index++;
            return *this;
        }

        Component operator*() const;
        std::optional<Component> operator->() const
        {
            return data->indexSafe(index);
        }
        iterator &operator=(const iterator &other)
        {
            if (&other != this) {
                index = other.index;
                data = other.data;
            }
            return *this;
        }

    private:
        friend class ComponentBox;

        iterator(uint index, const ComponentBox *cb)
            : index(index),
              data(cb)
        {
        }

        uint index;
        const ComponentBox *data;
    };

    /**
     * ComponentBox::Flags:
     * FlagNone:               No flags.
     * FlagNoChecks:           Only perform the most basic verification.
     *
     * Flags for a ComponentBox.
     **/
    enum Flag {
        FlagNone = 0,
        FlagNoChecks = 1 << 0,
    };
    Q_DECLARE_FLAGS(Flags, Flag)

    ComponentBox(ComponentBox::Flags flags);
    ComponentBox(_AsComponentBox *cbox);
    ComponentBox(const ComponentBox &other);
    ~ComponentBox();

    ComponentBox &operator=(const ComponentBox &other);

    /**
     * \returns the internally stored AsComponentBox
     */
    _AsComponentBox *cPtr() const;

    /**
     * \returns the contents of this component box as list.
     */
    QList<Component> toList() const;

    iterator begin() const
    {
        return iterator(0, this);
    }
    iterator end() const
    {
        return iterator(size(), this);
    }
    iterator erase(iterator it);

    void operator+=(const ComponentBox &other);

    uint size() const;
    bool isEmpty() const;

    std::optional<Component> indexSafe(uint index) const;

    void sort();
    void sortByScore();

private:
    QSharedDataPointer<ComponentBoxData> d;
};
}
