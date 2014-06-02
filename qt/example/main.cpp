#include "QApplication"
#include <QUrl>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include <appstream-qt.h>
#include "asintf.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qmlRegisterType<AsIntf>("org.asexample", 1, 0, "AsIntf");
    qmlRegisterType<Appstream::Component>("org.asexample", 1, 0, "AsComponent");
    QQmlApplicationEngine engine(QUrl("qrc:/qml/main.qml"));
    engine.addImportPath("qrc:/qml/");

    return app.exec();
}
