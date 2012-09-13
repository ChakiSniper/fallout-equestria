#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <panda3d/cmath.h>
#include <QMainWindow>
#include <qpandaapplication.h>
#include <QTreeWidgetItem>
#include <dialognewscript.h>
#include <QFile>
#include <QFileInfo>
#include <QMessageBox>
#include <QTextStream>
#include <qangelscriptedit.h>
#include <QDir>
#include "datatree.hpp"
#include <QListWidgetItem>
#include "dialogsplashscreen.h"
#include "dialognewmap.h"
#include "wizardobject.h"
#include "dialogobject.h"
#include "dialogwaypointgenerate.h"
#include "world.h"
#include "tabscript.h"
#include "tabdialog.h"
#include "tabl18n.h"
#include "serializer.hpp"

namespace Ui {
class MainWindow;
}

class WindowFramework;

class MainWindow : public QMainWindow
{
    Q_OBJECT
    
public:
    explicit MainWindow(QPandaApplication* app, QWidget *parent = 0);
    ~MainWindow();

    void closeEvent(QCloseEvent*);

    void LoadAllMaps(void);

    bool CloseAllScript(void);

    int currentHoveredCaseX;
    int currentHoveredCaseY;

signals:
    void Closed(void);

public slots:
    //void WaypointHovered(NodePath);
    void PandaButtonPressed(QMouseEvent*);
    void PandaButtonRelease(QMouseEvent*);

private slots:
	void SetDefaultLocalization(void);
    void PandaInitialized(void);
    void LoadProject(void);
    void FilterInit(void);

    // MAPS
    void LoadMap(const QString&);
    void SaveMap(void);
    void CreateMap(void);

    void CameraMoveTop(void);
    void CameraMoveLeft(void);
    void CameraMoveRight(void);
    void CameraMoveBottom(void);

    // WAYPOINTS
    void WaypointVisible(void);
    void WaypointAdd(void);
    void WaypointDelete(void);
    void WaypointConnect(void);
    void WaypointDisconnect(void);
    void WaypointUpdateX(void);
    void WaypointUpdateY(void);
    void WaypointUpdateZ(void);
    void WaypointUpdateSelX(void);
    void WaypointUpdateSelY(void);
    void WaypointUpdateSelZ(void);
    void WaypointSelFloor(void);
    void WaypointSelDelete(void);
    void WaypointHovered(NodePath);
    void WaypointSelect(Waypoint*);
    void UpdateSelection(void);

    // ENTRY/EXIT ZONES
    void EntryZoneAdd(void);
    void EntryZoneDelete(void);
    void EntryZoneSetAsSelection(void);
    void EntryZoneChanged(QString);

    void ExitZoneAdd(void);
    void ExitZoneDelete(void);
    void ExitZoneSetAsSelection(void);
    void ExitZoneChanged(QString);
    void ExitZoneDestinationAdd(void);
    void ExitZoneDestinationDelete(void);

    // OBJECTS
    void ObjectAdd(void);

    // MAPOBJECTS
    void MapObjectVisible(void);
    void MapObjectWizard(void);
    void MapObjectAdd(void);
    void MapObjectDelete(void);
    void MapObjectHovered(NodePath);
    void MapObjectNameChanged(QString);
    void MapObjectUpdateX(void);
    void MapObjectUpdateY(void);
    void MapObjectUpdateZ(void);
    void MapObjectRotationX(void);
    void MapObjectRotationY(void);
    void MapObjectRotationZ(void);
    void MapObjectScaleX(void);
    void MapObjectScaleY(void);
    void MapObjectScaleZ(void);
    void MapObjectFloor(void);

    // DYNAMICOBJECTS
    void DynamicObjectVisible(void);
    void DynamicObjectWizard(void);
    void DynamicObjectAdd(void);
    void DynamicObjectDelete(void);
    void DynamicObjectHovered(NodePath);
    void DynamicObjectNameChanged(QString);
    void DynamicObjectUpdateX(void);
    void DynamicObjectUpdateY(void);
    void DynamicObjectUpdateZ(void);
    void DynamicObjectRotationX(void);
    void DynamicObjectRotationY(void);
    void DynamicObjectRotationZ(void);
    void DynamicObjectScaleX(void);
    void DynamicObjectScaleY(void);
    void DynamicObjectScaleZ(void);
    void DynamicObjectSetWaypoint(void);

public:
    void DrawMap(void);

private:
    WindowFramework*         _window;
    Ui::MainWindow*          ui;
    TabScript                tabScript;
    TabDialog                tabDialog;
    TabL18n                  tabL18n;

    DialogSplashscreen       splashScreen;

    QString                  levelName;
    World*                   world;
    std::list<Waypoint*>     waypointsSelection;
    Waypoint*                waypointSelected;
    Waypoint*                waypointHovered;
    float                    waypointSelX, waypointSelY, waypointSelZ;
    DialogWaypointGenerate   waypointGenerate;

    WizardObject             wizardObject;

    bool                     wizardMapObject;
    MapObject*               mapobjectSelected;
    MapObject*               mapobjectHovered;
    void                     MapObjectSelect(void);

    bool                     wizardDynObject;
    DynamicObject*           dynamicObjectSelected;
    DynamicObject*           dynamicObjectHovered;
    void                     DynamicObjectSelect(void);

    DialogNewMap             dialogNewMap;

    DialogObject             dialogObject;
    DataTree*                objectFile;
};

#endif // MAINWINDOW_H
