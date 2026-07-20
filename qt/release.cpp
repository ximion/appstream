/*
 * Copyright (C) 2016 Aleix Pol Gonzalez <aleixpol@kde.org>
 * Copyright (C) 2018-2024 Matthias Klumpp <matthias@tenstral.net>
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

#include "release.h"

#include <QDateTime>
#include <QDebug>
#include <QTimeZone>
#include <QUrl>
#include "chelpers.h"

using namespace AppStream;

static_assert(static_cast<int>(Release::KindSnapshot) + 1 == AS_RELEASE_KIND_LAST,
              "Release::Kind is out of sync with AsReleaseKind");
static_assert(static_cast<int>(Release::UrgencyCritical) + 1 == AS_URGENCY_KIND_LAST,
              "Release::UrgencyKind is out of sync with AsUrgencyKind");
static_assert(static_cast<int>(Release::UrlKindDetails) + 1 == AS_RELEASE_URL_KIND_LAST,
              "Release::UrlKind is out of sync with AsReleaseUrlKind");

class AppStream::ReleaseData : public QSharedData
{
public:
    ReleaseData()
    {
        m_release = as_release_new();
    }

    ReleaseData(AsRelease *rel)
        : m_release(rel)
    {
        g_object_ref(m_release);
    }

    ~ReleaseData()
    {
        g_object_unref(m_release);
    }

    bool operator==(const ReleaseData &rd) const
    {
        return rd.m_release == m_release;
    }

    AsRelease *release() const
    {
        return m_release;
    }

    AsRelease *m_release;
};

Release::Kind Release::stringToKind(const QString &kindString)
{
    return static_cast<Release::Kind>(as_release_kind_from_string(qPrintable(kindString)));
}

QString Release::kindToString(Release::Kind kind)
{
    return valueWrap(as_release_kind_to_string(static_cast<AsReleaseKind>(kind)));
}

Release::UrgencyKind Release::stringToUrgencyKind(const QString &urgencyString)
{
    return static_cast<Release::UrgencyKind>(
        as_urgency_kind_from_string(qPrintable(urgencyString)));
}

QString Release::urgencyKindToString(Release::UrgencyKind urgency)
{
    return valueWrap(as_urgency_kind_to_string(static_cast<AsUrgencyKind>(urgency)));
}

Release::UrlKind Release::stringToUrlKind(const QString &kindString)
{
    return static_cast<Release::UrlKind>(as_release_url_kind_from_string(qPrintable(kindString)));
}

QString Release::urlKindToString(Release::UrlKind kind)
{
    return valueWrap(as_release_url_kind_to_string(static_cast<AsReleaseUrlKind>(kind)));
}

Release::Release()
    : d(new ReleaseData)
{
}

Release::Release(_AsRelease *release)
    : d(new ReleaseData(release))
{
}

Release::Release(const Release &release) = default;

Release::~Release() = default;

Release &Release::operator=(const Release &release) = default;

bool Release::operator==(const Release &other) const
{
    if (this->d == other.d) {
        return true;
    }
    if (this->d && other.d) {
        return *(this->d) == *other.d;
    }
    return false;
}

_AsRelease *AppStream::Release::cPtr() const
{
    return d->release();
}

Release::Kind Release::kind() const
{
    return Release::Kind(as_release_get_kind(d->m_release));
}

void Release::setKind(Release::Kind kind)
{
    as_release_set_kind(d->m_release, static_cast<AsReleaseKind>(kind));
}

QString Release::version() const
{
    return QString::fromUtf8(as_release_get_version(d->m_release));
}

void Release::setVersion(const QString &version)
{
    as_release_set_version(d->m_release, qPrintable(version));
}

QDateTime Release::timestamp() const
{
    const guint64 timestamp = as_release_get_timestamp(d->m_release);
    return timestamp > 0 ? QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::utc()) : QDateTime();
}

void Release::setTimestamp(const QDateTime &timestamp)
{
    as_release_set_timestamp(d->m_release, timestamp.isValid() ? timestamp.toSecsSinceEpoch() : 0);
}

QDateTime Release::timestampEol() const
{
    const guint64 timestamp = as_release_get_timestamp_eol(d->m_release);
    return timestamp > 0 ? QDateTime::fromSecsSinceEpoch(timestamp, QTimeZone::utc()) : QDateTime();
}

void Release::setTimestampEol(const QDateTime &timestamp)
{
    as_release_set_timestamp_eol(d->m_release,
                                 timestamp.isValid() ? timestamp.toSecsSinceEpoch() : 0);
}

QString Release::description() const
{
    return QString::fromUtf8(as_release_get_description(d->m_release));
}

void Release::setDescription(const QString &description, const QString &lang)
{
    as_release_set_description(d->m_release,
                               qPrintable(description),
                               lang.isEmpty() ? NULL : qPrintable(lang));
}

Release::UrgencyKind Release::urgency() const
{
    return Release::UrgencyKind(as_release_get_urgency(d->m_release));
}

void Release::setUrgency(Release::UrgencyKind urgency)
{
    as_release_set_urgency(d->m_release, static_cast<AsUrgencyKind>(urgency));
}

QList<Artifact> Release::artifacts() const
{
    QList<Artifact> res;

    auto artifacts = as_release_get_artifacts(d->m_release);
    res.reserve(artifacts->len);
    for (uint i = 0; i < artifacts->len; i++) {
        auto artifact = AS_ARTIFACT(g_ptr_array_index(artifacts, i));
        res.append(Artifact(artifact));
    }
    return res;
}

QList<Issue> Release::issues() const
{
    GPtrArray *issues = as_release_get_issues(d->m_release);
    QList<Issue> res;

    res.reserve(issues->len);
    for (uint i = 0; i < issues->len; i++) {
        auto issue = AS_ISSUE(g_ptr_array_index(issues, i));
        res.append(Issue(issue));
    }
    return res;
}

void Release::addIssue(const AppStream::Issue &issue)
{
    as_release_add_issue(d->m_release, issue.cPtr());
}

void Release::addArtifact(const AppStream::Artifact &artifact)
{
    as_release_add_artifact(d->m_release, artifact.cPtr());
}

QString Release::url(Release::UrlKind kind) const
{
    return valueWrap(as_release_get_url(d->m_release, static_cast<AsReleaseUrlKind>(kind)));
}

void Release::setUrl(Release::UrlKind kind, const QString &url)
{
    as_release_set_url(d->m_release, static_cast<AsReleaseUrlKind>(kind), qPrintable(url));
}

int Release::vercmp(const Release &other) const
{
    return as_release_vercmp(d->m_release, other.cPtr());
}

bool Release::hasTag(const QString &ns, const QString &tag) const
{
    return as_release_has_tag(d->m_release, qPrintable(ns), qPrintable(tag));
}

bool Release::addTag(const QString &ns, const QString &tag)
{
    return as_release_add_tag(d->m_release, qPrintable(ns), qPrintable(tag));
}

bool Release::removeTag(const QString &ns, const QString &tag)
{
    return as_release_remove_tag(d->m_release, qPrintable(ns), qPrintable(tag));
}

void Release::clearTags()
{
    as_release_clear_tags(d->m_release);
}

QDebug operator<<(QDebug s, const AppStream::Release &release)
{
    s.nospace() << "AppStream::Release(" << release.version() << ": " << release.description()
                << ")";
    return s.space();
}
