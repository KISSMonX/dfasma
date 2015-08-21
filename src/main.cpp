/*
Copyright (C) 2014  Gilles Degottex <gilles.degottex@gmail.com>

This file is part of DFasma.

DFasma is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

DFasma is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

A copy of the GNU General Public License is available in the LICENSE.txt
file provided in the source code of DFasma. Another copy can be found at
<http://www.gnu.org/licenses/>.
*/

#include "wmainwindow.h"
#include <QApplication>
#include <QObject>

#ifdef FILE_AUDIO_LIBAV
extern "C" {
#include <stdio.h>
#include <libavformat/avformat.h>
#include <libavcodec/avcodec.h>
#include <libavutil/avutil.h>
}
#endif
#ifdef FILE_AUDIO_LIBSOX
extern "C" {
#include <sox.h>
#include <assert.h>
}
#endif

#ifdef SUPPORT_SDIF
#include <easdif/easdif.h>
#endif

#include <iostream>

#include "qthelper.h"

int main(int argc, char *argv[])
{
    #ifdef FILE_AUDIO_LIBAV
        // This call is necessarily done once in your app to initialize
        // libavformat to register all the muxers, demuxers and protocols.
        av_register_all();
        // TODO av_unregister ?
    #endif
    #ifdef FILE_AUDIO_LIBSOX
        assert(sox_init() == SOX_SUCCESS);
    #endif

    #ifdef SUPPORT_SDIF
        Easdif::EasdifInit();
    #endif

    QApplication a(argc, argv);

    // The following is also necessary for QSettings
    QCoreApplication::setOrganizationName("DFasma");
    QCoreApplication::setOrganizationDomain("gillesdegottex.eu");
    QCoreApplication::setApplicationName("DFasma");

    a.setWindowIcon(QIcon(":/icons/dfasma.svg"));

    QStringList filestoload = QApplication::arguments();
    filestoload.removeAt(0);

    WMainWindow* w = new WMainWindow(filestoload);
    QObject::connect(&a, SIGNAL(focusWindowChanged(QWindow*)), w, SLOT(focusWindowChanged(QWindow*)));
    w->show();

    int ret = a.exec();

    #ifdef SUPPORT_SDIF
        Easdif::EasdifEnd();
    #endif
    #ifdef FILE_AUDIO_LIBSOX
        sox_quit();
    #endif

    delete w;

//    COUTD << ret << std::endl;
    exit(ret); // WORKAROUND?: won't quit otherwise on some platform (e.g. bouzouki) TODO

    return ret;
}
