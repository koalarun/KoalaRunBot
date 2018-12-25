#pragma once

#include "Common.h"

#include "BuildingManager.h"
#include "CombatCommander.h"
#include "InformationManager.h"
#include "MapGrid.h"
#include "OpponentModel.h"
#include "ProductionManager.h"
#include "ScoutManager.h"
#include "StrategyManager.h"
#include "TimerManager.h"
#include "WorkerManager.h"

namespace KoalaRunBot {
  class The;

  // class UnitToAssign {
  // public:
  //
  //   BWAPI::Unit unit_;
  //   bool is_assigned_;
  //
  //   UnitToAssign(const BWAPI::Unit u) {
  //     unit_ = u;
  //     is_assigned_ = false;
  //   }
  // };

  class GameCommander {
    The& the_;
    CombatCommander& combat_commander_;
    TimerManager timer_manager_;

    BWAPI::Unitset valid_units_;
    BWAPI::Unitset combat_units_;
    BWAPI::Unitset scout_units_;

    int surrender_time_; // for giving up early

    int initial_scout_time_; // 0 until a scouting worker is assigned

    void AssignUnit(BWAPI::Unit unit, BWAPI::Unitset& set);
    bool isAssigned(BWAPI::Unit unit) const;

    bool SurrenderMonkey();

    BWAPI::Unit GetAnyFreeWorker();

  public:

    GameCommander();
    ~GameCommander() = default;;

    void Update();

    void HandleUnitAssignments();
    void setValidUnits();
    void setScoutUnits();
    void setCombatUnits();

    void releaseOverlord(BWAPI::Unit overlord); // zerg scouting overlord

    //void GoScout();
    int getScoutTime() const { return initial_scout_time_; };

    void DrawDebugInterface();
    void DrawGameInformation(int x, int y);
    void DrawUnitOrders();

    void OnUnitShow(BWAPI::Unit unit);
    void OnUnitHide(BWAPI::Unit unit);
    void OnUnitCreate(BWAPI::Unit unit);
    void OnUnitComplete(BWAPI::Unit unit);
    void OnUnitRenegade(BWAPI::Unit unit);
    void OnUnitDestroy(BWAPI::Unit unit);
    void OnUnitMorph(BWAPI::Unit unit);

    static GameCommander& Instance();
  };

}
