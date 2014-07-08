/*
 * Part of Appstream, a library for accessing AppStream on-disk database
 * Copyright 2014  Sune Vuorela <sune@vuorela.dk>
 *
 * Based upon database-read.hpp
 * Copyright (C) 2012-2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 */

#define QT_NO_KEYWORDS

#include "database.h"
#include "screenshotxmlparser_p.h"
#include <xapian.h>
#include <QStringList>
#include <QUrl>

using namespace Appstream;

namespace XapianValues {
    //the xapian values are taken from the appstream library
enum XapianValues {
    TYPE = 140,
    IDENTIFIER = 141,
    CPTNAME = 142,
    CPTNAME_UNTRANSLATED = 143,
    PKGNAME = 144,

    SUMMARY = 145,
    DESCRIPTION = 146,
    CATEGORIES = 147,

    ICON = 148,
    ICON_URL = 149,

    PROVIDED_ITEMS = 150,
    SCREENSHOT_DATA = 152, // screenshot definitions, as XML
    RELEASES_DATA = 153, // releases definitions, as XML

    LICENSE = 154,
    URLS = 155,

    PROJECT_GROUP = 160,

    COMPULSORY_FOR = 170,

    GETTEXT_DOMAIN = 180,
    ARCHIVE_SECTION = 181,
    ARCHIVE_CHANNEL = 182
};
} // namespace


class Appstream::DatabasePrivate {
    public:
        DatabasePrivate(const QString& dbpath) : m_dbPath(dbpath) {
        }
        QString m_dbPath;
        QString m_errorString;
        Xapian::Database m_db;
        bool open() {
            try {
                m_db = Xapian::Database (m_dbPath.trimmed().toStdString());
            } catch (const Xapian::Error &error) {
                m_errorString = QString::fromStdString (error.get_msg());
                return false;
            }
            return true;
        }
        ~DatabasePrivate() {
            m_db.close();
        }
};

Database::Database(const QString& dbPath) : d(new DatabasePrivate(dbPath)) {

}

bool Database::open() {
    return d->open();
}

QString Database::errorString() const {
    return d->m_errorString;
}

Database::~Database() {
    // empty. needed for the scoped pointer for the private pointer
}

QString value(Xapian::Document document, XapianValues::XapianValues index) {
    return QString::fromStdString(document.get_value(index));
}

Component xapianDocToComponent(Xapian::Document document) {
    Component component;

    // kind
    QString kindString = value (document, XapianValues::TYPE);
    component.setKind(Component::stringToKind(kindString));

    // Identifier
    QString id = value(document, XapianValues::IDENTIFIER);
    component.setId(id);

    // Component name
    QString name = value(document,XapianValues::CPTNAME);
    component.setName(name);

    // Package name
    QString packageName = value(document,XapianValues::PKGNAME);
    component.setPackageName(packageName);

    // URLs
    QString concatUrlStrings = value(document, XapianValues::URLS);
    QStringList urlStrings= concatUrlStrings.split('\n',QString::SkipEmptyParts);
    if(urlStrings.size() %2 == 0) {
        QMultiHash<Component::UrlKind, QUrl> urls;
        for(int i = 0; i < urlStrings.size(); i=i+2) {
            Component::UrlKind ukind = Component::stringToUrlKind(urlStrings.at(i));
            QUrl url = QUrl::fromUserInput(urlStrings.at(i+1));
            urls.insertMulti(ukind, url);
        }
        component.setUrls(urls);
    } else {
        qWarning("Bad url strings for package: %s %s", qPrintable(packageName), qPrintable(concatUrlStrings));
    }

    QString concatProvides = value(document, XapianValues::PROVIDED_ITEMS);
    QStringList providesList = concatProvides.split('\n',QString::SkipEmptyParts);
    QList<Provides> provideslist;
    Q_FOREACH(const QString& string, providesList) {
        QStringList providesParts = string.split(';',QString::SkipEmptyParts);
        if(providesParts.size() < 2) {
            qWarning("Bad component parts for package %s %s",qPrintable(packageName), qPrintable(string));
            continue;
        }
        QString kindString = providesParts.takeFirst();
        Provides::Kind kind = Provides::stringToKind(kindString);
        Provides provides;
        provides.setKind(kind);
        QString value = providesParts.takeFirst();
        provides.setValue(value);
        QString extraData = providesParts.join(";");
        provides.setExtraData(extraData);
        provideslist << provides;
    }
    component.setProvides(provideslist);

    // Application icon
    QString icon = value(document,XapianValues::ICON);
    component.setIcon(icon);

    QUrl iconUrl = QUrl::fromUserInput(value(document,XapianValues::ICON_URL));
    component.setIconUrl(iconUrl);

    // Summary
    QString summary = value(document,XapianValues::SUMMARY);
    component.setSummary(summary);

    // Long description
    QString description = value(document,XapianValues::DESCRIPTION);
    component.setDescription(description);

    // Categories
    QStringList categories = value(document, XapianValues::CATEGORIES).split(";");
    component.setCategories(categories);

    // Screenshot data
    QString screenshotXml = value(document,XapianValues::SCREENSHOT_DATA);
    QXmlStreamReader reader(screenshotXml);
    QList<Appstream::ScreenShot> screenshots = parseScreenShotsXml(&reader);
    component.setScreenShots(screenshots);

    // Compulsory-for-desktop information
    QStringList compulsory = value(document, XapianValues::COMPULSORY_FOR).split(";");
    component.setCompulsoryForDesktops(compulsory);

    // License
    QString license = value(document,XapianValues::LICENSE);
    component.setProjectLicense(license);

    // Project group
    QString projectGroup = value(document,XapianValues::PROJECT_GROUP);
    component.setProjectGroup(projectGroup);

    // Releases data
    QString releasesXml = value(document,XapianValues::RELEASES_DATA);
    Q_UNUSED(releasesXml);

    return component;
}

QList<Component> parseSearchResults(Xapian::MSet matches) {
    QList<Component> components;
    for (Xapian::MSetIterator it = matches.begin(); it != matches.end(); ++it) {
        Xapian::Document document = it.get_document ();
        components << xapianDocToComponent(document);
    }
    return components;
}

QList< Component > Database::allComponents() const {
    QList<Component> components;

    // Iterate through all Xapian documents
    Xapian::PostingIterator it = d->m_db.postlist_begin (std::string());
    for (Xapian::PostingIterator it = d->m_db.postlist_begin(std::string());it != d->m_db.postlist_end(std::string()); ++it) {
        Xapian::Document doc = d->m_db.get_document (*it);
        Component component = xapianDocToComponent (doc);
        components << component;
    }

    return components;
}

Component Database::componentById(const QString& id) const {
    Xapian::Query id_query = Xapian::Query (Xapian::Query::OP_OR,
                                            Xapian::Query("AI" + id.trimmed().toStdString()),
                                            Xapian::Query ());
    id_query.serialise ();

    Xapian::Enquire enquire = Xapian::Enquire (d->m_db);
    enquire.set_query (id_query);

    Xapian::MSet matches = enquire.get_mset (0, d->m_db.get_doccount ());
    if (matches.size () > 1) {
        qWarning ("Found more than one component with id '%s'! Returning the first one.", qPrintable(id));
        Q_ASSERT(false);
    }
    if (matches.empty()) {
        return Component();
    }

    Xapian::Document document = matches[matches.get_firstitem ()].get_document ();

    return xapianDocToComponent(document);
}

QList< Component > Database::componentsByKind(Component::Kind kind) const {
    Xapian::Query item_query;
    item_query = Xapian::Query ("AT" + Component::kindToString(kind).toStdString());

    item_query.serialise ();

    Xapian::Enquire enquire = Xapian::Enquire (d->m_db);
    enquire.set_query (item_query);
    Xapian::MSet matches = enquire.get_mset (0, d->m_db.get_doccount ());
    return parseSearchResults(matches);
}

Xapian::QueryParser newAppStreamParser (Xapian::Database db) {
    Xapian::QueryParser xapian_parser = Xapian::QueryParser ();
    xapian_parser.set_database (db);
    xapian_parser.add_boolean_prefix ("pkg", "XP");
    xapian_parser.add_boolean_prefix ("pkg", "AP");
    xapian_parser.add_boolean_prefix ("mime", "AM");
    xapian_parser.add_boolean_prefix ("section", "XS");
    xapian_parser.add_boolean_prefix ("origin", "XOC");
    xapian_parser.add_prefix ("pkg_wildcard", "XP");
    xapian_parser.add_prefix ("pkg_wildcard", "AP");
    xapian_parser.set_default_op (Xapian::Query::OP_AND);
    return xapian_parser;
}

typedef QPair<Xapian::Query, Xapian::Query> QueryPair;

QueryPair buildQueries(QString searchTerm, const QStringList& categories, Xapian::Database db) {
    // empty query returns a query that matches nothing (for performance
    // reasons)
    if (searchTerm.isEmpty() && categories.isEmpty()) {
        return QueryPair();
    }

    // generate category query
    Xapian::Query categoryQuery = Xapian::Query ();
    Q_FOREACH(const QString& category, categories) {
        categoryQuery = Xapian::Query(Xapian::Query::OP_OR,
                                      categoryQuery,
                                      Xapian::Query(category.trimmed().toLower().toStdString()));
    }

        // we cheat and return a match-all query for single letter searches
    if (searchTerm.size() < 2) {
        Xapian::Query allQuery = Xapian::Query(Xapian::Query::OP_OR,Xapian::Query (""), categoryQuery);
        return QueryPair(allQuery,allQuery);
    }

    // get a pkg query
    Xapian::Query pkgQuery = Xapian::Query ();

    // try split on one magic char
    if(searchTerm.contains(',')) {
        QStringList parts = searchTerm.split(',');
        Q_FOREACH(const QString& part, parts) {
            pkgQuery = Xapian::Query (Xapian::Query::OP_OR,
                                   pkgQuery,
                                   Xapian::Query ("XP" + part.trimmed().toStdString()));
            pkgQuery = Xapian::Query (Xapian::Query::OP_OR,
                                   pkgQuery,
                                   Xapian::Query ("AP" + part.trimmed().toStdString()));
        }
    } else {
        // try another
        QStringList parts = searchTerm.split('\n');
        Q_FOREACH(const QString& part, parts) {
            pkgQuery = Xapian::Query (Xapian::Query::OP_OR,
                                       Xapian::Query("XP" + part.trimmed().toStdString()),
                                       pkgQuery);
        }
    }
    if(!categoryQuery.empty()) {
        pkgQuery = Xapian::Query(Xapian::Query::OP_AND,pkgQuery, categoryQuery);
    }

    // get a search query
    if (!searchTerm.contains (':')) {  // ie, not a mimetype query
        // we need this to work around xapian oddness
        searchTerm = searchTerm.replace('-','_');
    }

    Xapian::QueryParser parser = newAppStreamParser (db);
    Xapian::Query fuzzyQuery = parser.parse_query (searchTerm.trimmed().toStdString(),
                                                    Xapian::QueryParser::FLAG_PARTIAL |
                                                    Xapian::QueryParser::FLAG_BOOLEAN);
    // if the query size goes out of hand, omit the FLAG_PARTIAL
    // (LP: #634449)
    if (fuzzyQuery.get_length () > 1000) {
        fuzzyQuery = parser.parse_query(searchTerm.trimmed().toStdString(),
                                         Xapian::QueryParser::FLAG_BOOLEAN);
    }

    // now add categories
    if(!categoryQuery.empty()) {
        fuzzyQuery = Xapian::Query(Xapian::Query::OP_AND,fuzzyQuery, categoryQuery);
    }

    return QueryPair(pkgQuery, fuzzyQuery);
}

QList< Component > Database::findComponentsByString(QString searchTerm, QStringList categories) {
    QPair<Xapian::Query, Xapian::Query> queryPair = buildQueries(searchTerm.trimmed(), categories, d->m_db);

    // "normal" query
    Xapian::Query query = queryPair.first;
    query.serialise ();

    Xapian::Enquire enquire = Xapian::Enquire (d->m_db);
    enquire.set_query (query);
    QList<Component> result = parseSearchResults (enquire.get_mset(0,d->m_db.get_doccount()));

    // do fuzzy query if we got no results
    if (result.isEmpty()) {
        query = queryPair.second;
        query.serialise ();

        enquire = Xapian::Enquire (d->m_db);
        enquire.set_query (query);
        result = parseSearchResults(enquire.get_mset(0,d->m_db.get_doccount()));
    }

    return result;
}

#include "database.moc"
