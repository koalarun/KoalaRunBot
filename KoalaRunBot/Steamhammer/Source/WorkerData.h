#pragma once

#include "Common.h"

namespace KoalaRunBot {
  class The;

  class WorkerMoveData {
  public:

    int minerals_need_;
    int gas_need_;
    BWAPI::Position position_;

    WorkerMoveData(const int minerals_need, int gas_need, const BWAPI::Position position)
      : minerals_need_(minerals_need)
        , gas_need_(gas_need)
        , position_(position) { }

    WorkerMoveData(): minerals_need_(0), gas_need_(0) {}
  };

  class WorkerData {
    The& the_;

  public:
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
    std::map<BWAPI::Unit, BWAPI::Unit> worker_2_mineral_patch_; // worker -> mineral patch

    std::map<BWAPI::Unit, int> depot_2_worker_num_; // mineral workers per depot
    std::map<BWAPI::Unit, int> refinery_2_worker_num_; // gas workers per refinery
    std::map<BWAPI::Unit, int> mineral_patch_2_worker_num_; // workers per mineral patch
    
    void ClearPreviousJob(BWAPI::Unit unit);

  public:

    WorkerData();
    const BWAPI::Unitset& GetWorkers() const { return workers_; }

    void WorkerDestroyed(BWAPI::Unit unit);
    void AddWorker(BWAPI::Unit unit);

    void AddDepot(BWAPI::Unit unit);
    void DepotDestroyed(BWAPI::Unit unit);
    
    // void AddWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit);
    // void AddWorker(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType);
    void SetWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::Unit jobUnit);
    void SetWorkerJob(BWAPI::Unit unit, WorkerJob job, WorkerMoveData wmd);
    void SetWorkerJob(BWAPI::Unit unit, WorkerJob job, BWAPI::UnitType jobUnitType);

    int GetWorkersTotalNum() const;
    int ComputeMineralWorkersNum() const;
    int ComputeGasWorkersNum() const;
    int ComputeReturnCargoWorkersNum() const;
    int ComputeCombatWorkersNum() const;
    int ComputeIdleWorkersNum() const;
    char GetJobCode(BWAPI::Unit unit);

    // void getMineralWorkers(std::set<BWAPI::Unit>& mw);
    std::set<BWAPI::Unit> FindGasWorkers();
    // void getBuildingWorkers(std::set<BWAPI::Unit>& mw);
    // void getRepairWorkers(std::set<BWAPI::Unit>& mw);

    bool IsDepotFull(BWAPI::Unit depot);
    int ComputeMineralNumNearDepot(BWAPI::Unit depot);

    int ComputeAssignedWorkersNum(BWAPI::Unit unit);
    BWAPI::Unit GetOptimalMineralForWorker(BWAPI::Unit worker);

    enum WorkerJob GetWorkerJob(BWAPI::Unit unit);
    BWAPI::Unit GetWorkerResource(BWAPI::Unit worker);
    BWAPI::Unit GetWorkerDepot(BWAPI::Unit unit);
    BWAPI::Unit GetWorkerRepairUnit(BWAPI::Unit unit);
    BWAPI::UnitType GetWorkerBuildingType(BWAPI::Unit unit);
    WorkerMoveData GetWorkerMoveData(BWAPI::Unit unit);

    BWAPI::Unitset FindMineralPatchesNearDepot(BWAPI::Unit depot);
    void AddWorkerNumOfMineralPatch(BWAPI::Unit mineral_unit, int num);

    void DrawDepotDebugInfo();
  };
}
