
#include <QtTest>
#include <QObject>
#include <QTemporaryFile>
#include "database.h"
#include "testpaths.h"

class DatabaseReadTest : public QObject {
    Q_OBJECT
    private Q_SLOTS:
        void testRead01();
};

using namespace AppStream;

void DatabaseReadTest::testRead01()
{
    // first, create a new temporary cache using ascli
    QProcess *p = new QProcess(this);
    QTemporaryFile cfile;
    QVERIFY(cfile.open());

    const QStringList args = { "refresh-cache", "--force", "--datapath=" AS_SAMPLE_DATA, "--cachepath=" + cfile.fileName()};
    p->start(ASCLI_EXECUTABLE, args);
    p->waitForFinished();

    qDebug() << p->readAllStandardOutput();
    qDebug() << p->readAllStandardError();

    QCOMPARE(p->exitCode(), 0);

    // we now have a database, let's read some data

    auto *db = new Database(cfile.fileName());
    QVERIFY(db->open());

    QList<Component> cpts = db->allComponents();
    QCOMPARE(cpts.size(), 18);

    Component cpt = db->componentById("neverball.desktop");
    QVERIFY(!cpt.id().isEmpty());

    QCOMPARE(cpt.name(), QLatin1String("Neverball"));

    delete db;
}

QTEST_MAIN(DatabaseReadTest)

#include "qtcachereadtest.moc"
