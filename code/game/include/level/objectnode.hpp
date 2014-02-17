#ifndef  OBJECT_NODE_HPP
# define OBJECT_NODE_HPP

# include "globals.hpp"
# include <panda3d/pandaFramework.h>
# include <panda3d/pandaSystem.h>
# include <panda3d/texturePool.h>
# include <panda3d/animControlCollection.h>
# include "datatree.hpp"
# include "observatory.hpp"
# include "world/world.h"
# include "inventory.hpp"
# include "animatedobject.hpp"
# include "skill_target.hpp"

//HAIL MICROSOFT
#ifdef WIN32
static inline double round(double val)
{
    return floor(val + 0.5);
}
#endif

class Level;
class ObjectCharacter;

struct WaypointModifier
{
  WaypointModifier() : _collision_processed(0), _level(0), _waypointOccupied(0)
  {}

  virtual NodePath GetNodePath() const = 0;

  virtual void ProcessCollisions(void);
  void         UnprocessCollisions(void);
  virtual bool HasOccupiedWaypoint(void)      const { return (_waypointOccupied != 0); }
  int          GetOccupiedWaypointAsInt(void) const { return (_waypointOccupied ? _waypointOccupied->id : 0);  }
  Waypoint*    GetOccupiedWaypoint(void)      const { return (_waypointOccupied);      }
  void         SetOccupiedWaypoint(Waypoint* wp);

protected:
  void         WithdrawAllArcs(unsigned int id);
  void         WithdrawAllArcs(Waypoint* waypoint);
  void         WithdrawArc(unsigned int id1, unsigned int id2);
  void         WithdrawArc(Waypoint* first, Waypoint* second);

  unsigned short                  _collision_processed;
  Level*                          _level;
  Waypoint*                       _waypointOccupied;
  std::list<std::pair<int, int> > _waypointDisconnected;

private:
  struct WithdrawedArc
  {
    WithdrawedArc(Waypoint* first, Waypoint* second, Waypoint::ArcObserver* observer) : first(first), second(second), observer(observer) {}
    Waypoint              *first, *second;
    Waypoint::ArcObserver *observer;
  };
  typedef std::list<WithdrawedArc>        WithdrawedArcs;

  WithdrawedArcs                          _withdrawedArcs;
};

namespace ObjectTypes
{
  enum ObjectType
  {
    Character, Stair, Door, Shelf, Locker, Item, Other
  };
}

template<class C>
struct ObjectType2Code { enum { Type = ObjectTypes::ObjectType::Other }; };

class InstanceDynamicObject : public WaypointModifier, public AnimatedObject
{
  friend class SkillTarget;
public:
  static Sync::Signal<void (InstanceDynamicObject*)> ActionUse;
  static Sync::Signal<void (InstanceDynamicObject*)> ActionUseObjectOn;
  static Sync::Signal<void (InstanceDynamicObject*)> ActionUseSkillOn;
  static Sync::Signal<void (InstanceDynamicObject*)> ActionUseSpellOn;
  static Sync::Signal<void (InstanceDynamicObject*)> ActionTalkTo;

  struct Interaction
  {
    Interaction(const std::string& name, InstanceDynamicObject* instance, Sync::Signal<void (InstanceDynamicObject*)>* signal)
    : name(name), instance(instance), signal(signal) {}

    std::string                                         name;
    InstanceDynamicObject*                              instance;
    Sync::Signal<void (InstanceDynamicObject*)>* signal;
  };
  typedef std::list<Interaction> InteractionList;
  
  struct GoToData
  {
    Waypoint*              nearest;
    InstanceDynamicObject* objective;
    int                    max_distance;
    int                    min_distance;
  };
  
  InstanceDynamicObject(Level* level, DynamicObject* object);
  InstanceDynamicObject(void) : AnimatedObject(_window), _skill_target(this) {}
  virtual ~InstanceDynamicObject() {}

  virtual void       Load(Utils::Packet&);
  virtual void       Save(Utils::Packet&);

  virtual void       Run(float elapsedTime)
  {
    TaskAnimation();
  };

  bool               operator==(NodePath np)             const { return (_object->nodePath.is_ancestor_of(np)); }
  bool               operator==(const std::string& name) const { return (GetName() == name);                    }
  std::string        GetName(void)                       const { return (_object->nodePath.get_name());         }
  NodePath           GetNodePath(void)                   const { return (_object->nodePath);                    }
  InteractionList&   GetInteractions(void)                     { return (_interactions);                        }
  const std::string& GetDialog(void)                     const { return (_object->dialog);                      }
  DynamicObject*     GetDynamicObject(void)                    { return (_object);                              }
  virtual GoToData   GetGoToData(InstanceDynamicObject* character);
  
  template<class C>
  C*                 Get(void)
  {
    if (ObjectType2Code<C>::Type == _type)
      return (reinterpret_cast<C*>(this));
    return (0);
  }

  Sync::Signal<void (InstanceDynamicObject*)> AnimationEnded;

  virtual void             CallbackActionUse(InstanceDynamicObject* object) { ThatDoesNothing(); }
  virtual void             CallbackActionUseSkill(ObjectCharacter* object, const std::string& skill);

  void                     AddTextBox(const std::string& text, unsigned short r, unsigned short g, unsigned short b, float timeout = 5.f);

  void                     ResetAnimation(void)
  {
    AnimationEnded.DisconnectAll();
  }

protected:
  unsigned char            _type;
  DynamicObject*           _object;
  SkillTarget              _skill_target;

  // Interactions
  void                     ResetInteractions(void) { _interactions.clear(); }
  void                     ThatDoesNothing();

  InteractionList          _interactions;
  LPoint3                  _idle_size;

private:
  void CallbackAnimationEnded(void) { AnimationEnded.Emit(this); }
};


#endif