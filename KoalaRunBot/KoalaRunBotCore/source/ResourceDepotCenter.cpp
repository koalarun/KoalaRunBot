#include "ResourceDepotCenter.h"
#include <BWAPI/Unit.h>
#include "WorkerCenter.h"

using namespace KoalaRunBot;

void ResourceDepotCenter::ShiftCompleteDepot() {
  for (auto it = incomplete_depots_.begin(); it != incomplete_depots_.end();) {
    if ((*it)->isCompleted()) {
      depots_.insert(*it);
      it = incomplete_depots_.erase(it);
    }
    else {
      ++it;
    }
  }
}

//find minerals near depot and add them to depot_2_minerals_
void ResourceDepotCenter::AddMineralsNearDepot(BWAPI::Unit depot) {
	const BWAPI::Unitset minerals_near_depot = depot->getUnitsInRadius(64, BWAPI::Filter::IsMineralField);
  depot_2_minerals_[depot] = minerals_near_depot;
}

int ResourceDepotCenter::GetCompleteDepotTotalCount() {
	return depots_.size();
}

int ResourceDepotCenter::GetIncompleteDepotTotalCount() {
	return incomplete_depots_.size();
}

void ResourceDepotCenter::AddNewDepot(BWAPI::Unit depot) {
  if (depot->isCompleted()) {
    depots_.insert(depot);

    AddMineralsNearDepot(depot);
  } else {
    incomplete_depots_.insert(depot);
  }
}

void ResourceDepotCenter::DestroyDepot(BWAPI::Unit depot) {
  depots_.erase(depot);
	incomplete_depots_.erase(depot);
  depot_2_minerals_.erase(depot);
}

int ResourceDepotCenter::ComputeMineralsWorkerNum(BWAPI::Unit depot) {
  const BWAPI::Unitset worker_near_depot = depot->getUnitsInRadius(64, BWAPI::Filter::IsWorker);
	int num = 0;
  for (auto worker_tmp : worker_near_depot) {
    if (WorkerCenter::Instance().GetWorkerJob(worker_tmp) == WorkerCenter::kMinerals) {
      ++num;
    }
  }
	return num;
}

BWAPI::Unit ResourceDepotCenter::GetNearestDepot(BWAPI::Unit worker) {
	int min_distance = 999999;
  BWAPI::Unit depot = nullptr;
  for (auto depot_tmp : depots_) {
	  const int distance = depot_tmp->getDistance(worker);
    if (distance < min_distance) {
      min_distance = distance;
			depot = depot_tmp;
    }
  }

	return depot;
}

bool ResourceDepotCenter::IsDepotMineralWorkerFull(BWAPI::Unit depot) {
	const int depot_minerals_num = depot_2_minerals_.size();
	const int minerals_worker_num = ComputeMineralsWorkerNum(depot);
  return depot_minerals_num * 2 <= minerals_worker_num;
}

void ResourceDepotCenter::Update() {
  ShiftCompleteDepot();
}

ResourceDepotCenter& ResourceDepotCenter::Instance() {
  static ResourceDepotCenter instance;
  return instance;
}

void ResourceDepotCenter::Clear() {
  incomplete_depots_.clear();
  depots_.clear();
  depot_2_minerals_.clear();
}
