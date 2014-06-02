#include "QApplication"
#include <QUrl>
#include <QQmlApplicationEngine>
#include <QQmlComponent>
#include "asintf.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    qmlRegisterType<AsIntf>("org.asexample", 1, 0, "AsIntf");
    QQmlApplicationEngine engine(QUrl("qrc:/qml/main.qml"));

    return app.exec();
}
