#include <panda3d/cmath.h>
#include <QApplication>
#include "mainwindow.h"
#include <panda3d/pandaFramework.h>
#include <panda3d/pandaSystem.h>
#include "qpandawidget.h"
#include <iostream>
#include <QSettings>

#include "qpandaapplication.h"
#include "selectableresource.h"

int main(int argc, char *argv[])
{
    qDebug("Fallout Equestria Game Editor");
    QCoreApplication::setOrganizationName("Shinygami");
    QCoreApplication::setOrganizationDomain("plaristote.franceserv.fr");
    QCoreApplication::setApplicationName("Fallout Equestria Editor");
    QPandaApplication app(argc, argv);
    int               ret;

    {
      QSettings       settings;
      MainWindow      w(&app);

      // Loading window parameters
      settings.beginGroup("MainWindow");
      w.resize(settings.value("size", QSize(400, 400)).toSize());
      w.move(settings.value("pos", QPoint(50, 50)).toPoint());
      settings.endGroup();
      // Executing app
      w.show();
      w.splashScreen.open();
      ret = app.exec();
      // Savinf window parameters
      settings.beginGroup("MainWindow");
      settings.setValue("size", w.size());
      settings.setValue("pos", w.pos());
      settings.endGroup();
    }
    return (ret);
}