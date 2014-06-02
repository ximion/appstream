#include "asintf.h"

using namespace Appstream;

AsIntf::AsIntf(QObject *parent) :
    QObject(parent)
{
    asdb = new Appstream::Database(this);
    asdb->open();
}

QString
AsIntf::simple_test()
{
    QString name = "No component found!";
    QList<Component*> *cpts = asdb->getAllComponents();
    if (!cpts->isEmpty())
        name = cpts->first()->getName();
    return name;
}
