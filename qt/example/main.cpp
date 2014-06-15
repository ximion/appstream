/* Copyright (c) 2014 Matthias Klumpp <matthias@tenstral.net>
 *
 * This program is free software. It comes without any warranty, to
 * the extent permitted by applicable law. You can redistribute it
 * and/or modify it under the terms of the Do What The Fuck You Want
 * To Public License, Version 2, as published by Sam Hocevar. See
 * http://www.wtfpl.net/ for more details.
 */

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
