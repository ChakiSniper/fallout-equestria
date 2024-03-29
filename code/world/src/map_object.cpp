#include "world/world.h"
#include "world/map_object.hpp"
#include "world/waypoint.hpp"
#include "world/light.hpp"
#include "serializer.hpp"
#ifdef GAME_EDITOR
# include "qpandaapplication.h"
#else
  extern PandaFramework* framework;
  extern bool world_is_game_save;
#endif

using namespace std;

extern unsigned int blob_revision;

void MapObject::InitializeTree(World *world)
{
  std::function<void (MapObject&)> find_children = [this, world](MapObject& object)
  {
    if (name == object.parent)
      object.ReparentTo(this);
  };
  for_each(world->objects.begin(),        world->objects.end(),        find_children);
  for_each(world->dynamicObjects.begin(), world->dynamicObjects.end(), [find_children](DynamicObject& object) { find_children(object); });
  if (parent == "")
  {
    if (world->floors.size() <= floor)
      world->FloorResize(floor + 1);
    nodePath.reparent_to(world->floors[floor]);
  }
}

void MapObject::SetFloor(unsigned char floor)
{
  for_each(waypoints.begin(), waypoints.end(), [floor](Waypoint* waypoint)
  {
     waypoint->floor = floor;
  });
  for_each(children.begin(), children.end(), [floor](MapObject* child)
  {
    if (child->inherits_floor)
      child->SetFloor(floor);
  });
  this->floor = floor;
}

MapObject::~MapObject()
{
  ReparentTo(0);
  for_each(children.begin(), children.end(), [this](MapObject* child)
  {
    child->nodePath.reparent_to(nodePath.get_parent());
    child->parent        = "";
    child->parent_object = 0;
  });
}

void MapObject::ReparentTo(MapObject* object)
{
  if (parent_object)
    parent_object->children.erase(find(parent_object->children.begin(), parent_object->children.end(), this));
  parent_object   = object;
  if (object)
  {
    parent        = object->nodePath.get_name();
    object->children.push_back(this);
    if (object->floor != floor && inherits_floor)
      SetFloor(object->floor);
    nodePath.reparent_to(object->nodePath);
#ifndef GAME_EDITOR
    if (!inherits_floor)
      ReparentToFloor(World::LoadingWorld, floor);
#endif
  }
  else
    parent = "";
}

void MapObject::ReparentToFloor(World* world, unsigned char floor)
{
#ifndef GAME_EDITOR
  LPoint3f current_pos   = nodePath.get_pos(world->window->get_render());
  LPoint3f current_hpr   = nodePath.get_hpr(world->window->get_render());
  LPoint3f current_scale = nodePath.get_scale(world->window->get_render());
#endif

  world->MapObjectChangeFloor(*this, floor);
#ifndef GAME_EDITOR
  if (!world_is_game_save)
  {
    nodePath.set_pos(world->window->get_render(), current_pos);
    nodePath.set_hpr(world->window->get_render(), current_hpr);
    nodePath.set_scale(world->window->get_render(), current_scale);
  }
#endif
}

void MapObject::SetName(const std::string& name)
{
  this->name = name;
  if (!(nodePath.is_empty()))
    nodePath.set_name(name);
}

void MapObject::SetModel(const std::string& model)
{
#ifdef GAME_EDITOR
  PandaFramework& panda_framework = QPandaApplication::Framework();
#else
  PandaFramework& panda_framework = *framework;
#endif

  if (!(render.is_empty()))
    render.remove_node();
  strModel = model;
  render   = panda_framework.get_window(0)->load_model(panda_framework.get_models(), MODEL_ROOT + strModel);
  render.set_name("render-" + nodePath.get_name());
  render.set_collide_mask(CollideMask(ColMask::Render));
  if (use_color || use_opacity)
    render.set_color_scale(base_color);
  render.reparent_to(nodePath);
  if (!(render.is_empty()) && use_texture == true)
    SetTexture(strTexture);
  else
    std::cerr << "[MapObject][SetModel] Could not load model " << strModel << " for object '" << name << '\'' << std::endl;
}

void MapObject::SetTexture(const std::string& new_texture)
{
  strTexture = new_texture;
  if (!(render.is_empty()) && strTexture != "")
  {
    texture    = TexturePool::load_texture(TEXT_ROOT + strTexture);
    if (texture)
      render.set_texture(texture);
    else
      std::cerr << "[MapObject][SetTexture] Could not load texture " << strTexture << " for object '" << name << '\'' << std::endl;
  }
}

/*
 * Serialization
 */
void MapObject::Unserialize(Utils::Packet& packet)
{
  float  posX,   posY,   posZ;
  float  rotX,   rotY,   rotZ;
  float  scaleX, scaleY, scaleZ;
  char   inherits_floor = true;

  world = World::LoadingWorld;
  packet >> name >> strModel >> strTexture;
  packet >> posX >> posY >> posZ >> rotX >> rotY >> rotZ >> scaleX >> scaleY >> scaleZ;
  packet >> floor >> parent >> inherits_floor;
  this->inherits_floor = inherits_floor;
  if (blob_revision > 15)
  {
    float red, blue, green, alpha;

    packet >> use_texture >> use_color >> use_opacity;
    packet >> red >> blue >> green >> alpha;
    base_color = LColor(red, blue, green, alpha);
  }

  if (name == "")
    name = "name-not-found";

  if (world)
  {
    nodePath   = world->window->get_render().attach_new_node(name);
    nodePath.set_name(name);
    SetModel(strModel);
    nodePath.set_transparency(use_opacity ? TransparencyAttrib::M_alpha : TransparencyAttrib::M_none);
    nodePath.set_depth_offset(1);
    nodePath.set_two_sided(false);
    nodePath.set_hpr(rotX, rotY, rotZ);
    nodePath.set_scale(scaleX, scaleY, scaleZ);
    nodePath.set_pos(posX, posY, posZ);
    waypoints_root = nodePath.attach_new_node("waypoints");
    world->MapObjectChangeFloor(*this, floor);
  }

  UnserializeWaypoints(world, packet);
  collider.parent = nodePath;
  packet >> collider;
  InitializeCollideMask();
}

void MapObject::UnserializeWaypoints(World* world, Utils::Packet& packet)
{
  vector<int> waypoint_ids;

  packet >> waypoint_ids;
  if (world)
  {
    for_each(waypoint_ids.begin(), waypoint_ids.end(), [this, world](int id)
    {
      Waypoint* waypoint = world->GetWaypointFromId(id);

      if (waypoint)
      {
        waypoint->floor       = this->floor;
        waypoint->parent_path = this->render;
        waypoints.push_back(waypoint);
  #ifdef GAME_EDITOR
        if (!(waypoint->nodePath.is_empty()))
          waypoint->nodePath.reparent_to(waypoints_root);
  #endif
      }
    });
  }
}

void MapObject::Serialize(Utils::Packet& packet) const
{
  float  posX,   posY,   posZ;
  float  rotX,   rotY,   rotZ;
  float  scaleX, scaleY, scaleZ;

  posX   = nodePath.get_pos().get_x();
  posY   = nodePath.get_pos().get_y();
  posZ   = nodePath.get_pos().get_z();
  rotX   = nodePath.get_hpr().get_x();
  rotY   = nodePath.get_hpr().get_y();
  rotZ   = nodePath.get_hpr().get_z();
  scaleX = nodePath.get_scale().get_x();
  scaleY = nodePath.get_scale().get_y();
  scaleZ = nodePath.get_scale().get_z();
  packet << name << strModel << strTexture;
  packet << posX << posY << posZ << rotX << rotY << rotZ << scaleX << scaleY << scaleZ;
  packet << floor;
  packet << parent; // Revision #1
  packet << (char)inherits_floor;
  {
    packet << use_texture << use_color << use_opacity;
    packet << base_color.get_x() << base_color.get_y() << base_color.get_z() << base_color.get_w();
  } // # Revision 16
  {
    std::vector<int> waypoint_ids;

    waypoint_ids.resize(waypoints.size());
    for_each(waypoints.begin(), waypoints.end(), [&waypoint_ids](Waypoint* wp)
    {
      waypoint_ids.push_back(wp->id);
    });
    packet << waypoint_ids;
  } // #Revision2
  packet << collider;
}

void Collider::Serialize(Utils::Packet& packet) const
{
  float         posX,   posY,   posZ;
  float         rotX,   rotY,   rotZ;
  float         scaleX, scaleY, scaleZ;
  unsigned char _collider = (unsigned char)type;

  packet << _collider;
  if (type != NONE)
  {
    posX = node.get_pos().get_x(); rotX = node.get_hpr().get_x(); scaleX = node.get_scale().get_x();
    posY = node.get_pos().get_y(); rotY = node.get_hpr().get_y(); scaleY = node.get_scale().get_y();
    posZ = node.get_pos().get_z(); rotZ = node.get_hpr().get_z(); scaleZ = node.get_scale().get_z();
    packet << posX << posY << posZ << rotX << rotY << rotZ << scaleX << scaleY << scaleZ;
  }
}

void Collider::Unserialize(Utils::Packet& packet)
{
  float         posX,   posY,   posZ;
  float         rotX,   rotY,   rotZ;
  float         scaleX, scaleY, scaleZ;
  unsigned char _collider;

  packet >> _collider;
  if (_collider != NONE)
    packet >> posX >> posY >> posZ >> rotX >> rotY >> rotZ >> scaleX >> scaleY >> scaleZ;
  InitializeCollider((Type)_collider, LPoint3f(posX, posY, posZ), LPoint3f(scaleX, scaleY, scaleZ), LPoint3f(rotX, rotY, rotZ));
}

void Collider::InitializeCollider(Type type, LPoint3f position, LPoint3f scale, LPoint3f hpr)
{
  PT(CollisionNode)  node_ptr;
  PT(CollisionSolid) solid_ptr;

  this->type = type;
  switch (type)
  {
  default:
  case NONE:
    return ;
  case MODEL:
  case BOX:
      solid_ptr = new CollisionBox(LPoint3f(0, 0, 0), 1, 1, 1);
      break ;
  case SPHERE:
      solid_ptr = new CollisionSphere(LPoint3(0, 0, 0), 1);
      break ;
  }
  node_ptr = new CollisionNode("collision_node");
  node_ptr->add_solid(solid_ptr);
  node     = parent.attach_new_node(node_ptr);
  node.set_pos(position);
  node.set_scale(scale);
  node.set_hpr(hpr);
}

void MapObject::InitializeCollideMask(void)
{
  int flag     = GetObjectCollideMask();
  int col_flag = ColMask::FovBlocker;

  if (collider.type == Collider::MODEL)
    col_flag |= ColMask::CheckCollisionOnModel;
  if (waypoints.size() > 0)
    col_flag |= ColMask::WpPlane;
  nodePath.set_collide_mask(flag);
  collider.node.set_collide_mask(col_flag);
}

bool MapObject::IsCuttable(void) const
{
#ifndef GAME_EDITOR
  string name       = nodePath.get_name();
  string patterns[] = { "Wall", "wall", "Ceiling", "ceiling" };

  for (unsigned short i = 0 ; i < 4 ; ++i)
  {
    if (starts_with(name, patterns[i]))
      return (true);
  }
#endif
  return (false);
}

void MapObject::SetLight(WorldLight *light, bool is_active)
{
  if (light)
  {
    if (light->enabled == true && is_active == true)
      render.set_light(light->nodePath, light->priority);
    else
      render.set_light_off(light->nodePath);
    for_each(waypoints.begin(), waypoints.end(), [&light, is_active](Waypoint* waypoint)
    {
      waypoint->SetLight(light, is_active);
    });
  }
}
