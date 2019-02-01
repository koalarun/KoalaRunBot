#pragma once
#include <BWAPI/Unitset.h>

namespace KoalaRunBot {
  class WorkerCenter
  {
  public:
    enum WorkerJob { kIdle, kMinerals, kGas, kRepair, kScout, kBuild, kCombat, kMove, kReturnCargo };
  private:
    BWAPI::Unitset incomplete_workers_;
    BWAPI::Unitset workers_;
    std::map<BWAPI::Unit, enum WorkerJob> worker_2_job_;

    WorkerCenter();
		void ShiftCompleteWorker();
  public:
    ~WorkerCenter();

		//get set
    BWAPI::Unitset Workers() const { return workers_; }

		//worker data
    int GetCompleteWorkerTotalCount();
    void AddNewWorker(BWAPI::Unit worker);
    void DestroyWorker(BWAPI::Unit worker);
    WorkerJob GetWorkerJob(BWAPI::Unit worker);
		//worker operate
		void SetWorkerJobIdle(BWAPI::Unit worker);
    void SetWorkerJobMinerals(BWAPI::Unit worker);
		//worker update
	  void Update();

    static WorkerCenter& Instance();
    void Clear();
  };
}


