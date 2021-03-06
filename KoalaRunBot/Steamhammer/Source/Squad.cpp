#include "Squad.h"

#include "Bases.h"
#include "CombatSimulation.h"
#include "MapTools.h"
#include "Micro.h"
#include "Random.h"
#include "ScoutManager.h"
#include "StrategyManager.h"
#include "The.h"
#include "UnitUtil.h"

using namespace KoalaRunBot;

Squad::Squad()
  : the(The::Root())
    , _name("Default")
    , _combatSquad(false)
    , _combatSimRadius(Config::Micro::CombatSimRadius)
    , _fightVisibleOnly(false)
    , _meatgrinder(false)
    , _hasAir(false)
    , _hasGround(false)
    , _canAttackAir(false)
    , _canAttackGround(false)
    , _attackAtMax(false)
    , _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(0)
    , _lastScore(0.0) {
  int a = 10; // work around linker error
}

// A "combat" squad is any squad except the Idle squad, which is full of workers
// (and possibly unused units like unassigned overlords).
// The usual work of workers is managed by WorkerManager. If we put workers into
// another squad, we have to notify WorkerManager.
Squad::Squad(const std::string& name, SquadOrder order, size_t priority)
  : the(The::Root())
    , _name(name)
    , _combatSquad(name != "Idle" && name != "Overlord")
    , _combatSimRadius(Config::Micro::CombatSimRadius)
    , _fightVisibleOnly(false)
    , _meatgrinder(false)
    , _hasAir(false)
    , _hasGround(false)
    , _canAttackAir(false)
    , _canAttackGround(false)
    , _attackAtMax(false)
    , _lastRetreatSwitch(0)
    , _lastRetreatSwitchVal(false)
    , _priority(priority)
    , _lastScore(0.0) {
  setSquadOrder(order);
}

Squad::~Squad() {
  clear();
}

void Squad::update() {
  // update all necessary unit information within this squad
  updateUnits();

  if (_units.empty()) {
    return;
  }

  // This is for the Overlord squad.
  // Overlords as combat squad detectors are managed by _microDetectors.
  _microOverlords.update();

  // If this is a worker squad, there is nothing more to do.
  if (!_combatSquad) {
    return;
  }

  _microHighTemplar.update();

  if (_order.getType() == SquadOrderTypes::Load) {
    loadTransport();
    return;
  }

  if (_order.getType() == SquadOrderTypes::Drop) {
    _microTransports.update();
    // And fall through to let the rest of the drop squad fight.
  }

  // Maybe stim marines and firebats.
  stimIfNeeded();

  // Detectors.
  BWAPI::Unit vanguard = unitClosestToEnemy(_units);
  _microDetectors.setUnitClosestToEnemy(vanguard);
  _microDetectors.setSquadSize(_units.size());
  _microDetectors.go();

  // Don't cluster detectors or transports.
  BWAPI::Unitset unitsToCluster = _units;
  for (BWAPI::Unit unit : _units) {
    if (unit->getType().isDetector() || unit->getType().spaceProvided() > 0) {
      unitsToCluster.erase(unit);
    }
  }
  the.ops_boss_.Cluster(unitsToCluster, _clusters);

  for (UnitCluster& cluster : _clusters) {
    setClusterStatus(cluster);
  }

  // It gets slow in late game when there are many clusters, so cut down the update frequency.
  const int nPhases = std::max(1, std::min(5, int(_clusters.size() / 6)));
  int phase = BWAPI::Broodwar->getFrameCount() % nPhases;
  for (const UnitCluster& cluster : _clusters) {
    if (phase == 0) {
      clusterCombat(cluster);
    }
    phase = (phase + 1) % nPhases;
  }
}

// Set cluster status and take non-combat cluster actions.
void Squad::setClusterStatus(UnitCluster& cluster) {
  // Cases where the cluster can't get into a fight.
  if (noCombatUnits(cluster)) {
    if (joinUp(cluster)) {
      cluster.status_ = ClusterStatus::kAdvance;
      _regroupStatus = kYellow + std::string("Join up");
    }
    else {
      // Can't join another cluster. Move back to base.
      cluster.status_ = ClusterStatus::kRegroup;
      moveCluster(cluster, finalRegroupPosition());
      _regroupStatus = kRed + std::string("Fall back");
    }
  }
  else if (notNearEnemy(cluster)) {
    cluster.status_ = ClusterStatus::kAdvance;
    if (joinUp(cluster)) {
      _regroupStatus = kYellow + std::string("Join up");
    }
    else {
      // Move toward the order position.
      moveCluster(cluster, _order.getPosition());
      _regroupStatus = kYellow + std::string("Advance");
    }
    drawCluster(cluster);
  }
  else {
    // Cases where the cluster might get into a fight.
    if (needsToRegroup(cluster)) {
      cluster.status_ = ClusterStatus::kRegroup;
    }
    else {
      cluster.status_ = ClusterStatus::kAttack;
    }
  }

  drawCluster(cluster);
}

// Take cluster combat actions. These can depend on the status of other clusters.
void Squad::clusterCombat(const UnitCluster& cluster) {
  if (cluster.status_ == ClusterStatus::kRegroup) {
    // Regroup, aka retreat. Only fighting units care about regrouping.
    BWAPI::Position regroupPosition = calcRegroupPosition(cluster);

    if (Config::Debug::DrawClusters) {
      BWAPI::Broodwar->drawLineMap(cluster.center_, regroupPosition, BWAPI::Colors::Purple);
    }

    _microAirToAir.regroup(regroupPosition, cluster);
    _microMelee.regroup(regroupPosition, cluster);
    //_microMutas.regroup(regroupPosition);
    _microRanged.regroup(regroupPosition, cluster);
    _microScourge.regroup(regroupPosition, cluster);
    _microTanks.regroup(regroupPosition, cluster);
  }
  else if (cluster.status_ == ClusterStatus::kAttack) {
    // No need to regroup. Execute micro.
    _microAirToAir.execute(cluster);
    _microMelee.execute(cluster);
    //_microMutas.execute(cluster);
    _microRanged.execute(cluster);
    _microScourge.execute(cluster);
    _microTanks.execute(cluster);
  }

  // Lurkers never regroup, always execute their order.
  // NOTE It is because regrouping works poorly. It retreats and unburrows them too often.
  _microLurkers.execute(cluster);

  // The remaining non-combat micro managers try to keep units near the front line.
  static int defilerPhase = 0;
  defilerPhase = (defilerPhase + 1) % 8;
  if (defilerPhase == 3) {
    BWAPI::Unit vanguard = unitClosestToEnemy(cluster.units_);

    // Defilers.
    _microDefilers.updateMovement(cluster.center_, vanguard);

    // Medics.
    BWAPI::Position medicGoal = vanguard && vanguard->getPosition().isValid()
                                  ? vanguard->getPosition()
                                  : cluster.center_;
    _microMedics.update(cluster, medicGoal);
  }
  else if (defilerPhase == 5) {
    BWAPI::Unit vanguard = unitClosestToEnemy(cluster.units_);

    _microDefilers.updateSwarm();
  }
  else if (defilerPhase == 7) {
    BWAPI::Unit vanguard = unitClosestToEnemy(cluster.units_);

    _microDefilers.updatePlague();
  }
}

// The cluster has no units which can fight.
// It should try to join another cluster, or else retreat to base.
bool Squad::noCombatUnits(const UnitCluster& cluster) const {
  for (BWAPI::Unit unit : cluster.units_) {
    if (UnitUtil::CanAttackGround(unit) || UnitUtil::CanAttackAir(unit)) {
      return false;
    }
  }
  return true;
}

// The cluster has no enemies nearby.
// It tries to join another cluster, or advance toward the goal.
bool Squad::notNearEnemy(const UnitCluster& cluster) {
  for (BWAPI::Unit unit : cluster.units_) {
    if (_nearEnemy[unit]) {
      return false;
    }
  }
  return true;
}

// Try to merge this cluster with a nearby one. Return true for success.
bool Squad::joinUp(const UnitCluster& cluster) {
  if (_clusters.size() < 2) {
    // Nobody to join up with.
    return false;
  }

  // Move toward the closest other cluster which is closer to the goal.
  int bestDistance = 99999;
  const UnitCluster* bestCluster = nullptr;

  for (const UnitCluster& otherCluster : _clusters) {
    if (cluster.center_ != otherCluster.center_ &&
      cluster.center_.getApproxDistance(_order.getPosition()) >= otherCluster
                                                                 .center_.getApproxDistance(_order.getPosition())) {
      int dist = cluster.center_.getApproxDistance(otherCluster.center_);
      if (dist < bestDistance) {
        bestDistance = dist;
        bestCluster = &otherCluster;
      }
    }
  }

  if (bestCluster) {
    moveCluster(cluster, bestCluster->center_);
    return true;
  }

  return false;
}

// Move toward the given position.
void Squad::moveCluster(const UnitCluster& cluster, const BWAPI::Position& destination) {
  for (BWAPI::Unit unit : cluster.units_) {
    if (!UnitUtil::MobilizeUnit(unit)) {
      the.micro_.Move(unit, destination);
    }
  }
}

bool Squad::isEmpty() const {
  return _units.empty();
}

size_t Squad::getPriority() const {
  return _priority;
}

void Squad::setPriority(const size_t& priority) {
  _priority = priority;
}

void Squad::updateUnits() {
  setAllUnits();
  setNearEnemyUnits();
  addUnitsToMicroManagers();
}

// Clean up the _units vector.
// Also notice and remember a few facts about the members of the squad.
// Note: Some units may be loaded in a bunker or transport and cannot accept orders.
//       Check unit->isLoaded() before issuing orders.
void Squad::setAllUnits() {
  _hasAir = false;
  _hasGround = false;
  _canAttackAir = false;
  _canAttackGround = false;

  BWAPI::Unitset goodUnits;
  for (const auto unit : _units) {
    if (UnitUtil::IsValidUnit(unit)) {
      goodUnits.insert(unit);

      if (unit->isFlying()) {
        if (!unit->getType().isDetector()) // mobile detectors don't count
        {
          _hasAir = true;
        }
      }
      else {
        _hasGround = true;
      }
      if (UnitUtil::CanAttackAir(unit)) {
        _canAttackAir = true;
      }
      if (UnitUtil::CanAttackGround(unit)) {
        _canAttackGround = true;
      }
    }
  }
  _units = goodUnits;
}

void Squad::setNearEnemyUnits() {
  _nearEnemy.clear();

  for (const auto unit : _units) {
    if (!unit->getPosition().isValid()) // excludes loaded units
    {
      continue;
    }

    _nearEnemy[unit] = unitNearEnemy(unit);

    if (Config::Debug::DrawSquadInfo) {
      if (_nearEnemy[unit]) {
        int left = unit->getType().dimensionLeft();
        int right = unit->getType().dimensionRight();
        int top = unit->getType().dimensionUp();
        int bottom = unit->getType().dimensionDown();

        int x = unit->getPosition().x;
        int y = unit->getPosition().y;

        BWAPI::Broodwar->drawBoxMap(x - left, y - top, x + right, y + bottom,
                                    Config::Debug::ColorUnitNearEnemy);
      }
    }
  }
}

void Squad::addUnitsToMicroManagers() {
  BWAPI::Unitset airToAirUnits;
  BWAPI::Unitset meleeUnits;
  BWAPI::Unitset rangedUnits;
  BWAPI::Unitset defilerUnits;
  BWAPI::Unitset detectorUnits;
  BWAPI::Unitset highTemplarUnits;
  BWAPI::Unitset scourgeUnits;
  BWAPI::Unitset transportUnits;
  BWAPI::Unitset lurkerUnits;
  //BWAPI::Unitset mutaUnits;
  BWAPI::Unitset overlordUnits;
  BWAPI::Unitset tankUnits;
  BWAPI::Unitset medicUnits;

  // We will assign zerglings as defiler food. The defiler micro manager will control them.
  int defilerFoodWanted = 0;

  // First grab the defilers, so we know how many there are.
  // Assign the minimum number of zerglings as food--check each defiler's energy level.
  for (const auto unit : _units) {
    if (unit->getType() == BWAPI::UnitTypes::Zerg_Defiler &&
      unit->isCompleted() && unit->exists() && unit->getHitPoints() > 0 && unit->getPosition().isValid()) {
      defilerUnits.insert(unit);
      if (BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Consume)) {
        defilerFoodWanted += std::max(0, (199 - unit->getEnergy()) / 50);
      }
    }
  }

  for (const auto unit : _units) {
    if (unit->isCompleted() && unit->exists() && unit->getHitPoints() > 0 && unit->getPosition().isValid()) {
      if (defilerFoodWanted > 0 && unit->getType() == BWAPI::UnitTypes::Zerg_Zergling) {
        // If no defiler food is wanted, the zergling falls through to the melee micro manager.
        defilerUnits.insert(unit);
        --defilerFoodWanted;
      }
      else if (_name == "Overlord" && unit->getType() == BWAPI::UnitTypes::Zerg_Overlord) {
        // Special case for the Overlord squad: All overlords under control of MicroOverlords.
        overlordUnits.insert(unit);
      }
      else if (unit->getType() == BWAPI::UnitTypes::Terran_Valkyrie ||
        unit->getType() == BWAPI::UnitTypes::Protoss_Corsair ||
        unit->getType() == BWAPI::UnitTypes::Zerg_Devourer) {
        airToAirUnits.insert(unit);
      }
      else if (unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar) {
        highTemplarUnits.insert(unit);
      }
      else if (unit->getType() == BWAPI::UnitTypes::Zerg_Lurker) {
        lurkerUnits.insert(unit);
      }
        //else if (unit->getType() == BWAPI::UnitTypes::Zerg_Mutalisk)
        //{
        //	mutaUnits.insert(unit);
        //}
      else if (unit->getType() == BWAPI::UnitTypes::Zerg_Scourge) {
        scourgeUnits.insert(unit);
      }
      else if (unit->getType() == BWAPI::UnitTypes::Terran_Medic) {
        medicUnits.insert(unit);
      }
      else if (unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Siege_Mode ||
        unit->getType() == BWAPI::UnitTypes::Terran_Siege_Tank_Tank_Mode) {
        tankUnits.insert(unit);
      }
      else if (unit->getType().isDetector() && unit->getType().isFlyer()) // not a building
      {
        detectorUnits.insert(unit);
      }
        // NOTE This excludes overlords as transports (they are also detectors, a confusing case).
      else if (unit->getType() == BWAPI::UnitTypes::Protoss_Shuttle ||
        unit->getType() == BWAPI::UnitTypes::Terran_Dropship) {
        transportUnits.insert(unit);
      }
        // NOTE This excludes spellcasters.
      else if ((unit->getType().groundWeapon().maxRange() > 32) ||
        unit->getType() == BWAPI::UnitTypes::Protoss_Reaver ||
        unit->getType() == BWAPI::UnitTypes::Protoss_Carrier) {
        rangedUnits.insert(unit);
      }
      else if (unit->getType().isWorker() && _combatSquad) {
        // If this is a combat squad, then workers are melee units like any other,
        // but we have to tell WorkerManager about them.
        // If it's not a combat squad, WorkerManager owns them; don't add them to a micromanager.
        WorkerManager::Instance().setCombatWorker(unit);
        meleeUnits.insert(unit);
      }
        // Melee units include firebats, which have range 32.
      else if (unit->getType().groundWeapon().maxRange() <= 32) {
        meleeUnits.insert(unit);
      }
      // NOTE Some units may fall through and not be assigned.
    }
  }

  _microAirToAir.setUnits(airToAirUnits);
  _microMelee.setUnits(meleeUnits);
  _microRanged.setUnits(rangedUnits);
  _microDefilers.setUnits(defilerUnits);
  _microDetectors.setUnits(detectorUnits);
  _microHighTemplar.setUnits(highTemplarUnits);
  _microLurkers.setUnits(lurkerUnits);
  _microMedics.setUnits(medicUnits);
  //_microMutas.setUnits(mutaUnits);
  _microScourge.setUnits(scourgeUnits);
  _microOverlords.setUnits(overlordUnits);
  _microTanks.setUnits(tankUnits);
  _microTransports.setUnits(transportUnits);
}

// Calculates whether to regroup, aka retreat. Does combat sim if necessary.
bool Squad::needsToRegroup(const UnitCluster& cluster) {
  // Only specified orders are allowed to regroup.
  if (!_order.isRegroupableOrder()) {
    _regroupStatus = kYellow + std::string("Never retreat!");
    return false;
  }

  // If we're nearly maxed and have good income or cash, don't retreat.
  if (BWAPI::Broodwar->self()->supplyUsed() >= 390 &&
    (BWAPI::Broodwar->self()->minerals() > 1000 || WorkerManager::Instance().getNumMineralWorkers() > 12)) {
    _attackAtMax = true;
  }

  if (_attackAtMax) {
    if (BWAPI::Broodwar->self()->supplyUsed() < 320) {
      _attackAtMax = false;
    }
    else {
      _regroupStatus = kGreen + std::string("Banzai!");
      return false;
    }
  }

  BWAPI::Unit unitClosest = unitClosestToEnemy(cluster.units_);

  if (!unitClosest) {
    _regroupStatus = kYellow + std::string("No closest unit");
    return false;
  }

  // Is there static defense nearby that we should take into account?
  // unitClosest is known to be set thanks to the test immediately above.
  BWAPI::Unit nearest = nearbyStaticDefense(unitClosest->getPosition());
  const BWAPI::Position final = finalRegroupPosition();
  if (nearest) {
    // Don't retreat if we are in range of static defense that is attacking.
    if (nearest->getOrder() == BWAPI::Orders::AttackUnit) {
      _regroupStatus = kGreen + std::string("Go static defense!");
      return false;
    }

    // If there is static defense to retreat to, try to get behind it.
    // Assume that the static defense is between the final regroup position and the enemy.
    if (unitClosest->getDistance(nearest) < 196 &&
      unitClosest->getDistance(final) < nearest->getDistance(final)) {
      _regroupStatus = kGreen + std::string("Behind static defense");
      return false;
    }
  }
  else {
    // There is no static defense to retreat to.
    if (unitClosest->getDistance(final) < 224) {
      _regroupStatus = kGreen + std::string("Back to the wall");
      return false;
    }
  }

  // TODO disabled while the refactor is going on - rework this if needed
  // If we most recently retreated, don't attack again until retreatDuration frames have passed.
  const int retreatDuration = 2 * 24;
  //bool retreat = _lastRetreatSwitchVal && (BWAPI::Broodwar->getFrameCount() - _lastRetreatSwitch < retreatDuration);
  bool retreat = false;
  // bool retreat = Random::Instance().flag(0.98);

  if (!retreat) {
    // All other checks are done. Finally do the expensive combat simulation.
    CombatSimulation sim;

    // Center the circle of interest on the nearest enemy unit, not on one of our own units.
    // That reduces indecision: Enemy actions, not our own, induce us to move.
    BWAPI::Unit closestEnemy =
      BWAPI::Broodwar->getClosestUnit(unitClosest->getPosition(), BWAPI::Filter::IsEnemy, 384);
    BWAPI::Position combatSimCenter = closestEnemy ? closestEnemy->getPosition() : unitClosest->getPosition();
    //sim.setCombatUnits(cluster.units, combatSimCenter, _combatSimRadius, _fightVisibleOnly);

    // Certain unit types can or should exclude certain enemies from the sim.
    CombatSimEnemies enemies = CombatSimEnemies::kAllEnemies;
    if (_microRanged.getUnits().empty() &&
      (!_microMelee.getUnits().empty() || !_microLurkers.getUnits().empty() || !_microTanks.getUnits().empty())) {
      // Our units can't shoot up. Ignore air enemies that can't shoot down.
      enemies = CombatSimEnemies::kAntiGroundEnemies;
    }
    else if (!_microScourge.getUnits().empty()) {
      // NOTE This relies on scourge being separated, not mixed with any other unit type!
      enemies = CombatSimEnemies::kScourgeEnemies;
    }

    sim.SetCombatUnits(cluster.units_, unitClosest->getPosition(), _combatSimRadius, _fightVisibleOnly, enemies);
    _lastScore = sim.SimulateCombat(_meatgrinder);

    //double limit = _lastRetreatSwitchVal ? 0.8 : 1.1;
    // retreat = _lastScore < limit;

    retreat = _lastScore < 0.0;
    _lastRetreatSwitch = BWAPI::Broodwar->getFrameCount();
    _lastRetreatSwitchVal = retreat;
  }

  if (retreat) {
    _regroupStatus = kRed + std::string("Retreat");
  }
  else {
    _regroupStatus = kGreen + std::string("Attack");
  }

  return retreat;
}

BWAPI::Position Squad::calcRegroupPosition(const UnitCluster& cluster) const {
  // 1. Retreat toward static defense, if any is near.
  BWAPI::Unit vanguard = unitClosestToEnemy(cluster.units_);

  if (vanguard) {
    BWAPI::Unit nearest = nearbyStaticDefense(vanguard->getPosition());
    if (nearest) {
      // TODO this should be better but has a bug that can cause retreat toward the enemy
      // It's better to retreat to behind the static defense.
      // BWAPI::Position behind =
      //	DistanceAndDirection(nearest->getPosition(), cluster.center, -96);
      // return behind;

      return nearest->getPosition();
    }
  }

  // 2. Regroup toward another cluster.
  // Look for a cluster nearby, and preferably closer to the enemy.
  BWAPI::Unit closestEnemy = BWAPI::Broodwar->getClosestUnit(cluster.center_, BWAPI::Filter::IsEnemy, 384);
  const int safeRange = closestEnemy ? closestEnemy->getDistance(cluster.center_) : 384;
  const UnitCluster* bestCluster = nullptr;
  int bestScore = -99999;
  for (const UnitCluster& neighbor : _clusters) {
    int distToNeighbor = cluster.center_.getApproxDistance(neighbor.center_);
    int distToOrder = cluster.center_.getApproxDistance(_order.getPosition());
    // An air cluster may join a ground cluster, but not vice versa.
    if (distToNeighbor < safeRange && distToNeighbor > 0 && cluster.air_ >= neighbor.air_) {
      int score = distToOrder - neighbor.center_.getApproxDistance(_order.getPosition());
      if (neighbor.status_ == ClusterStatus::kAttack) {
        score += 4 * 32;
      }
      else if (neighbor.status_ == ClusterStatus::kRegroup) {
        score -= 32;
      }
      if (score > bestScore) {
        bestCluster = &neighbor;
        bestScore = score;
      }
    }
  }
  if (bestCluster) {
    return bestCluster->center_;
  }

  // 3. Retreat to the location of the cluster unit not near the enemy which is
  // closest to the order position. This tries to stay close while still out of range.
  // Units in the cluster are all air or all ground and exclude mobile detectors.
  BWAPI::Position regroup(BWAPI::Positions::Origin);
  int minDist = 100000;
  for (const auto unit : cluster.units_) {
    // Count combat units only. Bug fix originally thanks to AIL, it's been rewritten since then.
    if (unit->exists() &&
      !_nearEnemy.at(unit) &&
      unit->getType() != BWAPI::UnitTypes::Terran_Medic &&
      unit->getPosition().isValid()) // excludes loaded units
    {
      int dist = unit->getDistance(_order.getPosition());
      if (dist < minDist) {
        // If the squad has any ground units, don't try to retreat to the position of a unit
        // which is in a place that we cannot reach.
        if (!_hasGround || -1 != MapTools::Instance().getGroundTileDistance(unit->getPosition(), _order.getPosition())
        ) {
          minDist = dist;
          regroup = unit->getPosition();
        }
      }
    }
  }
  if (regroup != BWAPI::Positions::Origin) {
    return regroup;
  }

  // 4. Retreat to a base we own.
  return finalRegroupPosition();
}

// Return the rearmost position we should retreat to, which puts our "back to the wall".
BWAPI::Position Squad::finalRegroupPosition() const {
  // Retreat to the starting base (guaranteed not null, even if the buildings were destroyed).
  Base* base = Bases::Instance().myStartingBase();

  // If the natural has been taken, retreat there instead.
  Base* natural = Bases::Instance().myNaturalBase();
  if (natural && natural->getOwner() == BWAPI::Broodwar->self()) {
    base = natural;
  }
  return BWTA::getRegion(base->getTilePosition())->getCenter();
}

bool Squad::containsUnit(BWAPI::Unit u) const {
  return _units.contains(u);
}

BWAPI::Unit Squad::nearbyStaticDefense(const BWAPI::Position& pos) const {
  BWAPI::Unit nearest = nullptr;

  // NOTE What matters is whether the enemy has ground or air units.
  //      We are checking the wrong thing here. But it's usually correct anyway.
  if (hasGround()) {
    nearest = InformationManager::Instance().nearestGroundStaticDefense(pos);
  }
  else {
    nearest = InformationManager::Instance().nearestAirStaticDefense(pos);
  }
  if (nearest && nearest->getDistance(pos) < 800) {
    return nearest;
  }
  return nullptr;
}

bool Squad::containsUnitType(BWAPI::UnitType t) const {
  for (const auto u : _units) {
    if (u->getType() == t) {
      return true;
    }
  }
  return false;
}

void Squad::clear() {
  for (const auto unit : _units) {
    if (unit->getType().isWorker()) {
      WorkerManager::Instance().finishedWithWorker(unit);
    }
  }

  _units.clear();
}

bool Squad::unitNearEnemy(BWAPI::Unit unit) {
  UAB_ASSERT(unit, "missing unit");

  // A unit is automatically near the enemy if it is within the order radius.
  // This imposes a requirement that the order should make sense.
  if (unit->getDistance(_order.getPosition()) <= _order.getRadius()) {
    return true;
  }

  int safeDistance = (!unit->isFlying() && InformationManager::Instance().enemyHasSiegeMode()) ? 512 : 384;

  // For each enemy unit.
  for (const auto& kv : InformationManager::Instance().getUnitData(BWAPI::Broodwar->enemy()).getUnits()) {
    const UnitInfo& ui(kv.second);

    if (!ui.goneFromLastPosition) {
      if (unit->getDistance(ui.lastPosition) <= safeDistance) {
        return true;
      }
    }
  }
  return false;
}

// What map partition is the squad on?
// Not an easy question. The different units might be on different partitions.
// We simply pick a unit, any unit, and assume that that gives the partition.
int Squad::mapPartition() const {
  // Default to our starting position.
  BWAPI::Position pos = Bases::Instance().myStartingBase()->getPosition();

  // Pick any unit with a position on the map (not, for example, in a bunker).
  for (BWAPI::Unit unit : _units) {
    if (unit->getPosition().isValid()) {
      pos = unit->getPosition();
      break;
    }
  }

  return the.map_partitions_.id(pos);
}

// NOTE The squad center is a geometric center. It ignores terrain.
// The center might be on unwalkable ground, or even on a different island.
BWAPI::Position Squad::calcCenter() const {
  if (_units.empty()) {
    return Bases::Instance().myStartingBase()->getPosition();
  }

  BWAPI::Position accum(0, 0);
  for (const auto unit : _units) {
    if (unit->getPosition().isValid()) {
      accum += unit->getPosition();
    }
  }
  return BWAPI::Position(accum.x / _units.size(), accum.y / _units.size());
}

// Return the unit closest to the order position (not actually closest to the enemy).
BWAPI::Unit Squad::unitClosestToEnemy(const BWAPI::Unitset units) const {
  BWAPI::Unit closest = nullptr;
  int closestDist = 100000;

  UAB_ASSERT(_order.getPosition().isValid(), "bad order position");

  for (const auto unit : units) {
    // Non-combat units should be ignored for this calculation.
    if (unit->getType().isDetector() ||
      !unit->getPosition().isValid() || // includes units loaded into bunkers or transports
      unit->getType() == BWAPI::UnitTypes::Terran_Medic ||
      unit->getType() == BWAPI::UnitTypes::Protoss_High_Templar ||
      unit->getType() == BWAPI::UnitTypes::Protoss_Dark_Archon ||
      unit->getType() == BWAPI::UnitTypes::Zerg_Defiler ||
      unit->getType() == BWAPI::UnitTypes::Zerg_Queen) {
      continue;
    }

    int dist;
    if (_hasGround) {
      // A ground or air-ground squad. Use ground distance.
      // It is -1 if no ground path exists.
      dist = MapTools::Instance().getGroundDistance(unit->getPosition(), _order.getPosition());
    }
    else {
      // An all-air squad. Use air distance (which is what unit->getDistance() gives).
      dist = unit->getDistance(_order.getPosition());
    }

    if (dist < closestDist && dist != -1) {
      closest = unit;
      closestDist = dist;
    }
  }

  return closest;
}

const BWAPI::Unitset& Squad::getUnits() const {
  return _units;
}

void Squad::setSquadOrder(const SquadOrder& so) {
  _order = so;

  // Pass the order on to all micromanagers.
  _microAirToAir.setOrder(so);
  _microMelee.setOrder(so);
  _microRanged.setOrder(so);
  _microDetectors.setOrder(so);
  _microHighTemplar.setOrder(so);
  _microLurkers.setOrder(so);
  _microMedics.setOrder(so);
  //_microMutas.setOrder(so);
  _microScourge.setOrder(so);
  _microTanks.setOrder(so);
  _microTransports.setOrder(so);
}

const SquadOrder& Squad::getSquadOrder() const {
  return _order;
}

const std::string Squad::getRegroupStatus() const {
  return _regroupStatus;
}

void Squad::addUnit(BWAPI::Unit u) {
  _units.insert(u);
}

void Squad::removeUnit(BWAPI::Unit u) {
  if (_combatSquad && u->getType().isWorker()) {
    WorkerManager::Instance().finishedWithWorker(u);
  }
  _units.erase(u);
}

// Remove all workers from the squad, releasing them back to WorkerManager.
void Squad::releaseWorkers() {
  for (auto it = _units.begin(); it != _units.end();) {
    if (_combatSquad && (*it)->getType().isWorker()) {
      WorkerManager::Instance().finishedWithWorker(*it);
      it = _units.erase(it);
    }
    else {
      ++it;
    }
  }
}

const std::string& Squad::getName() const {
  return _name;
}

// The drop squad has been given a Load order. Load up the transports for a drop.
// Unlike other code in the drop system, this supports any number of transports, including zero.
// Called once per frame while a Load order is in effect.
void Squad::loadTransport() {
  for (const auto trooper : _units) {
    // If it's not the transport itself, send it toward the order location,
    // which is set to the transport's initial location.
    if (trooper->exists() && !trooper->isLoaded() && trooper->getType().spaceProvided() == 0) {
      the.micro_.Move(trooper, _order.getPosition());
    }
  }

  for (const auto transport : _microTransports.getUnits()) {
    if (!transport->exists()) {
      continue;
    }

    for (const auto unit : _units) {
      if (transport->getSpaceRemaining() == 0) {
        break;
      }

      if (transport->load(unit)) {
        break;
      }
    }
  }
}

// Stim marines and firebats if possible and appropriate.
// This stims for combat. It doesn't consider stim to travel faster.
// We bypass the micro managers because it simplifies the bookkeeping, but a disadvantage
// is that we don't have access to the target list. Should refactor a little to get that,
// because it can help us figure out how important it is to stim.
void Squad::stimIfNeeded() {
  // Do we have stim?
  if (BWAPI::Broodwar->self()->getRace() != BWAPI::Races::Terran ||
    !BWAPI::Broodwar->self()->hasResearched(BWAPI::TechTypes::Stim_Packs)) {
    return;
  }

  // Are there enemies nearby that we may want to fight?
  if (_nearEnemy.empty()) {
    return;
  }

  // So far so good. Time to get into details.

  // Stim can be used more freely if we have medics with lots of energy.
  int totalMedicEnergy = _microMedics.getTotalEnergy();

  // Stim costs 10 HP, which requires 5 energy for a medic to heal.
  const int stimEnergyCost = 5;

  // Firebats first, because they are likely to be right up against the enemy.
  for (const auto firebat : _microMelee.getUnits()) {
    // Invalid position means the firebat is probably in a bunker or transport.
    if (firebat->getType() != BWAPI::UnitTypes::Terran_Firebat || !firebat->getPosition().isValid()) {
      continue;
    }
    // Don't overstim and lose too many HP.
    if (firebat->getHitPoints() < 35 || totalMedicEnergy <= 0 && firebat->getHitPoints() < 45) {
      continue;
    }

    BWAPI::Unitset nearbyEnemies;
    MapGrid::Instance().GetUnits(nearbyEnemies, firebat->getPosition(), 64, false, true);

    // NOTE We don't check whether the enemy is attackable or worth attacking.
    if (!nearbyEnemies.empty()) {
      the.micro_.Stim(firebat);
      totalMedicEnergy -= stimEnergyCost;
    }
  }

  // Next marines, treated the same except for range and hit points.
  for (const auto marine : _microRanged.getUnits()) {
    // Invalid position means the marine is probably in a bunker or transport.
    if (marine->getType() != BWAPI::UnitTypes::Terran_Marine || !marine->getPosition().isValid()) {
      continue;
    }
    // Don't overstim and lose too many HP.
    if (marine->getHitPoints() <= 30 || totalMedicEnergy <= 0 && marine->getHitPoints() < 40) {
      continue;
    }

    BWAPI::Unitset nearbyEnemies;
    MapGrid::Instance().GetUnits(nearbyEnemies, marine->getPosition(), 5 * 32, false, true);

    if (!nearbyEnemies.empty()) {
      the.micro_.Stim(marine);
      totalMedicEnergy -= stimEnergyCost;
    }
  }
}

const bool Squad::hasCombatUnits() const {
  // If the only units we have are detectors, then we have no combat units.
  return !(_units.empty() || _units.size() == _microDetectors.getUnits().size());
}

// Is every unit in the squad an overlord hunter (or a detector)?
// An overlord hunter is a fast air unit that is strong against overlords.
const bool Squad::isOverlordHunterSquad() const {
  if (!hasCombatUnits()) {
    return false;
  }

  for (const auto unit : _units) {
    const BWAPI::UnitType type = unit->getType();
    if (!type.isFlyer()) {
      return false;
    }
    if (!type.isDetector() &&
      type != BWAPI::UnitTypes::Terran_Wraith &&
      type != BWAPI::UnitTypes::Terran_Valkyrie &&
      type != BWAPI::UnitTypes::Zerg_Mutalisk &&
      type != BWAPI::UnitTypes::Zerg_Scourge && // questionable, but the squad may have both
      type != BWAPI::UnitTypes::Protoss_Corsair &&
      type != BWAPI::UnitTypes::Protoss_Scout) {
      return false;
    }
  }
  return true;
}

void Squad::drawCluster(const UnitCluster& cluster) const {
  if (Config::Debug::DrawClusters) {
    cluster.Draw(BWAPI::Colors::Grey, kWhite + _name + ' ' + _regroupStatus);
  }
}

void Squad::drawCombatSimInfo() const {
  if (!_units.empty() && _order.isRegroupableOrder()) {
    BWAPI::Unit vanguard = unitClosestToEnemy(_units);
    BWAPI::Position spot = vanguard ? vanguard->getPosition() : _order.getPosition();

    BWAPI::Broodwar->drawTextMap(spot + BWAPI::Position(-16, 16), "%c%c %c%s",
                                 kYellow, _order.getCharCode(), kWhite, _name.c_str());
    BWAPI::Broodwar->drawTextMap(spot + BWAPI::Position(-16, 26), "sim %c%g",
                                 _lastScore >= 0 ? kGreen : kRed, _lastScore);
  }
}
