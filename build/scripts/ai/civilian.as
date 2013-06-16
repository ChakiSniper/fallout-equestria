#include "general_pony.as"

Timer myTimer;
int   movCount     = 0;
int   initWaypoint = 0;

bool shieldCasted = false;

void main(Character@ self, float elapsedTime)
{
  Data       data_engine = level.GetDataEngine();
  Character@ player      = level.GetPlayer();

  if (data_engine["variables"]["Sterling"]["allied"].AsInt() == 1)
    ai_template_follow_char(self, player, elapsedTime);
}

Character@ currentTarget = null;

bool       Fleeing(Character@ self)
{
  CharacterList enemies = self.GetNearbyEnemies();

  if (enemies.Size() > 0)
  {
    Character@ other = enemies[0];
    int waypoint = self.GetFarthestWaypoint(other.AsObject());

    if (waypoint != self.GetCurrentWaypoint())
    {
      if (self.GetPathSize() == 0)
        self.GoTo(waypoint);
    }
  }
  return (false);
}

Character@ SelectTarget(Character@ self)
{
  Cout("Civilian Select Target");
  CharacterList         enemies  = self.GetNearbyEnemies();
  int                   nEnemies = enemies.Size();
  int                   it       = 0;
  Character@            bestMatch;

  while (it < nEnemies)
  {
    Character@ current = enemies[it];

    if (@bestMatch == null || current.GetHitPoints() < bestMatch.GetHitPoints())
      @bestMatch = @current;
    it++;
  }
  if (@bestMatch != null)
  {
    Cout("-> Selected enemy: " + bestMatch.GetName());
    Write("-> Selected enemy: " + bestMatch.GetName());
  }
  else
  {
    Cout("-> No enemy detected");
  }
  return (bestMatch);
}

void combat(Character@ self)
{
  Cout("Civilian in Combat mode");
  if (self.IsMoving())
    return ;

  if (self.GetHitPoints() < 10 && Fleeing(self))
    return ;

  if (@currentTarget == null || !(currentTarget.IsAlive()))
    @currentTarget = @SelectTarget(self);
  if (@currentTarget == null)
  {
    Cout("-> Civilian passing turn");
    level.NextTurn();
  }
  else
  {
    Cout("-> Civilian acting on an enemy");
    if (self.HasLineOfSight(currentTarget.AsObject()))
    {
      int   actionPoints = self.GetActionPoints();
      int   actionPointsCost;
      Item@ equiped1 = self.GetEquipedItem(0);
      Item@ equiped2 = self.GetEquipedItem(1);
      Data  data1    = equiped1.AsData();
      Data  data2    = equiped2.AsData();
      bool  suitable1,  suitable2;
      int   distance = currentTarget.GetDistance(self.AsObject());

      Item@  bestEquipedItem;
      int    bestAction = -1;

      int    nActions = data1["actions"].Count();
      int    cAction  = 0;
      Data   cData    = data1;
      Item@  cItem    = @equiped1;

      while (cAction < nActions)
      {
        Data action   = cData["actions"][cAction];

        // If it's a combative action
        if (action["combat"].AsInt() == 1)
        {
          // If we're in range
          if (action["range"].AsFloat() > distance)
          {
            bool setAsBest = true;

            // If there's an action to compare this one to
            if (bestAction >= 0)
            {
              Data dataBestAction   = bestEquipedItem.AsData()["actions"][bestAction];
              int  bestActionNShots = dataBestAction["ap-cost"].AsInt() / actionPoints;
              int  curActionNShots  = action["ap-cost"].AsInt()         / actionPoints;

              if (bestActionNShots * dataBestAction["damage"].AsInt() >= curActionNShots * action["damage"].AsInt())
                setAsBest = false;
            }
            if (setAsBest)
            {
              @bestEquipedItem = @cItem;
              bestAction       = cAction;
            }
          }
        }
        cAction++;
      }

      if (@bestEquipedItem != null)
      {
        actionPointsCost = bestEquipedItem.AsData()["actions"][bestAction]["ap-cost"].AsInt();
        if (actionPoints >= actionPointsCost)
          level.ActionUseWeaponOn(self, currentTarget, bestEquipedItem, bestAction);
        else
          level.NextTurn();
      }
      else
      {
        /*if (currentTarget.GetPathDistance(self.AsObject()) <= 1)
        {
          Cout("Path distance <= 1");
          level.NextTurn();
        }
        else*/
        {
          Cout("Civilian going to player");
          self.GoTo(level.GetPlayer().AsObject(), 1);
          self.TruncatePath(1);
        }
      }
    }
    else
    {
      Cout("Civilian going to player");
      self.GoTo(level.GetPlayer().AsObject(), 1);
      self.TruncatePath(1);
    }
  }
}
