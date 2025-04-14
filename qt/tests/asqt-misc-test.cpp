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

class MiscTest : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void testMarkup();
};

using namespace AppStream;

void MiscTest::testMarkup()
{
    auto result = Utils::markupConvert(
        QStringLiteral("<p>Test!</p><p>Blah.</p><ul><li>A</li><li>B</li></ul><p>End.</p>"),
        Utils::MarkupText);
    QVERIFY(result);
    QCOMPARE(result.value(), QStringLiteral("Test!\n\nBlah.\n • A\n • B\n\nEnd."));
}

QTEST_MAIN(MiscTest)

#include "asqt-misc-test.moc"
