#ifndef WORLDMAPEDITOR_H
#define WORLDMAPEDITOR_H

#include <QWidget>
#include <QGraphicsScene>
#include <QGraphicsPixmapItem>
#include <QGraphicsSceneMouseEvent>
#include "datatree.hpp"
#include "citysplashdialog.h"

namespace Ui {
class WorldmapEditor;
}

class WorldmapEditor;

class WorldmapScene : public QGraphicsScene
{
public:
  void mouseReleaseEvent(QGraphicsSceneMouseEvent *event);

  WorldmapEditor* editor;
};

class CitySignalLauncher : public QObject
{
  Q_OBJECT
public:
    void Click(QString str) { Clicked(str); }
signals:
    void Clicked(QString);
};

class City : public QGraphicsEllipseItem
{
public:
  City(float x, float y, float w, float h) : QGraphicsEllipseItem(x, y, w, h)
  {}

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event);

  CitySignalLauncher my_signals;

  void SetName(QString name) { this->name = name; }
private:
  QString name;
};

class MapTileSignalLauncher : public QObject
{
  Q_OBJECT
public:
    void Click(unsigned int x, unsigned int y) { Clicked(x, y); }
signals:
    void Clicked(unsigned int x, unsigned int y);
};

class MapTile : public QGraphicsRectItem
{
public:
  unsigned int pos_x;
  unsigned int pos_y;

  void mouseReleaseEvent(QGraphicsSceneMouseEvent* event)
  {
    my_signals.Click(pos_x, pos_y);
    QGraphicsRectItem::mouseReleaseEvent(event);
  }

  MapTileSignalLauncher my_signals;
private:
};

class WorldmapEditor : public QWidget
{
    Q_OBJECT
public:
    explicit WorldmapEditor(QWidget *parent = 0);
    ~WorldmapEditor();

public slots:
    void Load(void);
    void Save(void);

    void ClickedCity(QString);
    void SelectedCity(QString);
    void OpenSplashEditor(void);
    void UpdateCitySplash(void);
    void UpdateCityData(void);
    void UpdateMapData(void);
    void AddCity(void);
    void DelCity(void);

    void MapClicked(int x, int y);
    void SelectCase(unsigned int x, unsigned int y);
    void UnselectAllCases(void);
    void SelectAllCases(void);

    void CaseAddMap(void);
    void CaseAddEncounter(void);
    void CaseDelMap(void);
    void CaseDelEncounter(void);
    void UpdateCaseData(void);

    void EncounterAddCharacter(void);
    void EncounterDelCharacter(void);
    void EncounterEditCharacter(int, int);
    void EncounterAdd(void);
    void EncounterDel(void);
    void EncounterSelect(void);
    void EncountersLoad(void);
    void EncountersSave(void);
    void EncounterRefresh(void);

    void SpecialEncounterAdd(void);
    void SpecialEncounterDelete(void);
    void SpecialEncounterLoad(void);
    void SpecialEncounterSave(void);

private:
    typedef QPair<QString, City*> CityHalo;
    typedef QList<CityHalo>       CityHalos;
    typedef QList<MapTile*>       Tiles;

    CityHalo& GetCityHalo(const QString& name);
    Data      GetCityData(const QString& name);
    void      AddCityHalo(const QString& name);
    void      DelCityHalo(const QString& name);

    MapTile*  GetTile(unsigned int x, unsigned int y);
    Data      GetTileData(unsigned int x, unsigned int y);
    void      UpdateCaseInterface(void);

    Ui::WorldmapEditor  *ui;
    WorldmapScene       scene;
    QGraphicsPixmapItem pixmap_worldmap;
    DataTree*           file_map;
    DataTree*           file_cities;
    DataTree*           data_encounters;
    DataTree*           data_special_encounters;

    CityHalos           city_halos;
    Tiles               tile_selection;

    CitySplashDialog    splash_edit;

    bool                lock_cities, lock_mspeed, lock_critters;
};

#endif // WORLDMAPEDITOR_H
