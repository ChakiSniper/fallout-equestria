#include "globals.hpp"
// REQUIREMENT FOR THE WALL-EATING BALL OF WRATH
#include <panda3d/stencilAttrib.h>
#include <panda3d/colorBlendAttrib.h>
#include <panda3d/depthTestAttrib.h>
#include <panda3d/cullFaceAttrib.h>
// END

#include "level/level.hpp"
#include "astar.hpp"

#include "level/objects/door.hpp"
#include "level/objects/shelf.hpp"
#include <level/objects/locker.hpp>
#include <i18n.hpp>

#include "ui/alert_ui.hpp"
#include "ui/ui_loot.hpp"
#include "ui/ui_equip_mode.hpp"
#include "options.hpp"
#include <mousecursor.hpp>
#include <panda_lock.hpp>

#define AP_COST_USE             2
#define WORLDTIME_TURN          10
#define WORLDTIME_DAYLIGHT_STEP 3


using namespace std;

Sync::Signal<void (InstanceDynamicObject*)> InstanceDynamicObject::ActionUse;
Sync::Signal<void (InstanceDynamicObject*)> InstanceDynamicObject::ActionUseObjectOn;
Sync::Signal<void (InstanceDynamicObject*)> InstanceDynamicObject::ActionUseSkillOn;
Sync::Signal<void (InstanceDynamicObject*)> InstanceDynamicObject::ActionUseSpellOn;
Sync::Signal<void (InstanceDynamicObject*)> InstanceDynamicObject::ActionTalkTo;


Level* Level::CurrentLevel = 0;
Level::Level(const std::string& name, WindowFramework* window, GameUi& gameUi, Utils::Packet& packet, TimeManager& tm) : _window(window), _mouse(window),
  _camera(window, window->get_camera_group()), _timeManager(tm), _main_script(name), _chatter_manager(window), _levelUi(window, gameUi)
{
  cout << "Level Loading Step #1" << endl;
  CurrentLevel = this;
  _state       = Normal;
  _mouseState  = MouseAction;
  _persistent  = true;
  _level_name  = name;

  obs.Connect(_levelUi.InterfaceOpened, *this, &Level::SetInterrupted);

  cout << "Level Loading Step #2" << endl;
  _items = DataTree::Factory::JSON("data/objects.json");

  _floor_lastWp = 0;

  for (unsigned short i = 0 ; i < UiTotalIt ; ++i)
    _currentUis[i] = 0;
  _mouseActionBlocked   = false;

  cout << "Level Loading Step #3" << endl;
  _graphicWindow = _window->get_graphics_window();

  cout << "Level Loading Step #4" << endl;
  MouseInit();
  _timer.Restart();

  // WORLD LOADING
  cout << "Level Loading Step #5" << endl;
  _world = new World(window);  
  try
  {
    _world->UnSerialize(packet);
    _light_iterator = _world->lights.begin();
  }
  catch (unsigned int&)
  {
    std::cout << "Failed to load file" << std::endl;
  }

  cout << "Level Loading Step #6" << endl;
  if (_world->sunlight_enabled)
    InitSun();

  LPoint3 upperLeft, upperRight, bottomLeft;
  cout << "Level Loading Step #7" << endl;
  _world->GetWaypointLimits(0, upperRight, upperLeft, bottomLeft);
  _camera.SetLimits(bottomLeft.get_x() - 50, bottomLeft.get_y() - 50, upperRight.get_x() - 50, upperRight.get_y() - 50);  

  cout << "Level Loading Step #8" << endl;
  ForEach(_world->entryZones, [this](EntryZone& zone)
  {
    LevelZone* lvl_zone = new LevelZone(this, zone);

    cout << "Registering Zone '" << zone.name << '\'' << endl;
    _zones.push_back(lvl_zone);
  });
  ForEach(_world->exitZones, [this](ExitZone& zone)
  {
    LevelExitZone* exitZone = new LevelExitZone(this, zone, zone.destinations);

    cout << "Registering ExitZone '" << zone.name << " with " << zone.waypoints.size() << " waypoints." << endl;
    exitZone->ExitZone.Connect      (*this, &Level::CallbackExitZone);
    exitZone->GoToNextZone.Connect  (*this, &Level::CallbackGoToZone);
    exitZone->SelectNextZone.Connect(*this, &Level::CallbackSelectNextZone);
    _zones.push_back(exitZone);
  });
  _exitingZone = false;

  ForEach(_world->dynamicObjects, [this](DynamicObject& dobj) { InsertDynamicObject(dobj); });
  _itCharacter = _characters.end();

  _world->SetWaypointsVisible(false);

  //loadingScreen->AppendText("Loading interface");
  obs.Connect(InstanceDynamicObject::ActionUse,         *this, &Level::CallbackActionUse);
  obs.Connect(InstanceDynamicObject::ActionTalkTo,      *this, &Level::CallbackActionTalkTo);
  obs.Connect(InstanceDynamicObject::ActionUseObjectOn, *this, &Level::CallbackActionUseObjectOn);
  obs.Connect(InstanceDynamicObject::ActionUseSkillOn,  *this, &Level::CallbackActionUseSkillOn);
  obs.Connect(InstanceDynamicObject::ActionUseSpellOn,  *this, &Level::CallbackActionUseSpellOn);

  _task_metabolism = _timeManager.AddTask(TASK_LVL_CITY, true, 0, 0, 1);
  _task_metabolism->Interval.Connect(*this, &Level::RunMetabolism);  


  /*
   * DIVIDE AND CONQUER WAYPOINTS
   */
  std::vector<Waypoint*>                      entries;
  
  for_each(_world->waypoints.begin(), _world->waypoints.end(), [&entries](Waypoint& wp) { entries.push_back(&wp); });
  _world->waypoint_graph.SetHeuristic([](LPoint3f position1, LPoint3f position2) -> float
  {
    float xd = position2.get_x() - position1.get_x();
    float yd = position2.get_y() - position1.get_y();
    float zd = position2.get_z() - position1.get_z();

    return (SQRT(xd * xd + yd * yd + zd * zd));
  });
  _world->waypoint_graph.Initialize(entries, [](const std::vector<Waypoint*>& entries) -> std::vector<LPoint3f>
  {
    std::vector<LPoint3f> positions;
    
    {
      LPoint3f block_size;
      LPoint3f max_pos(0, 0, 0);
      LPoint3f min_pos(0, 0, 0);
      auto     it  = entries.begin();
      auto     end = entries.end();

      for (; it != end ; ++it)
      {
        LPoint3f pos = (*it)->GetPosition();

        if (pos.get_x() < min_pos.get_x()) { min_pos.set_x(pos.get_x()); }
        if (pos.get_y() < min_pos.get_y()) { min_pos.set_y(pos.get_y()); }
        if (pos.get_z() < min_pos.get_z()) { min_pos.set_z(pos.get_z()); }
        if (pos.get_x() > max_pos.get_x()) { max_pos.set_x(pos.get_x()); }
        if (pos.get_y() > max_pos.get_y()) { max_pos.set_y(pos.get_y()); }
        if (pos.get_z() > max_pos.get_z()) { max_pos.set_z(pos.get_z()); }
      }
      
      function<float (float, float)> distance = [](float min_pos, float max_pos) -> float
      {
        if (min_pos < 0 && max_pos > 0)
          return (ABS(min_pos - max_pos));
        return (ABS(max_pos) - ABS(min_pos));
      };
      
      block_size.set_x(distance(min_pos.get_x(), max_pos.get_x()));
      block_size.set_y(distance(min_pos.get_y(), max_pos.get_y()));
      block_size.set_z(distance(min_pos.get_z(), max_pos.get_z()));

      unsigned short block_count = ceil(entries.size() / 200.f);
      for (unsigned short i = 0 ; i < block_count ; ++i)
      {
        LPoint3f block_position;
        
        block_position.set_x(min_pos.get_x() + block_size.get_x() / block_count * i);
        block_position.set_y(min_pos.get_y() + block_size.get_y() / block_count * i);
        block_position.set_z(min_pos.get_z() + block_size.get_z() / block_count * i);
        positions.push_back(block_position);
      }
    }
    return (positions);
  });
  /*
   * END DIVIDE AND CONQUER
   */

  //window->get_render().set_shader_auto();
}

LevelZone* Level::GetZoneByName(const std::string& name)
{
  auto it  = _zones.begin();
  auto end = _zones.end();

  while (it != end)
  {
    if ((*it)->GetName() == name)
      return (*it);
    ++it;
  }
  return (0);
}

void Level::RefreshCharactersVisibility(void)
{
  cout << "RefreshCharactersVisibility" << endl;
  for_each(_characters.begin(), _characters.end(), [this](ObjectCharacter* character)
  {
    std::list<ObjectCharacter*> fov = GetPlayer()->GetNearbyEnemies();
    auto                        it  = fov.begin();
    auto                        end = fov.end();
    NodePath                    np  = character->GetNodePath();

    if (character == GetPlayer())
      return ;
    else if (GetPlayer()->IsAlly(character))
      character->SetVisible(true);
    else if (GetPlayer()->IsEnemy(character))
    {
      for (; it != end ; ++it)
      {
        if (character == *it)
        {
          character->SetVisible(true);
          return ;
        }
      }
      character->SetVisible(false);
    }
    else if (GetPlayer()->HasLineOfSight(character))
      character->SetVisible(true);
    else
      character->SetVisible(false);
  });
}

void Level::InsertCharacter(ObjectCharacter* character)
{
  static unsigned short task_it = 0;
  TimeManager::Task*    task    = _timeManager.AddTask(TASK_LVL_CITY, true, 3);

  task->next_run = task->next_run + (task_it % 2);
  task->Interval.Connect(*character, &ObjectCharacter::CheckFieldOfView);
  ++task_it;
  _characters.push_back(character);
}

void Level::InsertDynamicObject(DynamicObject& object)
{
  InstanceDynamicObject* instance = 0;

  switch (object.type)
  {
    case DynamicObject::Character:
      InsertCharacter(new ObjectCharacter(this, &object));
      break ;
    case DynamicObject::Door:
      instance = new ObjectDoor(this, &object);
      break ;
    case DynamicObject::Shelf:
      instance = new ObjectShelf(this, &object);
      break ;
    case DynamicObject::Item:
    {
      DataTree*        item_data = DataTree::Factory::StringJSON(object.inventory.front().first);
      InventoryObject* item;

      item_data->key = object.key;
      item           = new InventoryObject(item_data);
      instance       = new ObjectItem(this, &object, item);
      delete item_data;
      break ;
    }
    case DynamicObject::Locker:
      instance = new ObjectLocker(this, &object);
      break ;
    default:
    {
      stringstream stream;

      stream << "Inserted unimplemented dynamic object type (" << object.type << ')';
      AlertUi::NewAlert.Emit(stream.str());
    }
  }
  cout << "Added an instance => " << instance << endl;
  if (instance != 0)
    _objects.push_back(instance);
}

PT(DirectionalLight) static_sunlight;

void Level::InitSun(void)
{
  NodePath     sun_root             = _world->floors_node;
  unsigned int shadow_caster_buffer = 128;
  unsigned int film_size            = 128;
  unsigned int graphics_quality     = OptionsManager::Get()["graphics-quality"];

  while (graphics_quality--)
  {
    shadow_caster_buffer *= 2;
    film_size            += 128;
  }
  if (static_sunlight.is_null())
    static_sunlight = new DirectionalLight("sun_light");
  _sunLight = static_sunlight;
  //_sunLight            = new DirectionalLight("sun_light");
  _sunLight->set_shadow_caster(true, shadow_caster_buffer, shadow_caster_buffer);
  _sunLight->get_lens()->set_near_far(10.f, 1200.f);
  _sunLight->get_lens()->set_film_size(film_size);
  _sunLightNode        = sun_root.attach_new_node(_sunLight);
  _sunLightNode.set_pos(150.f, 50, 50.f);
  _sunLightNode.set_hpr(127, -31,  0);

  _sunLightAmbient     = new AmbientLight("sun_light_ambient");
  _sunLightAmbientNode = sun_root.attach_new_node(_sunLightAmbient);

  sun_root.set_light(_sunLightNode, 6);
  sun_root.set_light(_sunLightAmbientNode, 5);
  sun_root.set_two_sided(false);

  _task_daylight   = _timeManager.AddTask(TASK_LVL_CITY, true, 0, 1);
  _task_daylight->Interval.Connect(*this, &Level::RunDaylight);
  _solar_circle.SetFromWorld(_world);
  RunDaylight();
}

bool StartsWith(const std::string& str, const std::string& starts_with)
{
  return (str.substr(0, starts_with.size()) == starts_with);
}

void Level::InitPlayer(void)
{
  if (!(GetPlayer()->GetStatistics().Nil()))
  {
    Data stats(GetPlayer()->GetStatistics());
    
    if (!(stats["Statistics"]["Action Points"].Nil()))
      _levelUi.GetMainBar().SetMaxAP(stats["Statistics"]["Action Points"]);
  }
  {
    ObjectCharacter::InteractionList& interactions = GetPlayer()->GetInteractions();

    interactions.clear();
    interactions.push_back(ObjectCharacter::Interaction("use_object", GetPlayer(), &(GetPlayer()->ActionUseObjectOn)));
    interactions.push_back(ObjectCharacter::Interaction("use_skill",  GetPlayer(), &(GetPlayer()->ActionUseSkillOn)));
    interactions.push_back(ObjectCharacter::Interaction("use_magic",  GetPlayer(), &(GetPlayer()->ActionUseSpellOn)));
  }
  _levelUi.GetMainBar().OpenSkilldex.Connect([this]() { ObjectCharacter::ActionUseSkillOn.Emit(GetPlayer()); });
  _levelUi.GetMainBar().OpenSpelldex.Connect([this]() { ObjectCharacter::ActionUseSpellOn.Emit(0);           });

  obs_player.Connect(GetPlayer()->HitPointsChanged,         _levelUi.GetMainBar(),   &GameMainBar::SetCurrentHp);
  obs_player.Connect(GetPlayer()->ActionPointChanged,       _levelUi.GetMainBar(),   &GameMainBar::SetCurrentAP);
  obs_player.Connect(GetPlayer()->EquipedItemActionChanged, _levelUi.GetMainBar(),   &GameMainBar::SetEquipedItemAction);
  obs_player.Connect(GetPlayer()->EquipedItemChanged,       _levelUi.GetMainBar(),   &GameMainBar::SetEquipedItem);
  obs_player.Connect(GetPlayer()->EquipedItemChanged,       _levelUi.GetInventory(), &GameInventory::SetEquipedItem);
  _levelUi.GetMainBar().EquipedItemNextAction.Connect(*GetPlayer(), &ObjectCharacter::ItemNextUseType);
  _levelUi.GetMainBar().UseEquipedItem.Connect       (*this, &Level::CallbackActionTargetUse);
  _levelUi.GetMainBar().CombatEnd.Connect            ([this](void)
  {
    StopFight();
    if (_state == Fight)
      ConsoleWrite("You can't leave combat mode if enemies are nearby.");
  });
  _levelUi.GetMainBar().CombatPassTurn.Connect       (*this, &Level::NextTurn);

  obs.Connect(_levelUi.GetInventory().EquipItem,   [this](const std::string& target, unsigned int slot, InventoryObject* object)
  {
    if (target == "equiped")
      PlayerEquipObject(slot, object);
  });
  obs.Connect(_levelUi.GetInventory().UnequipItem, [this](const std::string& target, unsigned int slot)
  {
    if (target == "equiped")
      GetPlayer()->UnequipItem(slot);
  });

  obs.Connect(_levelUi.GetInventory().DropObject,  *this,        &Level::PlayerDropObject);
  obs.Connect(_levelUi.GetInventory().UseObject,   *this,        &Level::PlayerUseObject);

  for (unsigned short it = 0 ; it < 2 ; ++it) // For every equiped item slot
  {
    _levelUi.GetMainBar().SetEquipedItem(it, GetPlayer()->GetEquipedItem(it));
    _levelUi.GetInventory().SetEquipedItem(it, GetPlayer()->GetEquipedItem(it));
  }

  //
  // The wall-eating ball of wrath
  //
  PandaNode*        node;
  CPT(RenderAttrib) atr1 = StencilAttrib::make(1, StencilAttrib::SCF_not_equal, StencilAttrib::SO_keep, StencilAttrib::SO_keep,    StencilAttrib::SO_keep,    1, 1, 0);
  CPT(RenderAttrib) atr2 = StencilAttrib::make(1, StencilAttrib::SCF_always,    StencilAttrib::SO_zero, StencilAttrib::SO_replace, StencilAttrib::SO_replace, 1, 0, 1);

  _player_halo = _window->load_model(_window->get_panda_framework()->get_models(), "misc/sphere");
  if ((_player_halo.node()) != 0)
  {
    _player_halo.set_scale(5);
    _player_halo.set_bin("background", 0);
    _player_halo.set_depth_write(0);
    _player_halo.reparent_to(_window->get_render());
    node        = _player_halo.node();
    node->set_attrib(atr2);
    node->set_attrib(ColorBlendAttrib::make(ColorBlendAttrib::M_add));
    for_each(_world->objects.begin(), _world->objects.end(), [node, atr1](MapObject& object)
    {
      std::string name = object.nodePath.get_name();
      std::string patterns[] = { "Wall", "wall", "Ceiling", "ceiling" };

      for (unsigned short i = 0 ; i < 4 ; ++i)
      {
        if (StartsWith(name, patterns[i]))
        {
          object.nodePath.set_attrib(atr1);
          break ;
        }
      }
    });
  }

  //
  // Field of view refreshing
  //
  {
    TimeManager::Task*    task    = _timeManager.AddTask(TASK_LVL_CITY, true, 1);

    task->loop     = true;
    task->next_run = task->next_run + 1;
    task->Interval.Connect(*(GetPlayer()), &ObjectCharacter::CheckFieldOfView);    
  }
  
  //
  // Initializing Main Script
  //
  if (_main_script.IsDefined("Initialize"))
    {
    _main_script.Call("Initialize");
    exit(0);
    }
}

void Level::InsertParty(PlayerParty& party)
{
  cout << "[Level] Insert Party" << endl;
  PlayerParty::DynamicObjects::reverse_iterator it, end;

  // Inserting progressively the last of PlayerParty's character at the beginning of the characters list
  // Player being the first of PlayerParty's list, it will also be the first of Level's character list.
  it  = party.GetObjects().rbegin();
  end = party.GetObjects().rend();
  for (; it != end ; ++it)
  {
    DynamicObject*   object    = _world->InsertDynamicObject(**it);

    if (object)
    {
      ObjectCharacter* character = new ObjectCharacter(this, object);

      if (character->GetStatistics().NotNil())
        character->SetActionPoints(character->GetStatistics()["Statistics"]["Action Points"]);
      _characters.insert(_characters.begin(), character);
      // Replace the Party DynamicObject pointer to the new one
      delete *it;
      *it = object;
    }
  }
  party.SetHasLocalObjects(false);
  SetupCamera();
}

void Level::FetchParty(PlayerParty& party)
{
  cout << "[Level] Fetch Party" << endl;
  PlayerParty::DynamicObjects::iterator it  = party.GetObjects().begin();
  PlayerParty::DynamicObjects::iterator end = party.GetObjects().end();

  cout << "Debug: Fetch Party" << endl;
  for (; it != end ; ++it)
  {
    Characters::iterator cit    = _characters.begin();
    Characters::iterator cend   = _characters.end();

    cout << "Fetching character: " << (*it)->nodePath.get_name() << endl;
    (*it)->nodePath.set_name(GetPlayer()->GetName());
    cout << "Fetching character: " << (*it)->nodePath.get_name() << endl;
    while (cit != cend)
    {
      ObjectCharacter* character = *cit;

      cout << "--> Comparing with " << (*cit)->GetDynamicObject()->nodePath.get_name() << endl;
      if (character->GetDynamicObject()->nodePath.get_name() == (*it)->nodePath.get_name())
      {
        cout << "--> SUCCESS" << endl;
        *it = character->GetDynamicObject();
        break ;
      }
      ++cit;
    }
  }
  //*it = GetPlayer()->GetDynamicObject();
  party.SetHasLocalObjects(false);
}

// Takes the party's characters out of the map.
// Backups their DynamicObject in PlayerParty because World is going to remove them anyway.
void Level::StripParty(PlayerParty& party)
{
  PlayerParty::DynamicObjects::iterator it  = party.GetObjects().begin();
  PlayerParty::DynamicObjects::iterator end = party.GetObjects().end();
  
  obs_player.DisconnectAll(); // Character is gonna get deleted: we must disconnect him.
  cout << "Debug: Strip Party" << endl;
  for (; it != end ; ++it)
  {
    DynamicObject*       backup = new DynamicObject;
    Characters::iterator cit    = _characters.begin();
    Characters::iterator cend   = _characters.end();

    cout << "Stripping character: " << (*it)->nodePath.get_name() << endl;
    *backup = **it;
    while (cit != cend)
    {
      ObjectCharacter* character = *cit;

      cout << "--> Comparing with: " << (*cit)->GetDynamicObject()->nodePath.get_name() << endl;
      if (character->GetDynamicObject() == *it)
      {
        cout << "--> SUCCESS" << endl;
        character->UnprocessCollisions();
        character->NullifyStatistics();
	delete character;
	_characters.erase(cit);
	_world->DeleteDynamicObject(*it);
	break ;
      }
      ++cit;
    }
    *it = backup;
  }
  party.SetHasLocalObjects(true);
}

void Level::SetPlayerInventory(Inventory* inventory)
{
  ObjectCharacter* player = GetPlayer();

  if (!inventory)
  {
    cout << "Using map's inventory" << endl;
    inventory = &(player->GetInventory());
  }
  else
  {  player->SetInventory(inventory);       }
  _levelUi.GetInventory().SetInventory(*inventory);
  player->EquipedItemChanged.Emit(0, player->GetEquipedItem(0));
  player->EquipedItemChanged.Emit(1, player->GetEquipedItem(1));  
}

Level::~Level()
{
  cout << "- Destroying Level" << endl;
  try
  {
    if (_main_script.IsDefined("Finalize"))
      _main_script.Call("Finalize");
  }
  catch (const AngelScript::Exception& exception)
  {
	AlertUi::NewAlert.Emit(std::string("Script crashed during Level destruction: ") + exception.what());
  }
  MouseCursor::Get()->SetHint("");
  _window->get_render().set_light_off(_sunLightAmbientNode);
  _window->get_render().set_light_off(_sunLightNode);
  _window->get_render().clear_light(_sunLightAmbientNode);
  _window->get_render().clear_light(_sunLightNode);
  _window->get_render().clear_light();
  _sunLightAmbientNode.remove_node();
  _sunLightNode.remove_node();
  _player_halo.remove_node();

  _timeManager.ClearTasks(TASK_LVL_CITY);
  obs.DisconnectAll();
  obs_player.DisconnectAll();
  ForEach(_zones,       [](LevelZone* zone)            { delete zone;       });
  ForEach(_projectiles, [](Projectile* projectile)     { delete projectile; });
  ForEach(_objects,     [](InstanceDynamicObject* obj) { delete obj;        });
  ForEach(_exitZones,   [](LevelExitZone* zone)        { delete zone;       });
  ForEach(_parties,     [](Party* party)               { delete party;      });
  CurrentLevel = 0;
  for (unsigned short i = 0 ; i < UiTotalIt ; ++i)
  {
    if (_currentUis[i] != 0)
      delete _currentUis[i];
  }
  if (_world) delete _world;
  if (_items) delete _items;
  cout << "-> Done." << endl;
}

bool Level::FindPath(std::list<Waypoint>& path, Waypoint& from, Waypoint& to)
{
  AstarPathfinding<Waypoint>        astar;
  int                               max_iterations = 0;
  AstarPathfinding<Waypoint>::State state;

  astar.SetStartAndGoalStates(from, to);
  while ((state = astar.SearchStep()) == AstarPathfinding<Waypoint>::Searching && max_iterations++ < 250);

  if (state == AstarPathfinding<Waypoint>::Succeeded)
  {
    path = astar.GetSolution();
    return (true);
  }
  return (false);  
}

InstanceDynamicObject* Level::GetObject(const string& name)
{
  InstanceObjects::iterator it  = _objects.begin();
  InstanceObjects::iterator end = _objects.end();

  for (; it != end ; ++it)
  {
    if ((*(*it)) == name)
      return (*it);
  }
  return (0);
}

Level::CharacterList Level::FindCharacters(function<bool (ObjectCharacter*)> selector) const
{
  CharacterList              list;
  Characters::const_iterator it  = _characters.begin();
  Characters::const_iterator end = _characters.end();

  for (; it != end ; ++it)
  {
    ObjectCharacter* character = *it;
    
    if (selector(character))
      list.push_back(character);
  }
  return (list);
}

ObjectCharacter* Level::GetCharacter(const string& name)
{
  Characters::const_iterator it  = _characters.begin();
  Characters::const_iterator end = _characters.end();

  for (; it != end ; ++it)
  {
    if ((*(*it)) == name)
      return (*it);
  }
  return (0);
}

ObjectCharacter* Level::GetCharacter(const DynamicObject* object)
{
  Characters::iterator it  = _characters.begin();
  Characters::iterator end = _characters.end();

  for (; it != end ; ++it)
  {
    if ((*(*it)).GetDynamicObject() == object)
      return (*it);
  }
  return (0);
}

ObjectCharacter* Level::GetPlayer(void)
{
  if (_characters.size() == 0)
    return (0);
  return (_characters.front());
}

void Level::SetState(State state)
{
  _state = state;
  if (state == Normal)
    _itCharacter = _characters.end();
  if (state != Fight)
  {
    DestroyCombatPath();
    ToggleCharacterOutline(false);
  }
  _camera.SetEnabledScroll(state != Interrupted);
  SetupCamera();  
}

void Level::SetInterrupted(bool set)
{
  for (int i = 0 ; i < UiTotalIt ; ++i)
  {
    if (_currentUis[i] && _currentUis[i]->IsVisible())
      set = true;
  }
  
  if (set)
    SetState(Interrupted);
  else
  {
    if (_itCharacter == _characters.end())
      SetState(Normal);
    else
      SetState(Fight);
  }
}

void Level::StartFight(ObjectCharacter* starter)
{
  _itCharacter = std::find(_characters.begin(), _characters.end(), starter);
  if (_itCharacter == _characters.end())
  { 
    cout << "[FATAL ERROR][Level::StartFight] Unable to find starting character" << endl;
    if (_characters.size() < 1)
    {
      cout << "[FATAL ERROR][Level::StartFight] Can't find a single character" << endl;
      return ;
    }
    _itCharacter = _characters.begin();
  }
  _levelUi.GetMainBar().SetEnabledAP(true);
  (*_itCharacter)->RestartActionPoints();
  if (_state != Fight)
    ConsoleWrite("You are now in combat mode.");
  SetState(Fight);
}

void Level::StopFight(void)
{
  if (_state == Fight)
  {
    Characters::iterator it  = _characters.begin();
    Characters::iterator end = _characters.end();
    
    for (; it != end ; ++it)
    {
      if (!((*it)->IsAlly(GetPlayer())))
      {
        list<ObjectCharacter*> listEnemies = (*it)->GetNearbyEnemies();

        if (!(listEnemies.empty()) && (*it)->IsAlive())
          return ;
      }
    }
    if (_mouseState == MouseTarget)
      SetMouseState(MouseAction);
    ConsoleWrite("Combat ended.");
    SetState(Normal);
    _levelUi.GetMainBar().SetEnabledAP(false);
  }
}

void Level::NextTurn(void)
{
  if (_state != Fight || _currentCharacter != _itCharacter)
  {
    cout << "cannot go to next turn" << endl;
    return ;
  }
  if (*_itCharacter == GetPlayer())
    StopFight();
  if (_itCharacter != _characters.end())
  {
    cout << "Playing animation idle" << endl;
    (*_itCharacter)->PlayAnimation("idle");
  }
  if ((++_itCharacter) == _characters.end())
  {
    _itCharacter = _characters.begin();
    (*_itCharacter)->CheckFieldOfView();
    _timeManager.AddElapsedTime(WORLDTIME_TURN);
  }
  if (_itCharacter != _characters.end())
    (*_itCharacter)->RestartActionPoints();
  else
    cout << "[FATAL ERROR][Level::NextTurn] Character Iterator points to nothing (n_characters = " << _characters.size() << ")" << endl;
  SetupCamera();
}

void Level::SetupCamera(void)
{
  if (_state == Fight)
  {
    if  (*_itCharacter != GetPlayer())
    {
      if (OptionsManager::Get()["camera"]["fight"]["focus-enemies"].Value() == "1")
        _camera.FollowObject(*_itCharacter);
      else
        _camera.StopFollowingNodePath();
    }
    else
    {
      if (OptionsManager::Get()["camera"]["fight"]["focus-self"].Value() == "1")
        _camera.FollowObject(*_itCharacter);
      else
        _camera.StopFollowingNodePath();
    }
  }
  else
  {
    if (OptionsManager::Get()["camera"]["focus-self"].Value() == "1")
      _camera.FollowObject(GetPlayer());
    else
      _camera.StopFollowingNodePath();
  }
}

void Level::RunProjectiles(float elapsed_time)
{
  auto it  = _projectiles.begin();
  auto end = _projectiles.end();

  while (it != end)
  {
    if ((*it)->HasExpired())
    {
      delete *it;
      it = _projectiles.erase(it);
    }
    else
    {
      (*it)->Run(elapsed_time);
      ++it;
    }
  }
}

void Level::RunMetabolism(void)
{
  for_each(_characters.begin(), _characters.end(), [this](ObjectCharacter* character)
  {
    if (character != GetPlayer() && character->GetHitPoints() > 0)
    {
      StatController* controller = character->GetStatController();

      controller->RunMetabolism();
    }
  });
}

void Level::RunDaylight(void)
{
  //cout << "Running daylight task" << endl;
  if (_sunLightNode.is_empty())
    return ;
  
  LVecBase4f color_steps[6] = {
    LVecBase4f(0.2, 0.2, 0.5, 1), // 00h00
    LVecBase4f(0.2, 0.2, 0.3, 1), // 04h00
    LVecBase4f(0.7, 0.7, 0.5, 1), // 08h00
    LVecBase4f(1.0, 1.0, 1.0, 1), // 12h00
    LVecBase4f(1.0, 0.8, 0.8, 1), // 16h00
    LVecBase4f(0.4, 0.4, 0.6, 1)  // 20h00
  };
  
  DateTime current_time    = _task_daylight->next_run + _task_daylight->length;
  int      current_hour    = current_time.GetHour();
  int      current_minute  = current_time.GetMinute();
  int      current_second  = current_time.GetSecond();
  int      it              = current_hour / 4;
  float    total_seconds   = 60 * 60 * 4;
  float    elapsed_seconds = current_second + (current_minute * 60) + ((current_hour - (it * 4)) * 60 * 60);

  LVecBase4f to_set(0, 0, 0, 0);
  LVecBase4f dif = (it == 5 ? color_steps[0] : color_steps[it + 1]) - color_steps[it];

  // dif / 5 * (it % 4) -> dif / 100 * (20 * (it % 4)) is this more clear ? I hope it is.
  to_set += color_steps[it] + (dif / total_seconds * elapsed_seconds);
  _sunLight->set_color(to_set);
  to_set.set_w(0.1);
  _sunLightAmbient->set_color(to_set / 3);
  //cout << "Sunlight color [" << _timeManager.GetHour() << "]->[" << it << "]: " << to_set.get_x() << "," << to_set.get_y() << "," << to_set.get_z() << endl;

  // Angle va de 0/180 de 8h/20h   20h/8h
  float    step  = (current_hour) % 12 + (current_minute / 60.f);
  float    angle = ((180.f / 120.f) / 12.f) * step;
  LPoint2f pos   = _solar_circle.PointAtAngle(angle);

  //cout << "Sun Step: " << step << endl;
  //cout << "Sun Position is (" << pos.get_x() << "," << pos.get_y() << ')' << endl;
  _sunLightNode.set_z(pos.get_x());
  _sunLightNode.set_y(pos.get_y());
  _sunLightNode.look_at(_solar_circle.GetPosition());
}

void Level::MouseSuccessRateHint(void)
{
  InstanceDynamicObject* dynObject = FindObjectFromNode(_mouse.Hovering().dynObject);
  
  if (dynObject)
  {
    ObjectCharacter*     player    = GetPlayer();
    InventoryObject*     item      = player->active_object;
    unsigned char        actionIt  = player->active_object_it;

    if ((*item)["actions"][actionIt]["combat"] == "1")
    {
      ObjectCharacter*   target = dynObject->Get<ObjectCharacter>();
      int                rate;

      if (!target)
        return ;
      rate = item->HitSuccessRate(player, target, actionIt);
      MouseCursor::Get()->SetHint(rate);
    }
    else
      ; // Not implemented yet
  }
}

AsyncTask::DoneStatus Level::do_task(void)
{
  float elapsedTime = _timer.GetElapsedTime();

  if (_levelUi.GetContext()->GetHoverElement() == _levelUi.GetContext()->GetRootElement())
    SetMouseState(_mouseState);
  else
    _mouse.SetMouseState('i');

  // TEST Transparent Ball of Wrath
  if (!(_player_halo.is_empty()))
  {
    _player_halo.set_pos(GetPlayer()->GetDynamicObject()->nodePath.get_pos());
    _player_halo.set_hpr(GetPlayer()->GetDynamicObject()->nodePath.get_hpr());
  }
  // TEST End

  _camera.SlideToHeight(GetPlayer()->GetDynamicObject()->nodePath.get_z());
  _camera.Run(elapsedTime);  

  //
  // Level Mouse Hints
  //
  if (_state != Interrupted && _levelUi.GetContext()->GetHoverElement() == _levelUi.GetContext()->GetRootElement())
  {
    _mouse.ClosestWaypoint(_world, _currentFloor);
    if (_mouse.Hovering().hasWaypoint)
    {
      unsigned int waypoint_id = _mouse.Hovering().waypoint_ptr->id;
      
      if (_world->IsInExitZone(waypoint_id))
        MouseCursor::Get()->SetHint("exit");
      else
        MouseCursor::Get()->SetHint("");
    }
    else
      MouseCursor::Get()->SetHint("nowhere");
    if (_mouseState == MouseTarget && _mouse.Hovering().hasDynObject)
      MouseSuccessRateHint();
  }
  else
    MouseCursor::Get()->SetHint("");
  //
  // End Level Mouse Hints
  //

  std::function<void (InstanceDynamicObject*)> run_object = [elapsedTime](InstanceDynamicObject* obj)
  {
    obj->Run(elapsedTime);
    obj->UnprocessCollisions();
    obj->ProcessCollisions();
  };
  switch (_state)
  {
    case Fight:
      ForEach(_objects, run_object);
      // If projectiles are moving, run them. Otherwise, run the current character
      if (_projectiles.size() > 0)
        RunProjectiles(elapsedTime);
      else
      {
        _currentCharacter = _itCharacter; // Keep a character from askin NextTurn several times
        if (_itCharacter != _characters.end())
          run_object(*_itCharacter);
        if (_mouse.Hovering().hasWaypoint && _mouse.Hovering().waypoint != _last_combat_path && _mouseState == MouseAction)
          DisplayCombatPath();
      }
      break ;
    case Normal:
      RunProjectiles(elapsedTime);
      _timeManager.AddElapsedSeconds(elapsedTime);
      ForEach(_objects,    run_object);
      ForEach(_characters, run_object);
      break ;
    case Interrupted:
      break ;
  }
  ForEach(_characters, [elapsedTime](ObjectCharacter* character) { character->RunEffects(elapsedTime); });
  ForEach(_zones,      []           (LevelZone* zone)            { zone->Refresh();                    });
  
  if (_main_script.IsDefined("Run"))
  {
    AngelScript::Type<float> param_time(elapsedTime);

    _main_script.Call("Run", 1, &param_time);
  }

  CheckCurrentFloor(elapsedTime);
  _chatter_manager.Run(elapsedTime, _camera.GetNodePath());
  _particle_manager.do_particles(ClockObject::get_global_clock()->get_dt());
  _mouse.Run();
  _timer.Restart();
  return (_exitingZone ? AsyncTask::DS_done : AsyncTask::DS_cont);
}

/*
 * Nodes Management
 */
InstanceDynamicObject* Level::FindObjectFromNode(NodePath node)
{
  {
    InstanceObjects::iterator cur = _objects.begin();
    
    while (cur != _objects.end())
    {
      if ((**cur) == node)
	return (*cur);
      ++cur;
    }
  }
  {
    Characters::iterator      cur = _characters.begin();
    
    while (cur != _characters.end())
    {
      if ((**cur) == node)
	return (*cur);
      ++cur;
    }
  }
  return (0);
}

void                   Level::RemoveObject(InstanceDynamicObject* object)
{
  InstanceObjects::iterator it = std::find(_objects.begin(), _objects.end(), object);
  
  if (it != _objects.end())
  {
    _world->DeleteDynamicObject((*it)->GetDynamicObject());
    delete (*it);
    _objects.erase(it);
  }
}

void                   Level::UnprocessAllCollisions(void)
{
  ForEach(_objects,    [](InstanceDynamicObject* object) { object->UnprocessCollisions(); });
  ForEach(_characters, [](ObjectCharacter*       object) { object->UnprocessCollisions(); });
}

void                   Level::ProcessAllCollisions(void)
{
  ForEach(_objects,    [](InstanceDynamicObject* object) { object->ProcessCollisions(); });
  ForEach(_characters, [](ObjectCharacter*       object) { object->ProcessCollisions(); });
}

/*
 * Level Mouse
 */
void Level::MouseInit(void)
{
  SetMouseState(MouseAction);
  _mouse.ButtonLeft.Connect  (*this,   &Level::MouseLeftClicked);
  _mouse.ButtonRight.Connect (*this,   &Level::MouseRightClicked);
  _mouse.WheelUp.Connect     (*this,   &Level::MouseWheelUp);
  _mouse.WheelDown.Connect   (*this,   &Level::MouseWheelDown);
  _mouse.ButtonMiddle.Connect(_camera, &SceneCamera::SwapCameraView);
}

void Level::SetMouseState(MouseState state)
{
  if (state != _mouseState)
  {
    // Cleanup if needed
    switch (_mouseState)
    {
      case MouseTarget:
        TargetPicked.DisconnectAll();
        break ;
      case MouseWaypointPicker:
        WaypointPicked.DisconnectAll();
        break ;
      default:
        break ;
    }
    DestroyCombatPath();
    _mouseState = state;
    ToggleCharacterOutline(_state == Level::Fight && _mouseState == MouseTarget && *_itCharacter == GetPlayer());
  }
  // Set mouse cursor
  switch (state)
  {
    case MouseWaypointPicker:
    case MouseAction:
      _mouse.SetMouseState('a');
      break ;
    case MouseInteraction:
      _mouse.SetMouseState('i');
      break ;
    case MouseTarget:
      _mouse.SetMouseState('t');
      break ;
  }
}

void Level::MouseWheelDown(void)
{
  if (!(_levelUi.GetContext()->GetHoverElement() != _levelUi.GetContext()->GetRootElement()))
  {
    Data options = OptionsManager::Get();
    float distance = options["camera"]["distance"];

    if (distance < 140.f)
      distance += 10.f;
    options["camera"]["distance"] = distance;
    _camera.RefreshCameraHeight();
  }
}

void Level::MouseWheelUp(void)
{
  if (!(_levelUi.GetContext()->GetHoverElement() != _levelUi.GetContext()->GetRootElement()))
  {
    Data options = OptionsManager::Get();
    float distance = options["camera"]["distance"];

    if (distance > 50.f)
      distance -= 10.f;
    options["camera"]["distance"] = distance;
    _camera.RefreshCameraHeight();
  }
}

void Level::MouseLeftClicked(void)
{
  const MouseHovering& hovering = _mouse.Hovering();

  if (_mouseActionBlocked || _state == Interrupted)
    return ;
  if (_levelUi.GetContext()->GetHoverElement() != _levelUi.GetContext()->GetRootElement())
    return ;
  switch (_mouseState)
  {
    case MouseAction:
      if (_currentUis[UiItInteractMenu] && _currentUis[UiItInteractMenu]->IsVisible())
	return ;
      else
      {
	_mouse.ClosestWaypoint(_world, _currentFloor);
	if (hovering.hasWaypoint)
	{
	  Waypoint* toGo = _world->GetWaypointFromNodePath(hovering.waypoint);

	  if (toGo && _characters.size() > 0)
	    GetPlayer()->GoTo(toGo);
	}
      }
      break ;
    case MouseInteraction:
      if (hovering.hasDynObject)
      {
        InstanceDynamicObject* object = FindObjectFromNode(hovering.dynObject);

        if (_currentUis[UiItInteractMenu] && _currentUis[UiItInteractMenu]->IsVisible())
          CloseRunningUi<UiItInteractMenu>();
        if (object && object->GetInteractions().size() != 0)
        {
          CloseRunningUi<UiItInteractMenu>();
          _currentUis[UiItInteractMenu] = new InteractMenu(_window, _levelUi.GetContext(), *object);
          _camera.SetEnabledScroll(false);
        }
      }
      break ;
    case MouseTarget:
      std::cout << "Mouse Target" << std::endl;
      if (hovering.hasDynObject)
      {
        InstanceDynamicObject* dynObject = FindObjectFromNode(hovering.dynObject);

        std::cout << "HasDynObject" << std::endl;
        if (dynObject)
          TargetPicked.Emit(dynObject);
      }
      break ;
    case MouseWaypointPicker:
      {
        std::cout << "Mouse Waypoint Picker" << std::endl;
        _mouse.ClosestWaypoint(_world, _currentFloor);
        if (hovering.hasWaypoint)
        {
          Waypoint* toGo = _world->GetWaypointFromNodePath(hovering.waypoint);

          if (toGo)
            WaypointPicked.Emit(toGo);
        }
      }
      break ;
  }
}

void Level::MouseRightClicked(void)
{
  CloseRunningUi<UiItInteractMenu>();
  SetMouseState(_mouseState == MouseInteraction || _mouseState == MouseTarget ? MouseAction : MouseInteraction);
}

void Level::ConsoleWrite(const string& str)
{
  _levelUi.GetMainBar().AppendToConsole(str);
}

void Level::PlayerLootWithScript(Inventory* inventory, InstanceDynamicObject* target, asIScriptContext* context, const std::string& filepath)
{
  UiBase* old_ptr = _currentUis[UiItLoot];

  PlayerLoot(inventory);
  if (!(old_ptr == _currentUis[UiItLoot] || _currentUis[UiItLoot] == 0))
  {
    UiLoot* ui_loot = (UiLoot*)(_currentUis[UiItLoot]);

    ui_loot->SetScriptObject(GetPlayer(), target, context, filepath);
  }
}

void Level::PlayerLoot(Inventory* inventory)
{
  if (!inventory)
  {
    Script::Engine::ScriptError.Emit("<span class='console-error'>[PlayerLoot] Aborted: NullPointer Error</span>");
    return ;
  }
  CloseRunningUi<UiItInteractMenu>();
  {
    UiLoot* ui_loot = new UiLoot(_window, _levelUi.GetContext(), GetPlayer()->GetInventory(), *inventory);

    ui_loot->Done.Connect(*this, &Level::CloseRunningUi<UiItLoot>);
    _mouseActionBlocked = true;
    _camera.SetEnabledScroll(false);
    SetInterrupted(true);
    _currentUis[UiItLoot] = ui_loot;
  }
}

void Level::PlayerEquipObject(unsigned short it, InventoryObject* object)
{
  bool canWeildMouth        = object->CanWeild(GetPlayer(), "equiped", EquipedMouth);
  bool canWeildMagic        = object->CanWeild(GetPlayer(), "equiped", EquipedMagic);
  bool canWeildBattleSaddle = object->CanWeild(GetPlayer(), "equiped", EquipedBattleSaddle);
  int  canWeildTotal        = (canWeildMouth ? 1 : 0) + (canWeildMagic ? 1 : 0) + (canWeildBattleSaddle ? 1 : 0);

  if (canWeildTotal >= 2)
  {
    UiEquipMode* ui = new UiEquipMode(_window, _levelUi.GetContext());

    if (canWeildMouth)
      ui->AddOption(EquipedMouth,        "Mouth/Hoof");
    if (canWeildMagic)
      ui->AddOption(EquipedMagic,        "Magic");
    if (canWeildBattleSaddle)
      ui->AddOption(EquipedBattleSaddle, "Battlesaddle");
    ui->Initialize();

    CloseRunningUi<UiItEquipMode>();
    _currentUis[UiItEquipMode] = ui;
    ui->Closed.Connect(*this, &Level::CloseRunningUi<UiItEquipMode>);
    ui->EquipModeSelected.Connect([this, it, object](unsigned char mode)
    {
      _currentUis[UiItEquipMode]->Destroy();
      GetPlayer()->GetInventory().SetEquipedItem("equiped", it, object, (EquipedMode)mode);
    });
  }
  else if (canWeildTotal)
  {
    GetPlayer()->GetInventory().SetEquipedItem("equiped", it, object, (canWeildMouth ? EquipedMouth :
                                                                      (canWeildMagic ? EquipedMagic : EquipedBattleSaddle)));
  }
  else
    ConsoleWrite("You can't equip " + object->GetName());
}

void Level::PlayerEquipObject(const std::string& target, unsigned int slot, InventoryObject* object)
{
  if (target == "equiped")
    PlayerEquipObject(slot, object);
  else
  {
  }
}

void Level::PlayerDropObject(InventoryObject* object)
{
  ActionDropObject(GetPlayer(), object);
}

void Level::PlayerUseObject(InventoryObject* object)
{
  ActionUseObjectOn(GetPlayer(), GetPlayer(), object, 0); // Default action is 0
}

void Level::ActionDropObject(ObjectCharacter* user, InventoryObject* object)
{
  Waypoint*   waypoint;

  if (!user || !object)
  {
    Script::Engine::ScriptError.Emit("<span class='console-error'>[ActionDropObject] Aborted: NullPointer Error</span>");
    return ;
  }
  if (!(UseActionPoints(AP_COST_USE)))
    return ;
  waypoint = user->GetOccupiedWaypoint();
  if (waypoint)
  {
    DynamicObject* graphics = object->CreateDynamicObject(_world);
    
    if (graphics)
    {
      ObjectItem*    item;

      user->GetInventory().DelObject(object);
      item     = new ObjectItem(this, graphics, object);
      _world->DynamicObjectChangeFloor(*item->GetDynamicObject(), waypoint->floor);
      item->SetOccupiedWaypoint(waypoint);
      _world->DynamicObjectSetWaypoint(*(item->GetDynamicObject()), *waypoint);
      item->GetDynamicObject()->nodePath.set_pos(waypoint->nodePath.get_pos());
      _objects.push_back(item);
    }
  }
  else
    cerr << "User has no waypoint, thus the object can't be dropped" << endl;
}

bool Level::UseActionPoints(unsigned short ap)
{
  if (_state == Fight)
  {
    ObjectCharacter& character = (**_itCharacter);
    unsigned short   charAp    = character.GetActionPoints();

    if (charAp >= ap)
      character.SetActionPoints(charAp - ap);
    else
    {
      if (&character == GetPlayer())
        ConsoleWrite("Not enough action points");
      return (false);
    }
  }
  return (true);
}

bool Level::IsWaypointOccupied(unsigned int id) const
{
  InstanceObjects::const_iterator it_object;
  Characters::const_iterator      it_character;
  
  for (it_object = _objects.begin() ; it_object != _objects.end() ; ++it_object)
  {
    if ((*it_object)->HasOccupiedWaypoint() && (int)id == ((*it_object)->GetOccupiedWaypointAsInt()))
      return (true);
  }
  for (it_character = _characters.begin() ; it_character != _characters.end() ; ++it_character)
  {
    if ((*it_character)->HasOccupiedWaypoint() && (int)id == ((*it_character)->GetOccupiedWaypointAsInt()))
      return (true);
  }
  return (false);
}

ISampleInstance* Level::PlaySound(const string& name)
{
  if (_sound_manager.Require(name))
  {
    ISampleInstance* instance = _sound_manager.CreateInstance(name);

    instance->Start();
    return (instance);
  }
  return (0);
}