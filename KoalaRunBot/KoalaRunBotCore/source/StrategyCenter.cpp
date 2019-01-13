#include <BWAPI/Game.h>
#include <BWAPI/Unit.h>
#include "StrategyCenter.h"
#include "WorkerCenter.h"
#include "ResourceDepotCenter.h"

using namespace KoalaRunBot;

void StrategyCenter::Update() {
	//depot create worker if idle
  for (BWAPI::Unit depot_tmp : ResourceDepotCenter::Instance().Depots()) {
    if (depot_tmp->isIdle()) {
      depot_tmp->train(depot_tmp->getType().getRace().getWorker());
    }
  }

	//send idle worker to minerals
  for (BWAPI::Unit worker_tmp : WorkerCenter::Instance().Workers()) {
    if (WorkerCenter::Instance().GetWorkerJob(worker_tmp) == WorkerCenter::kIdle) {
      WorkerCenter::Instance().SetWorkerJobMinerals(worker_tmp);
    }
  }
}

StrategyCenter& StrategyCenter::Instance() {
  static StrategyCenter instance;
  return instance;
}
