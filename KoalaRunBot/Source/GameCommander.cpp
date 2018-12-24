#include "Bases.h"
#include "GameCommander.h"
#include "MapTools.h"
#include "OpponentModel.h"
#include "UnitUtil.h"

using namespace KoalaRunBot;

GameCommander::GameCommander()
  : the_(The::Root())
    , combat_commander_(CombatCommander::Instance())
    , surrender_time_(0)
    , initial_scout_time_(0) {}

void GameCommander::Update() {
  timer_manager_.startTimer(TimerManager::Total);

  // populate the unit vectors we will pass into various managers
  HandleUnitAssignments();

  // Decide whether to give up early. Implements config option SurrenderWhenHopeIsLost.
  if (SurrenderMonkey()) {
    surrender_time_ = BWAPI::Broodwar->getFrameCount();
    BWAPI::Broodwar->printf("gg");
  }
  if (surrender_time_ != 0) {
    if (BWAPI::Broodwar->getFrameCount() - surrender_time_ >= 36) // 36 frames = 1.5 game seconds
    {
      BWAPI::Broodwar->leaveGame();
    }
    return;
  }

  // -- Managers that gather inforation. --

  timer_manager_.startTimer(TimerManager::InformationManager);
  InformationManager::Instance().update();
  timer_manager_.stopTimer(TimerManager::InformationManager);

  timer_manager_.startTimer(TimerManager::MapGrid);
  MapGrid::Instance().update();
  timer_manager_.stopTimer(TimerManager::MapGrid);

  timer_manager_.startTimer(TimerManager::OpponentModel);
  OpponentModel::Instance().update();
  timer_manager_.stopTimer(TimerManager::OpponentModel);

  // -- Managers that act on information. --

  timer_manager_.startTimer(TimerManager::Search);
  BOSSManager::Instance().update(35 - timer_manager_.getMilliseconds());
  timer_manager_.stopTimer(TimerManager::Search);

  timer_manager_.startTimer(TimerManager::Worker);
  WorkerManager::Instance().update();
  timer_manager_.stopTimer(TimerManager::Worker);

  timer_manager_.startTimer(TimerManager::Production);
  ProductionManager::Instance().update();
  timer_manager_.stopTimer(TimerManager::Production);

  timer_manager_.startTimer(TimerManager::Building);
  BuildingManager::Instance().update();
  timer_manager_.stopTimer(TimerManager::Building);

  timer_manager_.startTimer(TimerManager::Combat);
  combat_commander_.update(combat_units_);
  timer_manager_.stopTimer(TimerManager::Combat);

  timer_manager_.startTimer(TimerManager::Scout);
  ScoutManager::Instance().update();
  timer_manager_.stopTimer(TimerManager::Scout);

  // Execute micro commands gathered above. Do this at the end of the frame.
  timer_manager_.startTimer(TimerManager::Micro);
  the_.micro_.update();
  timer_manager_.stopTimer(TimerManager::Micro);

  timer_manager_.stopTimer(TimerManager::Total);

  DrawDebugInterface();
}

void GameCommander::DrawDebugInterface() {
  InformationManager::Instance().drawExtendedInterface();
  InformationManager::Instance().drawUnitInformation(425, 30);
  Bases::Instance().drawBaseInfo();
  Bases::Instance().drawBaseOwnership(575, 30);
  BuildingManager::Instance().drawBuildingInformation(200, 50);
  BuildingPlacer::Instance().drawReservedTiles();
  ProductionManager::Instance().drawProductionInformation(30, 60);
  BOSSManager::Instance().drawSearchInformation(490, 100);
  BOSSManager::Instance().drawStateInformation(250, 0);
  MapTools::Instance().drawHomeDistances();

  combat_commander_.drawSquadInformation(170, 70);
  combat_commander_.drawCombatSimInformation();
  timer_manager_.drawModuleTimers(490, 215);
  DrawGameInformation(4, 1);

  DrawUnitOrders();

  // draw position of mouse cursor
  if (Config::Debug::DrawMouseCursorInfo) {
    const auto mouse_x = BWAPI::Broodwar->getMousePosition().x + BWAPI::Broodwar->getScreenPosition().x;
    const auto mouse_y = BWAPI::Broodwar->getMousePosition().y + BWAPI::Broodwar->getScreenPosition().y;
    BWAPI::Broodwar->drawTextMap(mouse_x + 20, mouse_y, " %d %d", mouse_x, mouse_y);
  }
}

void GameCommander::DrawGameInformation(int x, int y) {
  if (!Config::Debug::DrawGameInfo) {
    return;
  }

  const OpponentModel::OpponentSummary& summary = OpponentModel::Instance().getSummary();
  BWAPI::Broodwar->drawTextScreen(x, y, "%c%s %cv %c%s %c%d-%d",
                                  BWAPI::Broodwar->self()->getTextColor(), BWAPI::Broodwar->self()->getName().c_str(),
                                  kWhite,
                                  BWAPI::Broodwar->enemy()->getTextColor(), BWAPI::Broodwar->enemy()->getName().c_str(),
                                  kWhite, summary.totalWins, summary.totalGames - summary.totalWins);
  y += 12;

  const std::string& openingGroup = StrategyManager::Instance().getOpeningGroup();
  const auto openingInfoIt = summary.openingInfo.find(Config::Strategy::StrategyName);
  const int wins = openingInfoIt == summary.openingInfo.end()
                     ? 0
                     : openingInfoIt->second.sameWins + openingInfoIt->second.otherWins;
  const int games = openingInfoIt == summary.openingInfo.end()
                      ? 0
                      : openingInfoIt->second.sameGames + openingInfoIt->second.otherGames;
  const bool gas_steal = OpponentModel::Instance().getRecommendGasSteal() || ScoutManager::Instance().wantGasSteal();
  BWAPI::Broodwar->drawTextScreen(x, y, "\x03%s%s%s%s %c%d-%d",
                                  Config::Strategy::StrategyName.c_str(),
                                  !openingGroup.empty() ? (" (" + openingGroup + ")").c_str() : "",
                                  gas_steal ? " + steal gas" : "",
                                  Config::Strategy::FoundEnemySpecificStrategy ? " - enemy specific" : "",
                                  kWhite, wins, games - wins);
  BWAPI::Broodwar->setTextSize();
  y += 12;

  std::string expect;
  std::string enemyPlanString;
  if (OpponentModel::Instance().getEnemyPlan() == OpeningPlan::Unknown &&
    OpponentModel::Instance().getExpectedEnemyPlan() != OpeningPlan::Unknown) {
    if (OpponentModel::Instance().getEnemySingleStrategy()) {
      expect = "surely ";
    }
    else {
      expect = "expect ";
    }
    enemyPlanString = OpponentModel::Instance().getExpectedEnemyPlanString();
  }
  else {
    enemyPlanString = OpponentModel::Instance().getEnemyPlanString();
  }
  BWAPI::Broodwar->drawTextScreen(x, y, "%cOpp Plan %c%s%c%s", kWhite, kOrange, expect.c_str(), kYellow,
                                  enemyPlanString.c_str());
  y += 12;

  std::string island;
  if (Bases::Instance().isIslandStart()) {
    island = " (island)";
  }
  BWAPI::Broodwar->drawTextScreen(x, y, "%c%s%c%s", kYellow, BWAPI::Broodwar->mapFileName().c_str(), kOrange,
                                  island.c_str());
  BWAPI::Broodwar->setTextSize();
  y += 12;

  const auto frame = BWAPI::Broodwar->getFrameCount();
  BWAPI::Broodwar->drawTextScreen(x, y, "\x04%d %2u:%02u mean %.1fms max %.1fms",
                                  frame,
                                  int(frame / (23.8 * 60)),
                                  int(frame / 23.8) % 60,
                                  timer_manager_.getMeanMilliseconds(),
                                  timer_manager_.getMaxMilliseconds());

  /*
  // latency display
  y += 12;
  BWAPI::Broodwar->drawTextScreen(x + 50, y, "\x04%d max %d",
    BWAPI::Broodwar->getRemainingLatencyFrames(),
    BWAPI::Broodwar->getLatencyFrames());
  */
}

void GameCommander::DrawUnitOrders() {
  if (!Config::Debug::DrawUnitOrders) {
    return;
  }

  for (const auto unit : BWAPI::Broodwar->getAllUnits()) {
    if (!unit->getPosition().isValid()) {
      continue;
    }

    std::string extra;
    if (unit->getType() == BWAPI::UnitTypes::Zerg_Egg ||
      unit->getType() == BWAPI::UnitTypes::Zerg_Cocoon ||
      unit->getType().isBuilding() && !unit->isCompleted()) {
      extra = unit->getBuildType().getName();
    }
    else if (unit->isTraining() && !unit->getTrainingQueue().empty()) {
      extra = unit->getTrainingQueue()[0].getName();
    }
    else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode ||
      unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode) {
      extra = unit->getType().getName();
    }
    else if (unit->isResearching()) {
      extra = unit->getTech().getName();
    }
    else if (unit->isUpgrading()) {
      extra = unit->getUpgrade().getName();
    }

    int x = unit->getPosition().x - 8;
    int y = unit->getPosition().y - 2;
    if (!extra.empty()) {
      BWAPI::Broodwar->drawTextMap(x, y, "%c%s", kYellow, extra.c_str());
    }
    if (unit->getOrder() != BWAPI::Orders::Nothing) {
      BWAPI::Broodwar->drawTextMap(x, y + 10, "%c%d %c%s", kWhite, unit->getID(), kCyan,
                                   unit->getOrder().getName().c_str());
    }
  }
}

// assigns units to various managers
void GameCommander::HandleUnitAssignments() {
  valid_units_.clear();
  combat_units_.clear();
  // Don't clear the scout units.

  // filter our units for those which are valid and usable
  setValidUnits();

  // set each type of unit
  setScoutUnits();
  setCombatUnits();
}

bool GameCommander::isAssigned(BWAPI::Unit unit) const {
  return combat_units_.contains(unit) || scout_units_.contains(unit);
}

// validates units as usable for distribution to various managers
void GameCommander::setValidUnits() {
  for (const auto unit : BWAPI::Broodwar->self()->getUnits()) {
    if (UnitUtil::IsValidUnit(unit)) {
      valid_units_.insert(unit);
    }
  }
}

void GameCommander::setScoutUnits() {
  // If we're zerg, assign the first overlord to scout.
  if (BWAPI::Broodwar->getFrameCount() == 0 &&
    BWAPI::Broodwar->self()->getRace() == BWAPI::Races::Zerg) {
    for (const auto unit : BWAPI::Broodwar->self()->getUnits()) {
      if (unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) {
        ScoutManager::Instance().setOverlordScout(unit);
        AssignUnit(unit, scout_units_);
        break;
      }
    }
  }

  // Send a scout worker if we haven't yet and should.
  if (!initial_scout_time_) {
    if (ScoutManager::Instance().shouldScout() && !Bases::Instance().isIslandStart()) {
      const BWAPI::Unit worker_scout = GetAnyFreeWorker();

      // If we find a worker, make it the scout unit.
      if (worker_scout) {
        ScoutManager::Instance().setWorkerScout(worker_scout);
        AssignUnit(worker_scout, scout_units_);
        initial_scout_time_ = BWAPI::Broodwar->getFrameCount();
      }
    }
  }
}

// Set combat units to be passed to CombatCommander.
void GameCommander::setCombatUnits() {
  for (const auto unit : valid_units_) {
    if (!isAssigned(unit) && (UnitUtil::IsCombatUnit(unit) || unit->getType().isWorker())) {
      AssignUnit(unit, combat_units_);
    }
  }
}

// Release the zerg scouting overlord for other duty.
void GameCommander::releaseOverlord(BWAPI::Unit overlord) {
  scout_units_.erase(overlord);
}

void GameCommander::OnUnitShow(BWAPI::Unit unit) {
  InformationManager::Instance().onUnitShow(unit);
  WorkerManager::Instance().onUnitShow(unit);
}

void GameCommander::OnUnitHide(BWAPI::Unit unit) {
  InformationManager::Instance().onUnitHide(unit);
}

void GameCommander::OnUnitCreate(BWAPI::Unit unit) {
  InformationManager::Instance().onUnitCreate(unit);
}

void GameCommander::OnUnitComplete(BWAPI::Unit unit) {
  InformationManager::Instance().onUnitComplete(unit);
}

void GameCommander::OnUnitRenegade(BWAPI::Unit unit) {
  InformationManager::Instance().onUnitRenegade(unit);
}

void GameCommander::OnUnitDestroy(BWAPI::Unit unit) {
  ProductionManager::Instance().onUnitDestroy(unit);
  WorkerManager::Instance().onUnitDestroy(unit);
  InformationManager::Instance().onUnitDestroy(unit);
}

void GameCommander::OnUnitMorph(BWAPI::Unit unit) {
  InformationManager::Instance().onUnitMorph(unit);
  WorkerManager::Instance().onUnitMorph(unit);
}

// Used only to choose a worker to scout.
BWAPI::Unit GameCommander::GetAnyFreeWorker() {
  for (const auto unit : valid_units_) {
    if (unit->getType().isWorker() &&
      !isAssigned(unit) &&
      WorkerManager::Instance().isFree(unit) &&
      !unit->isCarryingMinerals() &&
      !unit->isCarryingGas() &&
      unit->getOrder() != BWAPI::Orders::MiningMinerals) {
      return unit;
    }
  }

  return nullptr;
}

void GameCommander::AssignUnit(const BWAPI::Unit unit, BWAPI::Unitset& set) {
  if (scout_units_.contains(unit)) {
    scout_units_.erase(unit);
  }
  else if (combat_units_.contains(unit)) {
    combat_units_.erase(unit);
  }

  set.insert(unit);
}

// Decide whether to give up early. See config option SurrenderWhenHopeIsLost.
bool GameCommander::SurrenderMonkey() {
  if (!Config::Strategy::SurrenderWhenHopeIsLost) {
    return false;
  }

  // Only check once every five seconds. No hurry to give up.
  if (BWAPI::Broodwar->getFrameCount() % (5 * 24) != 0) {
    return false;
  }

  // Surrender if all conditions are met:
  // 1. We don't have the cash to make a worker.
  // 2. We have no unit that can attack.
  // 3. The enemy has at least one visible unit that can destroy buildings.
  // Terran does not float buildings, so we check whether the enemy can attack ground.

  // 1. Our cash.
  if (BWAPI::Broodwar->self()->minerals() >= 50) {
    return false;
  }

  // 2. Our units.
  for (const auto unit : valid_units_) {
    if (unit->canAttack()) {
      return false;
    }
  }

  // 3. Enemy units.
  bool safe = true;
  for (const auto unit : BWAPI::Broodwar->enemy()->getUnits()) {
    if (unit->isVisible() && UnitUtil::CanAttackGround(unit)) {
      safe = false;
      break;
    }
  }
  if (safe) {
    return false;
  }

  // Surrender monkey says surrender!
  return true;
}

GameCommander& GameCommander::Instance() {
  static GameCommander instance;
  return instance;
}
