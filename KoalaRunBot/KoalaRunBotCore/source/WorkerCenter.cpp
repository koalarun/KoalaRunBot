#include "WorkerCenter.h"
#include <BWAPI/Game.h>
#include <BWAPI/Unit.h>

using namespace KoalaRunBot;


WorkerCenter::WorkerCenter() {}


WorkerCenter::~WorkerCenter() {}

int WorkerCenter::GetCompleteWorkerTotalCount() {
	return workers_.size();
}

void WorkerCenter::AddNewWorker(BWAPI::Unit worker) {
	if (worker->isCompleted()) {
		workers_.insert(worker);
		SetWorkerJobIdle(worker);
	}
	else {
		incomplete_workers_.insert(worker);
	}
}

void WorkerCenter::DestroyWorker(BWAPI::Unit worker) {
  incomplete_workers_.erase(worker);
	workers_.erase(worker);
  worker_2_job_.erase(worker);
}

WorkerCenter::WorkerJob WorkerCenter::GetWorkerJob(BWAPI::Unit worker) {
  return worker_2_job_[worker];
}

void WorkerCenter::SetWorkerJobIdle(BWAPI::Unit worker) {
  //worker->stop(false);
	worker_2_job_[worker] = kIdle;
}

void WorkerCenter::SetWorkerJobMinerals(BWAPI::Unit worker) {
	//find a optimal mineral
	//--find the near depot, if not full, mine the mineral near this depot; otherwise, find next depot;
	// BWAPI::Unit depot = ResourceDepotCenter::Instance().GetNearestDepot(worker);
 //  if (!ResourceDepotCenter::Instance().IsDepotMineralWorkerFull(depot)) {
	// 	//
 //
 //    worker_2_job_[worker] = kMinerals;
 //  } else {
 //    worker_2_job_[worker] = kIdle;
 //  }

  const BWAPI::Unit minerals_near_worker = worker->getClosestUnit(BWAPI::Filter::IsMineralField);
  if (minerals_near_worker) {
    worker->gather(minerals_near_worker);
    worker_2_job_[worker] = kMinerals;
  }
  
}


//shift complete worker from incomplete_workers_ to workers_
void WorkerCenter::ShiftCompleteWorker() {
	for (auto it = incomplete_workers_.begin(); it != incomplete_workers_.end();) {
		if ((*it)->isCompleted()) {
      workers_.insert(*it);
      SetWorkerJobIdle(*it);
			it = incomplete_workers_.erase(it);
		}
		else {
			++it;
		}
	}
}


void WorkerCenter::Update() {
	ShiftCompleteWorker();
}


WorkerCenter& WorkerCenter::Instance() {
	static WorkerCenter instance;
	return instance;
}

void WorkerCenter::Clear() {
  incomplete_workers_.clear();
  workers_.clear();
  worker_2_job_.clear();
}
