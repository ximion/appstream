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

#ifndef APPSTREAMQT_METADATA_H
#define APPSTREAMQT_METADATA_H

#include <QSharedDataPointer>
#include <QString>
#include <QObject>
#include "appstreamqt_export.h"

#include "component.h"

struct _AsMetadata;
namespace AppStream {

class MetadataData;
class APPSTREAMQT_EXPORT Metadata {
    Q_GADGET
    public:
        enum FormatStyle {
            FormatStyleUnknown,
            FormatStyleMetainfo,
            FormatStyleCollection
        };
        Q_ENUM(FormatStyle)

        enum FormatKind {
            FormatKindUnknown,
            FormatKindXml,
            FormatKindYaml,
            FormatKindDesktopEntry
        };
        Q_ENUM(FormatKind)

        enum FormatVersion {
            FormatVersionV06,
            FormatVersionV07,
            FormatVersionV08,
            FormatVersionV09,
            FormatVersionV010,
            FormatVersionV011,
            FormatVersionV012,
            FormatVersionV013,
            FormatVersionV014,
        };
        Q_ENUM(FormatVersion)

        enum MetadataError {
            /*
             * Needed to identify whether parsing was successful or not
             */
            MetadataErrorNoError = -1,
            MetadataErrorFailed,
            MetadataErrorParse,
            MetadataErrorFormatUnexpected,
            MetadataErrorNoComponent,
            MetadataErrorValueMissing
        };
        Q_ENUM(MetadataError)

        static FormatKind stringToFormatKind(const QString& kindString);
        static QString formatKindToString(FormatKind format);

        static FormatVersion stringToFormatVersion(const QString& formatVersionString);
        static QString formatVersionToString(FormatVersion version);

        Metadata();
        explicit Metadata(_AsMetadata *metadata);
        Metadata(const Metadata& metadata);
        ~Metadata();

        Metadata& operator=(const Metadata& metadata);
        bool operator==(const Metadata& r) const;

        /**
         * \returns the internally stored AsMetadata
         */
        _AsMetadata *asMetadata() const;

        MetadataError parseFile(const QString& file, FormatKind format);

        MetadataError parse(const QString& data, FormatKind format);

        MetadataError parseDesktopData(const QString& data, const QString& cid);

        AppStream::Component component() const;
        QList<AppStream::Component> components() const;
        void clearComponents();
        void addComponent(const AppStream::Component& component);

        QString componentToMetainfo(FormatKind format) const;

        MetadataError saveMetainfo(const QString& fname, FormatKind format);

        QString componentsToCollection(FormatKind format) const;

        MetadataError saveCollection(const QString& collection, FormatKind format);

        FormatVersion formatVersion() const;
        void setFormatVersion(FormatVersion formatVersion);

        FormatStyle formatStyle() const;
        void setFormatStyle(FormatStyle format);

        QString locale() const;
        void setLocale(const QString& locale);

        QString origin() const;
        void setOrigin(const QString& origin);

        bool updateExisting() const;
        void setUpdateExisting(bool updateExisting);

        bool writeHeader() const;
        void setWriteHeader(bool writeHeader);

        QString architecture() const;
        void setArchitecture(const QString& architecture);

        /**
         * \return The last error message received.
         */
        QString lastError() const;

    private:
        QSharedDataPointer<MetadataData> d;
};
}

APPSTREAMQT_EXPORT QDebug operator<<(QDebug s, const AppStream::Metadata& metadata);

#endif // APPSTREAMQT_METADATA_H

