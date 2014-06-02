#ifndef ASINTF_H
#define ASINTF_H

#include <QObject>
#include <appstream-qt.h>

class AsIntf : public QObject
{
    Q_OBJECT
public:
    explicit AsIntf(QObject *parent = 0);

    Q_INVOKABLE QString simple_test();

signals:

public slots:

private:
    Appstream::Database *asdb;

};

#endif // ASINTF_H
