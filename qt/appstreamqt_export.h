
#ifndef APPSTREAMQT_EXPORT_H
#define APPSTREAMQT_EXPORT_H

#include <QtGlobal>

#ifdef APPSTREAMQT_STATIC_DEFINE
#  define APPSTREAMQT_EXPORT
#  define APPSTREAMQT_NO_EXPORT
#else
#  ifndef APPSTREAMQT_EXPORT
#    ifdef AppStreamQt_EXPORTS
        /* We are building this library */
#      define APPSTREAMQT_EXPORT __attribute__((visibility("default")))
#    else
        /* We are using this library */
#      define APPSTREAMQT_EXPORT __attribute__((visibility("default")))
#    endif
#  endif

#  ifndef APPSTREAMQT_NO_EXPORT
#    define APPSTREAMQT_NO_EXPORT __attribute__((visibility("hidden")))
#  endif
#endif

#ifndef APPSTREAMQT_DEPRECATED
#  define APPSTREAMQT_DEPRECATED __attribute__ ((__deprecated__))
#endif

#ifndef APPSTREAMQT_DEPRECATED_EXPORT
#  define APPSTREAMQT_DEPRECATED_EXPORT APPSTREAMQT_EXPORT APPSTREAMQT_DEPRECATED
#endif

#ifndef APPSTREAMQT_DEPRECATED_NO_EXPORT
#  define APPSTREAMQT_DEPRECATED_NO_EXPORT APPSTREAMQT_NO_EXPORT APPSTREAMQT_DEPRECATED
#endif

#if QT_VERSION >= QT_VERSION_CHECK(5, 14, 0)
#define ASQ_ENUM_DEPRECATED [[deprecated]]
#else
#define ASQ_ENUM_DEPRECATED
#endif

#endif
