/*
 * Copyright (C) 2016-2025 Matthias Klumpp <matthias@tenstral.net>
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

#include <QtTest>
#include <QObject>
#include <QTemporaryFile>

#include "utils.h"
#include "agreement.h"
#include "artifact.h"
#include "bundle.h"
#include "checksum.h"
#include "component.h"
#include "issue.h"
#include "launchable.h"
#include "metadata.h"
#include "reference.h"
#include "relation.h"
#include "release.h"
#include "review.h"
#include "screenshot.h"

class MiscTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testMarkup();
    void testEnumRoundtrip();
    void testFormatVersion();
    void testNewWrappers();
};

using namespace AppStream;

/**
 * Round-trip every value of an enum through its string converters.
 * This catches the Qt mirror drifting out of sync with the C enum: a newly
 * added C value that the Qt enum lacks stringifies to "unknown" or to nothing.
 *
 * Value 0 is skipped deliberately — the C library represents the "unknown"
 * member inconsistently (some converters return "unknown", others NULL), and
 * it never round-trips meaningfully.
 */
template<typename EnumT, typename ToStr, typename FromStr>
static void checkEnumRoundtrip(const char *name, int lastValue, ToStr toString, FromStr fromString)
{
    for (int i = 1; i <= lastValue; i++) {
        const auto value = static_cast<EnumT>(i);
        const QString str = toString(value);
        QVERIFY2(!str.isEmpty(),
                 qPrintable(QStringLiteral("%1 value %2 stringifies to nothing")
                                .arg(QLatin1String(name))
                                .arg(i)));
        QVERIFY2(str != QLatin1String("unknown"),
                 qPrintable(QStringLiteral("%1 value %2 stringifies to \"unknown\"")
                                .arg(QLatin1String(name))
                                .arg(i)));
        QVERIFY2(fromString(str) == value,
                 qPrintable(QStringLiteral("%1 value %2 (\"%3\") does not round-trip")
                                .arg(QLatin1String(name))
                                .arg(i)
                                .arg(str)));
    }
}

void MiscTest::testMarkup()
{
    QString errorMsg;
    auto result = Utils::markupConvert(
        QStringLiteral("<p>Test!</p><p>Blah.</p><ul><li>A</li><li>B</li></ul><p>End.</p>"),
        Utils::MarkupText,
        &errorMsg);
    QVERIFY(result);
    QCOMPARE(result.value(), QStringLiteral("Test!\n\nBlah.\n • A\n • B\n\nEnd."));
    QVERIFY(errorMsg.isEmpty());

    QCOMPARE(
        Utils::markupConvert(QStringLiteral("<p>Test!</p><ul><li>A</li><li>B</li></ul><p>End.</p>"),
                             Utils::MarkupText),
        QStringLiteral("Test!\n • A\n • B\n\nEnd."));
}

void MiscTest::testEnumRoundtrip()
{
    checkEnumRoundtrip<Component::Kind>("Component::Kind",
                                        Component::KindIconTheme,
                                        Component::kindToString,
                                        Component::stringToKind);
    checkEnumRoundtrip<Component::MergeKind>("Component::MergeKind",
                                             Component::MergeKindRemoveComponent,
                                             Component::mergeKindToString,
                                             Component::stringToMergeKind);
    checkEnumRoundtrip<Component::UrlKind>("Component::UrlKind",
                                           Component::UrlKindContribute,
                                           Component::urlKindToString,
                                           Component::stringToUrlKind);

    checkEnumRoundtrip<Bundle::Kind>("Bundle::Kind",
                                     Bundle::KindSysupdate,
                                     Bundle::kindToString,
                                     Bundle::stringToKind);
    checkEnumRoundtrip<Launchable::Kind>("Launchable::Kind",
                                         Launchable::KindUrl,
                                         Launchable::kindToString,
                                         Launchable::stringToKind);
    checkEnumRoundtrip<Relation::ItemKind>("Relation::ItemKind",
                                           Relation::ItemKindInternet,
                                           Relation::itemKindToString,
                                           Relation::stringToItemKind);
    checkEnumRoundtrip<Relation::ControlKind>("Relation::ControlKind",
                                              Relation::ControlKindTablet,
                                              Relation::controlKindToString,
                                              Relation::controlKindFromString);
    checkEnumRoundtrip<Relation::InternetKind>("Relation::InternetKind",
                                               Relation::InternetKindFirstRun,
                                               Relation::internetKindToString,
                                               Relation::stringToInternetKind);
    checkEnumRoundtrip<Screenshot::Kind>("Screenshot::Kind",
                                         Screenshot::KindExtra,
                                         Screenshot::kindToString,
                                         Screenshot::stringToKind);
    checkEnumRoundtrip<Issue::Kind>("Issue::Kind",
                                    Issue::KindGcve,
                                    Issue::kindToString,
                                    Issue::stringToKind);
    checkEnumRoundtrip<Release::Kind>("Release::Kind",
                                      Release::KindSnapshot,
                                      Release::kindToString,
                                      Release::stringToKind);
    checkEnumRoundtrip<Release::UrgencyKind>("Release::UrgencyKind",
                                             Release::UrgencyCritical,
                                             Release::urgencyKindToString,
                                             Release::stringToUrgencyKind);
    checkEnumRoundtrip<Artifact::Kind>("Artifact::Kind",
                                       Artifact::KindBinary,
                                       Artifact::kindToString,
                                       Artifact::stringToKind);
    checkEnumRoundtrip<Checksum::Kind>("Checksum::Kind",
                                       Checksum::KindBlake3,
                                       Checksum::kindToString,
                                       Checksum::stringToKind);
    checkEnumRoundtrip<Reference::Kind>("Reference::Kind",
                                        Reference::KindRegistry,
                                        Reference::kindToString,
                                        Reference::stringToKind);
    checkEnumRoundtrip<Agreement::Kind>("Agreement::Kind",
                                        Agreement::KindPrivacy,
                                        Agreement::kindToString,
                                        Agreement::stringToKind);
    // NOTE: stops at Yaml, not DesktopEntry — the C as_format_kind_{to,from}_string
    // pair does not handle "desktop-entry" in either direction.
    checkEnumRoundtrip<Metadata::FormatKind>("Metadata::FormatKind",
                                             Metadata::FormatKindYaml,
                                             Metadata::formatKindToString,
                                             Metadata::stringToFormatKind);
}

void MiscTest::testFormatVersion()
{
    // the C enum collapsed to {UNKNOWN, V1_0}; the Qt mirror must follow it,
    // otherwise a plain 1.0 document reports a bogus pre-1.0 version.
    QCOMPARE(Metadata::formatVersionToString(Metadata::FormatVersionV10), QStringLiteral("1.0"));
    QCOMPARE(Metadata::stringToFormatVersion(QStringLiteral("1.0")), Metadata::FormatVersionV10);

    Metadata metadata;
    metadata.setFormatVersion(Metadata::FormatVersionV10);
    QCOMPARE(metadata.formatVersion(), Metadata::FormatVersionV10);
}

void MiscTest::testNewWrappers()
{
    Checksum cs(Checksum::KindSha256, QStringLiteral("deadbeef"));
    QCOMPARE(cs.kind(), Checksum::KindSha256);
    QCOMPARE(cs.value(), QStringLiteral("deadbeef"));

    Artifact artifact;
    artifact.setKind(Artifact::KindBinary);
    artifact.setPlatform(QStringLiteral("x86_64-linux-gnu"));
    artifact.addLocation(QStringLiteral("https://example.com/foo.tar.xz"));
    artifact.addChecksum(cs);
    artifact.setSize(1024, Artifact::SizeKindDownload);
    QCOMPARE(artifact.kind(), Artifact::KindBinary);
    QCOMPARE(artifact.platform(), QStringLiteral("x86_64-linux-gnu"));
    QCOMPARE(artifact.locations(), QStringList{QStringLiteral("https://example.com/foo.tar.xz")});
    QCOMPARE(artifact.size(Artifact::SizeKindDownload), 1024u);
    QCOMPARE(artifact.checksums().size(), 1);
    auto foundCs = artifact.checksum(Checksum::KindSha256);
    QVERIFY(foundCs);
    QCOMPARE(foundCs->value(), QStringLiteral("deadbeef"));

    // Release is no longer read-only and can reach its artifacts
    Release release;
    release.setKind(Release::KindStable);
    release.setVersion(QStringLiteral("1.2.3"));
    release.setUrgency(Release::UrgencyHigh);
    release.addArtifact(artifact);
    QCOMPARE(release.kind(), Release::KindStable);
    QCOMPARE(release.version(), QStringLiteral("1.2.3"));
    QCOMPARE(release.urgency(), Release::UrgencyHigh);
    QCOMPARE(release.artifacts().size(), 1);

    // issues resolved by a release, with URLs derived from their identifier
    Issue cveIssue;
    cveIssue.setKind(Issue::KindCve);
    cveIssue.setId(QStringLiteral("CVE-2023-40224"));
    QCOMPARE(cveIssue.kind(), Issue::KindCve);
    QCOMPARE(cveIssue.url(), QStringLiteral("https://www.cve.org/CVERecord?id=CVE-2023-40224"));
    QCOMPARE(cveIssue.jsonUrl(),
             QStringLiteral("https://db.gcve.eu/api/vulnerability/CVE-2023-40224"));

    Issue gcveIssue;
    gcveIssue.setKind(Issue::KindGcve);
    gcveIssue.setId(QStringLiteral("GCVE-1-2025-0001"));
    QCOMPARE(gcveIssue.url(), QStringLiteral("https://db.gcve.eu/vuln/GCVE-1-2025-0001"));

    release.addIssue(cveIssue);
    release.addIssue(gcveIssue);
    QCOMPARE(release.issues().size(), 2);
    QCOMPARE(release.issues().at(1).id(), QStringLiteral("GCVE-1-2025-0001"));

    Reference reference;
    reference.setKind(Reference::KindDoi);
    reference.setValue(QStringLiteral("10.1000/182"));
    QCOMPARE(reference.kind(), Reference::KindDoi);
    QCOMPARE(reference.value(), QStringLiteral("10.1000/182"));

    Review review;
    review.setId(QStringLiteral("r1"));
    review.setSummary(QStringLiteral("Great"));
    review.setRating(80);
    review.setFlags(Review::FlagSelf);
    QCOMPARE(review.id(), QStringLiteral("r1"));
    QCOMPARE(review.rating(), 80);
    QVERIFY(review.flags().testFlag(Review::FlagSelf));

    AgreementSection section;
    section.setKind(QStringLiteral("intro"));
    section.setName(QStringLiteral("Introduction"));

    Agreement agreement;
    agreement.setKind(Agreement::KindEula);
    agreement.setVersionId(QStringLiteral("1.0"));
    agreement.addSection(section);
    QCOMPARE(agreement.kind(), Agreement::KindEula);
    QCOMPARE(agreement.sections().size(), 1);
    QCOMPARE(agreement.sections().first().name(), QStringLiteral("Introduction"));

    // Component can now reach the previously unwrapped subsystems
    Component cpt;
    cpt.setId(QStringLiteral("org.example.Test"));
    cpt.addKeyword(QStringLiteral("editor"));
    cpt.addReview(review);
    cpt.addReference(reference);
    cpt.addAgreement(agreement);
    cpt.setBranch(QStringLiteral("stable"));
    cpt.setPriority(5);
    QCOMPARE(cpt.keywords(), QStringList{QStringLiteral("editor")});
    QCOMPARE(cpt.reviews().size(), 1);
    QCOMPARE(cpt.references().size(), 1);
    QCOMPARE(cpt.agreements().size(), 1);
    QCOMPARE(cpt.branch(), QStringLiteral("stable"));
    QCOMPARE(cpt.priority(), 5);
    QVERIFY(cpt.agreementByKind(Agreement::KindEula));

    // tag removal reports whether anything was actually removed
    QVERIFY(cpt.addTag(QStringLiteral("ns"), QStringLiteral("tag")));
    QVERIFY(cpt.hasTag(QStringLiteral("ns"), QStringLiteral("tag")));
    QVERIFY(cpt.removeTag(QStringLiteral("ns"), QStringLiteral("tag")));
    QVERIFY(!cpt.removeTag(QStringLiteral("ns"), QStringLiteral("tag")));

    // Category is writable now
    Category category;
    category.setId(QStringLiteral("Office"));
    category.setName(QStringLiteral("Office"));
    category.addComponent(cpt);
    QCOMPARE(category.id(), QStringLiteral("Office"));
    QVERIFY(category.hasComponent(cpt));
}

QTEST_MAIN(MiscTest)

#include "asqt-misc-test.moc"
