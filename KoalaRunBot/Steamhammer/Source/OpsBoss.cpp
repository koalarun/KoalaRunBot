#include "OpsBoss.h"
#include "The.h"
#include "InformationManager.h"
#include "UnitUtil.h"

using namespace KoalaRunBot;

UnitCluster::UnitCluster()
  : center_(BWAPI::Positions::Origin)
    , radius_(0)
    , status_(ClusterStatus::kNone)
    , air_(false)
    , slowest_speed_(0.0)
    , count_(0)
    , total_hp_(0)
    , ground_dpf_(0.0)
    , air_dpf_(0.0) {

}

// Add a unit to the cluster.
// While adding units, we don't worry about the center and radius.
void UnitCluster::Add(const UnitInfo& ui) {
  if (count_ == 0) {
    air_ = ui.type.isFlyer();
    slowest_speed_ = BWAPI::Broodwar->enemy()->topSpeed(ui.type);
  }
  else {
    const auto top_speed = BWAPI::Broodwar->enemy()->topSpeed(ui.type);
    if (top_speed > 0.0) {
      slowest_speed_ = std::min(slowest_speed_, top_speed);
    }
  }
  ++count_;
  total_hp_ += ui.estimateHealth();
  ground_dpf_ += UnitUtil::GroundDPF(BWAPI::Broodwar->enemy(), ui.type);
  air_dpf_ += UnitUtil::AirDPF(BWAPI::Broodwar->enemy(), ui.type);
  units_.insert(ui.unit);
}

void UnitCluster::Draw(const BWAPI::Color color, const std::string& label) const {
  BWAPI::Broodwar->drawCircleMap(center_, radius_, color);

  BWAPI::Position xy(center_.x - 12, center_.y - radius_ + 8);
  if (xy.y < 8) {
    xy.y = center_.y + radius_ - 4 * 10 - 8;
  }

  BWAPI::Broodwar->drawTextMap(xy, "%c%s %c%d", kOrange, air_ ? "air" : "ground", kCyan, count_);
  xy.y += 10;
  BWAPI::Broodwar->drawTextMap(xy, "%chp %c%d", kOrange, kCyan, total_hp_);
  xy.y += 10;
  //BWAPI::Broodwar->drawTextMap(xy, "%cdpf %c%g/%g", orange, cyan, groundDPF, airDPF);
  // xy.y += 10;
  //BWAPI::Broodwar->drawTextMap(xy, "%cspeed %c%g", orange, cyan, speed);
  // xy.y += 10;
  if (!label.empty()) {
    // The label is responsible for its own colors.
    BWAPI::Broodwar->drawTextMap(xy, "%s", label.c_str());
    xy.y += 10;
  }
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

// Given the set of unit positions in a cluster, find the center and radius.
void OpsBoss::LocateCluster(const std::vector<BWAPI::Position>& points, UnitCluster& cluster) const {
  auto total = BWAPI::Positions::Origin;
  UAB_ASSERT(!points.empty(), "empty cluster");
  for (const auto& point : points) {
    total += point;
  }
  cluster.center_ = total / points.size();

  auto radius = 0;
  for (const auto& point : points) {
    radius = std::max(radius, point.getApproxDistance(cluster.center_));
  }
  cluster.radius_ = std::max(32, radius);
}

// Form a cluster around the given seed, updating the value of the cluster argument.
// Remove enemies added to the cluster from the enemies set.
void OpsBoss::FormCluster(const UnitInfo& seed, const UIMap& the_ui, BWAPI::Unitset& units, UnitCluster& cluster) const {
  cluster.Add(seed);
  cluster.center_ = seed.lastPosition;

  // The locations of each unit in the cluster so far.
  std::vector<BWAPI::Position> points;
  points.push_back(seed.lastPosition);

  bool any;
  auto next_radius = cluster_start_;
  do {
    any = false;
    for (auto it = units.begin(); it != units.end();) {
      const UnitInfo& ui = the_ui.at(*it);
      if (ui.type.isFlyer() == cluster.air_ &&
        cluster.center_.getApproxDistance(ui.lastPosition) <= next_radius) {
        any = true;
        points.push_back(ui.lastPosition);
        cluster.Add(ui);
        it = units.erase(it);
      }
      else {
        ++it;
      }
    }
    LocateCluster(points, cluster);
    next_radius = cluster.radius_ + cluster_range_;
  }
  while (any);
}

// Group a given set of units into clusters.
// NOTE The set of units gets modified! You may have to copy it before you pass it in.
void OpsBoss::ClusterUnits(BWAPI::Unitset& units, std::vector<UnitCluster>& clusters) const {
  clusters.clear();

  if (units.empty()) {
    return;
  }

  const UIMap& theUI = InformationManager::Instance().getUnitData((*units.begin())->getPlayer()).getUnits();

  while (!units.empty()) {
    const auto& seed = theUI.at(*units.begin());
    units.erase(units.begin());

    clusters.emplace_back();
    FormCluster(seed, theUI, units, clusters.back());
  }
}

// -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- -- --

OpsBoss::OpsBoss()
  : the_(The::Root()) {
}

void OpsBoss::Initialize() {
}

// Group all units of a player into clusters.
void OpsBoss::Cluster(BWAPI::Player player, std::vector<UnitCluster>& clusters) const {
  const UIMap& the_ui = InformationManager::Instance().getUnitData(player).getUnits();

  // Step 1: Gather units that should be put into clusters.

  const auto now = BWAPI::Broodwar->getFrameCount();
  BWAPI::Unitset units;

  for (const auto& kv : the_ui) {
    const auto& ui = kv.second;

    if (UnitUtil::IsCombatSimUnit(ui.type) && // not a worker, larva, ...
      !ui.type.isBuilding() && // not a static defense building
      (!ui.goneFromLastPosition || // not known to have moved from its last position, or
        now - ui.updateFrame < 5 * 24)) // known to have moved but not long ago
    {
      units.insert(kv.first);
    }
  }

  // Step 2: Fill in the clusters.

  ClusterUnits(units, clusters);
}

// Group a given set of units into clusters.
void OpsBoss::Cluster(const BWAPI::Unitset& units, std::vector<UnitCluster>& clusters) const {
  BWAPI::Unitset unitsCopy = units;

  ClusterUnits(unitsCopy, clusters); // NOTE modifies unitsCopy
}

void OpsBoss::Update() {
  const int phase = BWAPI::Broodwar->getFrameCount() % 5;

  if (phase == 0) {
    Cluster(BWAPI::Broodwar->enemy(), your_clusters_);
  }

  DrawClusters();
}

// Draw enemy clusters.
// Squads are responsible for drawing squad clusters.
void OpsBoss::DrawClusters() const {
  if (Config::Debug::DrawClusters) {
    for (const UnitCluster& cluster : your_clusters_) {
      cluster.Draw(BWAPI::Colors::Red);
    }
  }
}
