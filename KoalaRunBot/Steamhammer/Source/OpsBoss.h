#pragma once

// Operations boss.

#include <BWAPI.h>
#include "UnitStatistic.h"

namespace KoalaRunBot {
  class The;
  struct UnitInfo;

  enum class ClusterStatus {
    kNone,
    // enemy cluster or not updated yet
    kAdvance,
    // no enemy near, moving forward
    kAttack,
    // enemy nearby, attacking
    kRegroup // regrouping (usually retreating)
  };

  class UnitCluster {
  public:
    BWAPI::Position center_;
    int radius_{};
    ClusterStatus status_;

    bool air_{};
    double slowest_speed_{};

    int count_{};
    int total_hp_{};
    double ground_dpf_{}; // damage per frame
    double air_dpf_{};

    BWAPI::Unitset units_;

    UnitCluster();

    void Add(const UnitInfo& ui);
    void Draw(BWAPI::Color color, const std::string& label = "") const;
  };

  // Operations boss.
  // Responsible for high-level tactical analysis and decisions.
  // This will eventually replace CombatCommander, once all the new parts are available.
  class OpsBoss {
    The& the_;

    const int cluster_start_ = 5 * 32;
    const int cluster_range_ = 3 * 32;

    std::vector<UnitCluster> your_clusters_;

    void LocateCluster(const std::vector<BWAPI::Position>& points, UnitCluster& cluster) const;
    void FormCluster(const UnitInfo& seed, const UIMap& the_ui, BWAPI::Unitset& units, UnitCluster& cluster) const;
    void ClusterUnits(BWAPI::Unitset& units, std::vector<UnitCluster>& clusters) const;

  public:
    OpsBoss();
    void Initialize();

    void Cluster(BWAPI::Player player, std::vector<UnitCluster>& clusters) const;
    void Cluster(const BWAPI::Unitset& units, std::vector<UnitCluster>& clusters) const;

    void Update();

    void DrawClusters() const;
  };
}
