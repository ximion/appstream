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

#include <QtTest>
#include <QObject>
#include <QTemporaryFile>
#include "pool.h"
#include "testpaths.h"

class PoolReadTest : public QObject {
    Q_OBJECT
    private Q_SLOTS:
        void testRead01();
};

using namespace AppStream;

void PoolReadTest::testRead01()
{
    // set up the data pool to read our sample data, without localization
    auto pool = new Pool();

    pool->clearMetadataLocations();
    pool->addMetadataLocation(AS_SAMPLE_DATA_PATH);
    pool->setLocale("C");

    // don't load system metainfo/desktop files
    auto flags = pool->flags();
    flags &= ~Pool::FlagReadDesktopFiles;
    pool->setFlags(flags);

    // don't use caches
    pool->setCacheFlags(Pool::CacheFlagNone);

    // read metadata
    QVERIFY(pool->load());

    auto cpts = pool->components();
    QCOMPARE(cpts.size(), 18);

    cpts = pool->componentsById("org.neverball.Neverball");
    QCOMPARE(cpts.size(), 1);

    auto cpt = cpts[0];
    QVERIFY(!cpt.id().isEmpty());

    QCOMPARE(cpt.name(), QLatin1String("Neverball"));

    delete pool;
}

QTEST_MAIN(PoolReadTest)

#include "asqt-pool-test.moc"
