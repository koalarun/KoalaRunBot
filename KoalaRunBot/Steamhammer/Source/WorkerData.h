#pragma once

#include "Common.h"

namespace KoalaRunBot {
  class The;

  class WorkerMoveData {
  public:

    int minerals_needed_;
    int gas_needed_;
    BWAPI::Position position_;

    WorkerMoveData(const int m, int g, const BWAPI::Position p)
      : minerals_needed_(m)
        , gas_needed_(g)
        , position_(p) { }

    WorkerMoveData(): minerals_needed_(0), gas_needed_(0) {}
  };

  class WorkerData {
    The& the_;

  public:
    //todo add protect job, to block the intersection, or surround the fort, or fight with the combat unit at early game.
    enum WorkerJob { kMinerals, kGas, kBuild, kCombat, kIdle, kRepair, kMove, kScout, kReturnCargo, kDefault };

  private:

    BWAPI::Unitset workers_;
    BWAPI::Unitset depots_;

    std::map<BWAPI::Unit, enum WorkerJob> worker_2_job_; // worker -> job
    std::map<BWAPI::Unit, BWAPI::Unit> worker_2_depot_; // worker -> resource depot (hatchery)
    std::map<BWAPI::Unit, BWAPI::Unit> worker_2_refinery_; // worker -> refinery
    std::map<BWAPI::Unit, BWAPI::Unit> worker_2_repair_; // worker -> unit to repair
    std::map<BWAPI::Unit, WorkerMoveData> worker_2_move_data_; // worker -> location
    std::map<BWAPI::Unit, BWAPI::UnitType> worker_2_building_type_; // worker -> building type

    std::map<BWAPI::Unit, int> depot_2_worker_count_; // mineral workers per depot
    std::map<BWAPI::Unit, int> refinery_2_worker_count_; // gas workers per refinery

    std::map<BWAPI::Unit, int> mineral_patch_2_worker_num_; // workers per mineral patch
    std::map<BWAPI::Unit, BWAPI::Unit> worker_2_mineral_patch_; // worker -> mineral patch

    void ClearPreviousJob(BWAPI::Unit unit);

  public:

    WorkerData();
    const BWAPI::Unitset& GetWorkers() const { return workers_; }
    void WorkerDestroyed(BWAPI::Unit unit);
    void AddDepot(BWAPI::Unit unit);
    void RemoveDepot(BWAPI::Unit unit);
    void AddWorker(BWAPI::Unit unit);
    // void AddWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit);
    // void AddWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType);
    void SetWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit);
    void SetWorkerJob(BWAPI::Unit unit, WorkerJob job, WorkerMoveData wmd);
    void SetWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType);

    int GetNumWorkers() const;
    int ComputeMineralWorkersNum() const;
    int ComputeGasWorkersNum() const;
    int ComputeReturnCargoWorkersNum() const;
    int ComputeCombatWorkersNum() const;
    int ComputeIdleWorkersNum() const;
    char GetJobCode(BWAPI::Unit unit);

    // void getMineralWorkers(std::set<BWAPI::Unit>& mw);
    std::set<BWAPI::Unit> GetGasWorkers();
    // void getBuildingWorkers(std::set<BWAPI::Unit>& mw);
    // void getRepairWorkers(std::set<BWAPI::Unit>& mw);

    bool IsDepotFull(BWAPI::Unit depot);
    int ComputeMineralNumNearDepot(BWAPI::Unit depot);

    int getNumAssignedWorkers(BWAPI::Unit unit);
    BWAPI::Unit GetOptimalMineralForWorker(BWAPI::Unit worker);

    enum WorkerJob GetWorkerJob(BWAPI::Unit unit);
    BWAPI::Unit GetWorkerResource(BWAPI::Unit unit);
    BWAPI::Unit GetWorkerDepot(BWAPI::Unit unit);
    BWAPI::Unit GetWorkerRepairUnit(BWAPI::Unit unit);
    BWAPI::UnitType GetWorkerBuildingType(BWAPI::Unit unit);
    WorkerMoveData GetWorkerMoveData(BWAPI::Unit unit);

    BWAPI::Unitset FindMineralPatchesNearDepot(BWAPI::Unit depot);
    void AddWorkerNumOfMineralPatch(BWAPI::Unit unit, int num);
    void DrawDepotDebugInfo();
  };
}
