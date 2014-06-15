/*
 * Copyright (C) 2014 Matthias Klumpp <matthias@tenstral.net>
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

#include "appstream.h"
#include "component.h"
#include "utils-private.h"

using namespace Appstream;

class Appstream::ComponentPrivate
{
public:
    ComponentPrivate(bool create_ascpt = true)
    {
        if (create_ascpt)
            cpt = as_component_new ();
    }

    ~ComponentPrivate() {
        g_object_unref (cpt);
    }

    AsComponent *cpt;
    QList<Screenshot*> sshots;
};

Component::Component(QObject *parent)
    : QObject(parent)
{

    priv = new ComponentPrivate();
}

Component::Component(AsComponent *cpt, QObject *parent)
    : QObject(parent)
{
    priv = new ComponentPrivate(false);
    priv->cpt = cpt;
}

Component::~Component()
{
    delete priv;
}

QString
Component::toString()
{
    gchar *str;
    QString qstr;

    str = as_component_to_string (priv->cpt);
    qstr = QString::fromUtf8 (str);
    return qstr;
}

Component::Kind
Component::getKind()
{
    return (Component::Kind) as_component_get_kind (priv->cpt);
}

void
Component::setKind(Component::Kind kind)
{
    as_component_set_kind (priv->cpt, (AsComponentKind) kind);
}

QString
Component::kindToString(Component::Kind kind)
{
    return QString::fromUtf8(as_component_kind_to_string ((AsComponentKind) kind));

}

Component::Kind
Component::kindFromString(QString kind_str)
{
    return (Component::Kind) as_component_kind_from_string (qPrintable(kind_str));

}

QString
Component::getId()
{
    return QString::fromUtf8(as_component_get_id(priv->cpt));
}

void
Component::setId(QString id)
{
    as_component_set_id(priv->cpt, qPrintable(id));
}

QString
Component::getPkgName()
{
    return QString::fromUtf8(as_component_get_pkgname(priv->cpt));
}

void
Component::setPkgName(QString pkgname)
{
    as_component_set_pkgname(priv->cpt, qPrintable(pkgname));
}

QString
Component::getName()
{
    return QString::fromUtf8(as_component_get_name(priv->cpt));
}

void
Component::setName(QString name)
{
    as_component_set_name(priv->cpt, qPrintable(name));
}

QString
Component::getSummary()
{
    return QString::fromUtf8(as_component_get_summary(priv->cpt));
}

void
Component::setSummary(QString summary)
{
    as_component_set_summary(priv->cpt, qPrintable(summary));
}

QString
Component::getDescription()
{
    return QString::fromUtf8(as_component_get_description(priv->cpt));
}

void
Component::setDescription(QString description)
{
    as_component_set_description(priv->cpt, qPrintable(description));
}

QString
Component::getProjectLicense()
{
    return QString::fromUtf8(as_component_get_project_license(priv->cpt));
}

void
Component::setProjectLicense(QString license)
{
    as_component_set_project_license(priv->cpt, qPrintable(license));
}

QString
Component::getProjectGroup()
{
    return QString::fromUtf8(as_component_get_project_group(priv->cpt));
}

void
Component::setProjectGroup(QString group)
{
    as_component_set_project_group(priv->cpt, qPrintable(group));
}

QStringList
Component::getCompulsoryForDesktops()
{
    return strv_to_stringlist(as_component_get_compulsory_for_desktops(priv->cpt));
}

void
Component::setCompulsoryForDesktops(QStringList desktops)
{
    gchar **strv;
    strv = stringlist_to_strv(desktops);
    as_component_set_compulsory_for_desktops(priv->cpt, strv);
    g_strfreev(strv);
}

bool
Component::isCompulsoryForDesktop(QString desktop)
{
    return as_component_is_compulsory_for_desktop(priv->cpt, qPrintable(desktop));
}

QStringList
Component::getCategories()
{
    return strv_to_stringlist(as_component_get_categories(priv->cpt));
}

void
Component::setCategories(QStringList categories)
{
    gchar **strv;
    strv = stringlist_to_strv(categories);
    as_component_set_categories(priv->cpt, strv);
    g_strfreev(strv);
}

bool
Component::hasCategory(QString category)
{
    return as_component_has_category(priv->cpt, qPrintable(category));
}

QString
Component::getIcon()
{
    return QString::fromUtf8(as_component_get_icon(priv->cpt));
}

void
Component::setIcon(QString icon)
{
    as_component_set_icon(priv->cpt, qPrintable(icon));
}

QUrl
Component::getIconUrl()
{
    QString iconUrl = QString::fromUtf8(as_component_get_icon_url(priv->cpt));
    if (!iconUrl.isEmpty()) {
        if (iconUrl[0] == '/')
            iconUrl = "file://" + iconUrl;
    }
    return QUrl(iconUrl);
}

void
Component::setIconUrl(QUrl icon_url)
{
    as_component_set_icon_url(priv->cpt, qPrintable(icon_url.toString()));
}

QStringList
Component::getExtends()
{
    return strarray_to_stringlist(as_component_get_extends(priv->cpt));
}

QList<Screenshot*>
Component::getScreenshots()
{
    /* check if we have cached something */
    if (!priv->sshots.isEmpty())
        return priv->sshots;

    GPtrArray *array = as_component_get_screenshots (priv->cpt);
    for (unsigned int i = 0; i < array->len; i++) {
        AsScreenshot *asScr;
        asScr = (AsScreenshot*) g_ptr_array_index (array, i);
        Screenshot *scr = new Screenshot (this);
        scr->setKind((Screenshot::Kind) as_screenshot_get_kind (asScr));
        scr->setCaption(QString::fromUtf8(as_screenshot_get_caption (asScr)));

        GPtrArray *images = as_screenshot_get_images (asScr);
        for (unsigned int j = 0; j < images->len; j++) {
            AsImage *asImg = (AsImage*) g_ptr_array_index (images, i);
            Image *img = new Image (scr);
            img->setKind((Image::Kind) as_image_get_kind (asImg));
            img->setHeight(as_image_get_height (asImg));
            img->setWidth(as_image_get_width (asImg));
            QString url = QString::fromUtf8(as_image_get_url(asImg));
            img->setUrl(QUrl(url));
        }

        priv->sshots.append(scr);
    }

    return priv->sshots;
}
