#pragma once

#include "Common.h"
#include "MapGrid.h"
#include "InformationManager.h"

namespace KoalaRunBot {
  enum class CombatSimEnemies {
    kAllEnemies,
    // ignore air enemies that can't shoot down
    kAntiGroundEnemies,
    // count only ground enemies that can shoot up
    kScourgeEnemies
  };

  class CombatSimulation {
    bool IncludeEnemy(CombatSimEnemies which, BWAPI::UnitType type) const;

  public:
    CombatSimulation() = default;

    void SetCombatUnits(const BWAPI::Unitset& my_units
                        , const BWAPI::Position& center
                        , int radius
                        , bool visible_only
                        , CombatSimEnemies which
    ) const;

    double SimulateCombat(bool meat_grinder) const;
  };
}
