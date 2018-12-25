#include "CombatSimulation.h"
#include "FAP.h"
#include "UnitUtil.h"

using namespace KoalaRunBot;

bool CombatSimulation::IncludeEnemy(const CombatSimEnemies which, BWAPI::UnitType type) const {
  if (which == CombatSimEnemies::kAntiGroundEnemies) {
    // Ground enemies plus air enemies that can shoot down.
    return
      !type.isFlyer() ||
      UnitUtil::GetGroundWeapon(type) != BWAPI::WeaponTypes::None;
  }

  if (which == CombatSimEnemies::kScourgeEnemies) {
    // Only ground enemies that can shoot up.
    // The scourge will take on air enemies no matter what.
    return
      !type.isFlyer() &&
      UnitUtil::GetAirWeapon(type) != BWAPI::WeaponTypes::None;
  }

  // AllEnemies.
  return true;
}

// Set up the combat sim state based on the given friendly units and the enemy units within a given circle.
void CombatSimulation::SetCombatUnits(const BWAPI::Unitset& my_units
                                      , const BWAPI::Position& center
                                      , const int radius
                                      , const bool visible_only
                                      , const CombatSimEnemies which) const {
  fap.clearState();

  if (Config::Debug::DrawCombatSimulationInfo) {
    BWAPI::Broodwar->drawCircleMap(center.x, center.y, 6, BWAPI::Colors::Red, true);
    BWAPI::Broodwar->drawCircleMap(center.x, center.y, radius, BWAPI::Colors::Red);
  }

  // Work around a bug in mutalisks versus spore colony: It believes that any 2 mutalisks
  // can beat a spore. 6 is a better estimate. So for each spore in the fight, we compensate
  // by dropping 5 mutalisks.
  // TODO fix the bug and remove the workaround
  // Compensation only applies when visibleOnly is false.
  auto compensatory_mutalisks = 0;

  // Add enemy units.
  if (visible_only) {
    // Only units that we can see right now.
    BWAPI::Unitset enemyCombatUnits;
    MapGrid::Instance().GetUnits(enemyCombatUnits, center, radius, false, true);
    for (const auto unit : enemyCombatUnits) {
      if (unit->getHitPoints() > 0 && UnitUtil::IsCombatSimUnit(unit) && IncludeEnemy(which, unit->getType())) {
        fap.addIfCombatUnitPlayer2(unit);
        if (Config::Debug::DrawCombatSimulationInfo) {
          BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Orange, true);
        }
      }
    }

    // Also static defense that is out of sight.
    // NOTE getNearbyForce() includes completed units and uncompleted buildings which are out of vision.
    std::vector<UnitInfo> enemyStaticDefense;
    InformationManager::Instance().getNearbyForce(enemyStaticDefense, center, BWAPI::Broodwar->enemy(), radius);
    for (const UnitInfo& ui : enemyStaticDefense) {
      if (ui.type.isBuilding() && !ui.unit->isVisible() && IncludeEnemy(which, ui.type)) {
        fap.addIfCombatUnitPlayer2(ui);
        if (Config::Debug::DrawCombatSimulationInfo) {
          BWAPI::Broodwar->drawCircleMap(ui.lastPosition, 3, BWAPI::Colors::Orange, true);
        }
      }
    }
  }
  else {
    // All known enemy units, according to their most recently seen position.
    // Skip if goneFromLastPosition, which means the last position was seen and the unit wasn't there.
    std::vector<UnitInfo> enemyCombatUnits;
    InformationManager::Instance().getNearbyForce(enemyCombatUnits, center, BWAPI::Broodwar->enemy(), radius);
    for (const UnitInfo& ui : enemyCombatUnits) {
      // The check is careful about seen units and assumes that unseen units are completed and powered.
      if (ui.lastHealth > 0 &&
        (ui.unit->exists() || ui.lastPosition.isValid() && !ui.goneFromLastPosition) &&
        (ui.unit->exists() ? UnitUtil::IsCombatSimUnit(ui.unit) : UnitUtil::IsCombatSimUnit(ui.type)) &&
        IncludeEnemy(which, ui.type)) {
        fap.addIfCombatUnitPlayer2(ui);
        if (ui.type == BWAPI::UnitTypes::Zerg_Spore_Colony) {
          compensatory_mutalisks += 5;
        }
        if (Config::Debug::DrawCombatSimulationInfo) {
          BWAPI::Broodwar->drawCircleMap(ui.lastPosition, 3, BWAPI::Colors::Red, true);
        }
      }
    }
  }

  // Add our units.
  // Add them from the input set. Other units have been given other instructions
  // and may not cooperate in the fight, so skip them.
  for (const auto unit : my_units) {
    if (UnitUtil::IsCombatSimUnit(unit)) {
      if (compensatory_mutalisks > 0 && unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk) {
        --compensatory_mutalisks;
      }
      else {
        fap.addIfCombatUnitPlayer1(unit);
        if (Config::Debug::DrawCombatSimulationInfo) {
          BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Green, true);
        }
      }
    }
  }

  /* Add our units by location.
  BWAPI::Unitset ourCombatUnits;
  MapGrid::Instance().getUnits(ourCombatUnits, center, radius, true, false);
  for (const auto unit : ourCombatUnits)
  {
    if (UnitUtil::IsCombatSimUnit(unit))
    {
      if (compensatoryMutalisks > 0 && unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
      {
        --compensatoryMutalisks;
        continue;
      }
      fap.addIfCombatUnitPlayer1(unit);
      if (Config::Debug::DrawCombatSimulationInfo)
      {
        BWAPI::Broodwar->drawCircleMap(unit->getPosition(), 3, BWAPI::Colors::Green, true);
      }
    }
  }
  */
}

// Simulate combat and return the result as a score. Score >= 0 means you win.
double CombatSimulation::SimulateCombat(const bool meat_grinder) const {
  const auto start_scores = fap.playerScores();
  if (start_scores.second == 0) {
    // No enemies. We can stop early.
    return 0.0;
  }

  fap.simulate();
  const auto end_scores = fap.playerScores();

  const auto myLosses = start_scores.first - end_scores.first;
  const auto yourLosses = start_scores.second - end_scores.second;

  //BWAPI::Broodwar->printf("  p1 %d - %d = %d, p2 %d - %d = %d  ==>  %d",
  //	startScores.first, endScores.first, myLosses,
  //	startScores.second, endScores.second, yourLosses,
  //	(myLosses == 0) ? yourLosses : endScores.first - endScores.second);

  // If we came out ahead, call it a win regardless.
  // if (yourLosses > myLosses)
  // The most conservative case: If we lost nothing, it's a win (since a draw counts as a win).
  if (myLosses == 0) {
    return double(yourLosses);
  }

  // Be more aggressive if requested. The setting is on the squad.
  // NOTE This tested poorly. I recommend against using it as it stands. - Jay
  if (meat_grinder) {
    // We only need to do a limited amount of damage to "win".
    BWAPI::Broodwar->printf("  meat grinder result = ", 3 * yourLosses - myLosses);

    // Call it a victory if we took down at least this fraction of the enemy army.
    return double(3 * yourLosses - myLosses);
  }

  // Winner is the side with smaller losses.
  // return double(yourLosses - myLosses);

  // Original scoring: Winner is whoever has more stuff left.
  return double(end_scores.first - end_scores.second);

  /*
  if (Config::Debug::DrawCombatSimulationInfo)
  {
    BWAPI::Broodwar->drawTextScreen(150, 200, "%cCombat sim: us %c%d %c/ them %c%d %c= %c%g",
      white, orange, endScores.first, white, orange, endScores.second, white,
      score >= 0.0 ? green : red, score);
  }
  */
}
