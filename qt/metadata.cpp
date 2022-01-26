/*
 * Copyright (C) 2016 Matthias Klumpp <matthias@tenstral.net>
 * Copyright (C) 2017 Jan Grulich <jgrulich@redhat.com>
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
#include "metadata.h"
#include "chelpers.h"

#include <QDebug>

using namespace AppStream;

class AppStream::MetadataData : public QSharedData {
public:
    MetadataData()
    {
        m_metadata = as_metadata_new();
    }

    MetadataData(AsMetadata *metadata)
        : m_metadata(metadata)
    {
        g_object_ref(m_metadata);
    }

    ~MetadataData()
    {
        g_object_unref(m_metadata);
    }

    bool operator==(const MetadataData& rd) const
    {
        return rd.m_metadata == m_metadata;
    }

    AsMetadata *metadata() const
    {
        return m_metadata;
    }

    QString lastError;
    AsMetadata* m_metadata;
};

AppStream::Metadata::FormatKind AppStream::Metadata::stringToFormatKind(const QString& kindString)
{
    if (kindString == QLatin1String("xml")) {
        return FormatKind::FormatKindXml;
    } else if (kindString == QLatin1String("yaml")) {
        return FormatKind::FormatKindYaml;
    }
    return FormatKind::FormatKindUnknown;
}

QString AppStream::Metadata::formatKindToString(AppStream::Metadata::FormatKind kind)
{
    if (kind == FormatKind::FormatKindXml) {
        return QLatin1String("xml");
    } else if (kind == FormatKind::FormatKindYaml) {
        return QLatin1String("yaml");
    }
    return QLatin1String("unknown");
}

AppStream::Metadata::FormatVersion AppStream::Metadata::stringToFormatVersion(const QString& formatVersionString)
{
    return static_cast<Metadata::FormatVersion>(as_format_version_from_string(qPrintable(formatVersionString)));
}

QString AppStream::Metadata::formatVersionToString(AppStream::Metadata::FormatVersion version)
{
    return QString::fromUtf8(as_format_version_to_string(static_cast<AsFormatVersion>(version)));
}

Metadata::Metadata()
    : d(new MetadataData())
{}

Metadata::Metadata(_AsMetadata* metadata)
    : d(new MetadataData(metadata))
{}

Metadata::Metadata(const Metadata &metadata) = default;

Metadata::~Metadata() = default;

Metadata& Metadata::operator=(const Metadata &metadata) = default;

bool Metadata::operator==(const Metadata &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsMetadata * AppStream::Metadata::asMetadata() const
{
    return d->metadata();
}

AppStream::Metadata::MetadataError AppStream::Metadata::parseFile(const QString& file, AppStream::Metadata::FormatKind format)
{
    g_autoptr(GError) error = nullptr;
    g_autoptr(GFile) gFile = g_file_new_for_path(qPrintable(file));
    as_metadata_parse_file(d->m_metadata, gFile, (AsFormatKind) format, &error);

    if (error != nullptr) {
        d->lastError = QString::fromUtf8(error->message);
        if (error->domain == AS_METADATA_ERROR)
            return static_cast<AppStream::Metadata::MetadataError>(error->code);
        else
            return AppStream::Metadata::MetadataErrorFailed;
    }

    return AppStream::Metadata::MetadataErrorNoError;
}

AppStream::Metadata::MetadataError AppStream::Metadata::parse(const QString& data, AppStream::Metadata::FormatKind format)
{
    g_autoptr(GError) error = nullptr;
    as_metadata_parse(d->m_metadata, qPrintable(data), (AsFormatKind) format, &error);

    if (error != nullptr) {
        d->lastError = QString::fromUtf8(error->message);
        if (error->domain == AS_METADATA_ERROR)
            return static_cast<AppStream::Metadata::MetadataError>(error->code);
        else
            return AppStream::Metadata::MetadataErrorFailed;
    }

    return AppStream::Metadata::MetadataErrorNoError;
}

AppStream::Metadata::MetadataError AppStream::Metadata::parseDesktopData(const QString& data, const QString& cid)
{
    g_autoptr(GError) error = nullptr;
    as_metadata_parse_desktop_data(d->m_metadata, qPrintable(data), qPrintable(cid), &error);

    if (error != nullptr) {
        d->lastError = QString::fromUtf8(error->message);
        if (error->domain == AS_METADATA_ERROR)
            return static_cast<AppStream::Metadata::MetadataError>(error->code);
        else
            return AppStream::Metadata::MetadataErrorFailed;
    }

    return AppStream::Metadata::MetadataErrorNoError;
}

AppStream::Component AppStream::Metadata::component() const
{
    auto component = as_metadata_get_component(d->m_metadata);
    if (component == nullptr)
        return AppStream::Component();
    return AppStream::Component(component);
}

QList<AppStream::Component> AppStream::Metadata::components() const
{
    QList<Component> res;

    auto components = as_metadata_get_components(d->m_metadata);
    res.reserve(components->len);
    for (uint i = 0; i < components->len; i++) {
        auto component = AS_COMPONENT (g_ptr_array_index (components, i));
        res.append(Component(component));
    }
    return res;
}

void AppStream::Metadata::clearComponents()
{
    as_metadata_clear_components(d->m_metadata);
}

void AppStream::Metadata::addComponent(const AppStream::Component& component)
{
    as_metadata_add_component(d->m_metadata, component.asComponent());
}

QString AppStream::Metadata::componentToMetainfo(AppStream::Metadata::FormatKind format) const
{
    return valueWrap(as_metadata_component_to_metainfo(d->m_metadata, (AsFormatKind) format, nullptr));
}

AppStream::Metadata::MetadataError AppStream::Metadata::saveMetainfo(const QString& fname, AppStream::Metadata::FormatKind format)
{
    g_autoptr(GError) error = nullptr;
    as_metadata_save_metainfo(d->m_metadata, qPrintable(fname), (AsFormatKind) format, &error);

    if (error != nullptr) {
        d->lastError = QString::fromUtf8(error->message);
        if (error->domain == AS_METADATA_ERROR)
            return static_cast<AppStream::Metadata::MetadataError>(error->code);
        else
            return AppStream::Metadata::MetadataErrorFailed;
    }

    return AppStream::Metadata::MetadataErrorNoError;
}

QString AppStream::Metadata::componentsToCollection(AppStream::Metadata::FormatKind format) const
{
    return valueWrap(as_metadata_components_to_collection(d->m_metadata, (AsFormatKind) format, nullptr));
}

AppStream::Metadata::MetadataError AppStream::Metadata::saveCollection(const QString& collection, AppStream::Metadata::FormatKind format)
{
    g_autoptr(GError) error = nullptr;
    as_metadata_save_collection(d->m_metadata, qPrintable(collection), (AsFormatKind) format, &error);

    if (error != nullptr) {
        d->lastError = QString::fromUtf8(error->message);
        if (error->domain == AS_METADATA_ERROR)
            return static_cast<AppStream::Metadata::MetadataError>(error->code);
        else
            return AppStream::Metadata::MetadataErrorFailed;
    }

    return AppStream::Metadata::MetadataErrorNoError;
}

AppStream::Metadata::FormatVersion AppStream::Metadata::formatVersion() const
{
    return static_cast<AppStream::Metadata::FormatVersion>(as_metadata_get_format_version(d->m_metadata));
}

void AppStream::Metadata::setFormatVersion(AppStream::Metadata::FormatVersion formatVersion)
{
    as_metadata_set_format_version(d->m_metadata, (AsFormatVersion) formatVersion);
}

AppStream::Metadata::FormatStyle AppStream::Metadata::formatStyle() const
{
    return static_cast<AppStream::Metadata::FormatStyle>(as_metadata_get_format_style(d->m_metadata));
}

void AppStream::Metadata::setFormatStyle(AppStream::Metadata::FormatStyle format)
{
    as_metadata_set_format_style(d->m_metadata, (AsFormatStyle) format);
}

QString AppStream::Metadata::locale() const
{
    return valueWrap(as_metadata_get_locale(d->m_metadata));
}

void AppStream::Metadata::setLocale(const QString& locale)
{
    as_metadata_set_locale(d->m_metadata, qPrintable(locale));
}

QString AppStream::Metadata::origin() const
{
    return valueWrap(as_metadata_get_origin(d->m_metadata));
}

void AppStream::Metadata::setOrigin(const QString& origin)
{
    as_metadata_set_origin(d->m_metadata, qPrintable(origin));
}

bool AppStream::Metadata::updateExisting() const
{
    return as_metadata_get_update_existing(d->m_metadata);
}

void AppStream::Metadata::setUpdateExisting(bool updateExisting)
{
    as_metadata_set_update_existing(d->m_metadata, updateExisting);
}

bool AppStream::Metadata::writeHeader() const
{
    return as_metadata_get_write_header(d->m_metadata);
}

void AppStream::Metadata::setWriteHeader(bool writeHeader)
{
    as_metadata_set_write_header(d->m_metadata, writeHeader);
}

QString AppStream::Metadata::architecture() const
{
    return valueWrap(as_metadata_get_architecture(d->m_metadata));
}

void AppStream::Metadata::setArchitecture(const QString& architecture)
{
    as_metadata_set_architecture(d->m_metadata, qPrintable(architecture));
}

QDebug operator<<(QDebug s, const AppStream::Metadata& metadata)
{
    QStringList list;
    foreach (const AppStream::Component& component, metadata.components()) {
        list << component.id();
    }
    s.nospace() << "AppStream::Metadata(" << list << ")";
    return s.space();
}
