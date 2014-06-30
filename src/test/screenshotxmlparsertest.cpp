#include <QtTest>
#include <QObject>
#include <qxmlstream.h>
#include "screenshotxmlparser_p.h"

class ScreenShotXmlParserTest : public QObject {
    Q_OBJECT
    private Q_SLOTS:
        void testData01();
};

using namespace Asmara;

void ScreenShotXmlParserTest::testData01() {

    QString data1("<?xml version=\"1.0\"?>"
                    "<screenshots>"
                        "<screenshot type=\"default\">"
                            "<image type=\"thumbnail\" width=\"624\" height=\"351\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/624x351/shotwell-992dd22536daf59226f1f7f6b939312e.png"
                            "</image>"
                            "<image type=\"thumbnail\" width=\"112\" height=\"63\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/112x63/shotwell-992dd22536daf59226f1f7f6b939312e.png"
                            "</image>"
                            "<image type=\"thumbnail\" width=\"752\" height=\"423\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/752x423/shotwell-992dd22536daf59226f1f7f6b939312e.png"
                            "</image>"
                        "</screenshot>"
                        "<screenshot>"
                            "<image type=\"thumbnail\" width=\"624\" height=\"351\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/624x351/shotwell-1bbcf4adfeedd835093747981faefc52.png"
                            "</image>"
                            "<image type=\"thumbnail\" width=\"112\" height=\"63\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/112x63/shotwell-1bbcf4adfeedd835093747981faefc52.png"
                            "</image>"
                            "<image type=\"thumbnail\" width=\"752\" height=\"423\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/752x423/shotwell-1bbcf4adfeedd835093747981faefc52.png"
                            "</image>"
                        "</screenshot>"
                        "<screenshot>"
                            "<image type=\"thumbnail\" width=\"624\" height=\"351\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/624x351/shotwell-bbb95ac685d53abc526c51190dff54f5.png"
                            "</image>"
                            "<image type=\"thumbnail\" width=\"112\" height=\"63\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/112x63/shotwell-bbb95ac685d53abc526c51190dff54f5.png"
                            "</image>"
                            "<image type=\"thumbnail\" width=\"752\" height=\"423\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/752x423/shotwell-bbb95ac685d53abc526c51190dff54f5.png"
                            "</image>"
                        "</screenshot>"
                        "<screenshot>"
                            "<image type=\"thumbnail\" width=\"624\" height=\"351\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/624x351/shotwell-6ebdf7af56dfb4679f402928d6128bc2.png"
                            "</image>"
                            "<image type=\"thumbnail\" width=\"112\" height=\"63\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/112x63/shotwell-6ebdf7af56dfb4679f402928d6128bc2.png"
                            "</image>"
                            "<image type=\"thumbnail\" width=\"752\" height=\"423\">"
                                "http://alt.fedoraproject.org/pub/alt/screenshots/f21/752x423/shotwell-6ebdf7af56dfb4679f402928d6128bc2.png"
                            "</image>"
                        "</screenshot>"
                    "</screenshots>");
    QXmlStreamReader reader(data1);
    QList<Asmara::ScreenShot> screenshots = parseScreenShotsXml(&reader);
    QCOMPARE(screenshots.length(),4);

    QVERIFY(screenshots.first().isDefault());
    int defaultCounter=0;
    Q_FOREACH(const ScreenShot& ss, screenshots) {
        QCOMPARE(ss.images().length(),3);
        if(ss.isDefault()) {
            defaultCounter++;
        }
    }
    QCOMPARE(defaultCounter,1);
    ScreenShot ss3 = screenshots.at(2);
    QVERIFY(!ss3.isDefault());
    QVERIFY(ss3.caption().isNull());
    QCOMPARE(ss3.images().length(),3);

    Image image2 = ss3.images().value(1);
    QCOMPARE(image2.width(),112);
    QCOMPARE(image2.height(),63);
    QCOMPARE(image2.kind(), Image::Thumbnail);
    QCOMPARE(image2.url(), QUrl::fromUserInput("http://alt.fedoraproject.org/pub/alt/screenshots/f21/112x63/shotwell-bbb95ac685d53abc526c51190dff54f5.png"));

}

QTEST_MAIN(ScreenShotXmlParserTest)

#include "screenshotxmlparsertest.moc"
