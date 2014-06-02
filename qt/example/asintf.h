#ifndef ASINTF_H
#define ASINTF_H

#include <QObject>
#include <appstream-qt.h>
#include <QQmlListProperty>

class AsIntf : public QObject
{
    Q_OBJECT
public:
    explicit AsIntf(QObject *parent = 0);

    Q_PROPERTY(QQmlListProperty<Appstream::Component> allComponents READ getAllComponents CONSTANT)
    QQmlListProperty<Appstream::Component> getAllComponents();

signals:

public slots:

private:
    Appstream::Database *asdb;
    QList<Appstream::Component*> m_cpts;

};

#endif // ASINTF_H
