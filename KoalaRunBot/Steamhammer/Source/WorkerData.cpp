#include "WorkerData.h"
#include "Micro.h"
#include "The.h"

using namespace KoalaRunBot;

WorkerData::WorkerData()
	: the_(The::Root()) {
	for (const auto unit : BWAPI::Broodwar->getAllUnits()) {
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)) {
			mineral_patch_2_worker_num_[unit] = 0;
		}
	}
}

void WorkerData::WorkerDestroyed(const BWAPI::Unit unit) {
	if (!unit) { return; }

	ClearPreviousJob(unit);
	workers_.erase(unit);
}

void WorkerData::AddWorker(const BWAPI::Unit unit) {
	if (!unit || !unit->exists()) { return; }

	workers_.insert(unit);
	worker_2_job_[unit] = kDefault;
}

// void WorkerData::AddWorker(const BWAPI::Unit unit, const WorkerJob job, const BWAPI::Unit job_unit) {
//   if (!unit || !unit->exists() || !job_unit || !job_unit->exists()) { return; }
//
//   assert(workers_.find(unit) == workers_.end());
//
//   workers_.insert(unit);
//   SetWorkerJob(unit, job, job_unit);
// }
//
// void WorkerData::AddWorker(const BWAPI::Unit unit, const WorkerJob job, const BWAPI::UnitType job_unit_type) {
//   if (!unit || !unit->exists()) { return; }
//
//   assert(workers_.find(unit) == workers_.end());
//   workers_.insert(unit);
//   SetWorkerJob(unit, job, job_unit_type);
// }

void WorkerData::AddDepot(const BWAPI::Unit unit) {
	if (!unit || !unit->exists()) { return; }

	assert(depots_.find(unit) == depots_.end());
	depots_.insert(unit);
	depot_2_worker_num_[unit] = 0;
}

void WorkerData::DepotDestroyed(const BWAPI::Unit unit) {
	if (!unit) { return; }

	depots_.erase(unit);
	depot_2_worker_num_.erase(unit);

	// re-balance workers in here
	for (auto& worker : workers_) {
		// if a worker was working at this depot
		if (worker_2_depot_[worker] == unit) {
			SetWorkerJob(worker, kIdle, nullptr);
		}
	}
}

void WorkerData::AddWorkerNumOfMineralPatch(const BWAPI::Unit mineral_unit, const int num) {
	if (mineral_patch_2_worker_num_.find(mineral_unit) == mineral_patch_2_worker_num_.end()) {
		mineral_patch_2_worker_num_[mineral_unit] = num;
	}
	else {
		mineral_patch_2_worker_num_[mineral_unit] += num;
	}
}

void WorkerData::SetWorkerJob(const BWAPI::Unit unit, const WorkerJob job, const BWAPI::Unit job_unit) {
	if (!unit || !unit->exists()) { return; }

	ClearPreviousJob(unit);
	worker_2_job_[unit] = job;

	if (job == kMinerals) {
		// increase the number of workers assigned to this nexus
		depot_2_worker_num_[job_unit] += 1;

		// set the mineral the worker is working on
		worker_2_depot_[unit] = job_unit;

		const BWAPI::Unit selected_mineral = GetOptimalMineralForWorker(unit);
		worker_2_mineral_patch_[unit] = selected_mineral;
		AddWorkerNumOfMineralPatch(selected_mineral, 1);

		// right click the mineral to start mining
		the_.micro_.RightClick(unit, selected_mineral);
	}
	else if (job == kGas) {
		// increase the count of workers assigned to this refinery
		refinery_2_worker_num_[job_unit] += 1;

		// set the refinery the worker is working on
		worker_2_refinery_[unit] = job_unit;

		// right click the refinery to start harvesting
		the_.micro_.RightClick(unit, job_unit);
	}
	else if (job == kRepair) {
		// only SCVs can repair
		assert(unit->getType() == BWAPI::UnitTypes::Terran_SCV);

		// set the building the worker is to repair
		worker_2_repair_[unit] = job_unit;

		// start repairing 
		if (!unit->isRepairing()) {
			the_.micro_.Repair(unit, job_unit);
		}
	}
		//	else if (job == Scout)
		//	{
		//
		//	}
	else if (job == kBuild) {
		// BWAPI::Broodwar->printf("Setting worker job to build");
	}
}

void WorkerData::SetWorkerJob(const BWAPI::Unit unit, const WorkerJob job, const BWAPI::UnitType job_unit_type) {
	if (!unit) { return; }

	ClearPreviousJob(unit);
	worker_2_job_[unit] = job;

	if (job == kBuild) {
		worker_2_building_type_[unit] = job_unit_type;
	}

	if (worker_2_job_[unit] != kBuild) {
		//BWAPI::Broodwar->printf("Something went horribly wrong");
	}
}

void WorkerData::SetWorkerJob(const BWAPI::Unit unit, const WorkerJob job, const WorkerMoveData wmd) {
	if (!unit) { return; }

	ClearPreviousJob(unit);
	worker_2_job_[unit] = job;

	if (job == kMove) {
		worker_2_move_data_[unit] = wmd;
	}

	if (worker_2_job_[unit] != kMove) {
		//BWAPI::Broodwar->printf("Something went horribly wrong");
	}
}

void WorkerData::ClearPreviousJob(const BWAPI::Unit unit) {
	if (!unit) { return; }

	const auto previous_job = GetWorkerJob(unit);

	if (previous_job == kMinerals) {
		depot_2_worker_num_[worker_2_depot_[unit]] -= 1;

		worker_2_depot_.erase(unit);

		// remove a worker from this unit's assigned mineral patch
		AddWorkerNumOfMineralPatch(worker_2_mineral_patch_[unit], -1);

		// erase the association from the map
		worker_2_mineral_patch_.erase(unit);
	}
	else if (previous_job == kGas) {
		refinery_2_worker_num_[worker_2_refinery_[unit]] -= 1;
		worker_2_refinery_.erase(unit);
	}
	else if (previous_job == kBuild) {
		worker_2_building_type_.erase(unit);
	}
	else if (previous_job == kRepair) {
		worker_2_repair_.erase(unit);
	}
	else if (previous_job == kMove) {
		worker_2_move_data_.erase(unit);
	}

	worker_2_job_.erase(unit);
}

int WorkerData::GetWorkersTotalNum() const {
	return workers_.size();
}

int WorkerData::ComputeMineralWorkersNum() const {
	size_t num = 0;
	for (auto& unit : workers_) {
		if (worker_2_job_.at(unit) == kMinerals) {
			num++;
		}
	}
	return num;
}

int WorkerData::ComputeGasWorkersNum() const {
	size_t num = 0;
	for (auto& unit : workers_) {
		if (worker_2_job_.at(unit) == kGas) {
			num++;
		}
	}
	return num;
}

int WorkerData::ComputeReturnCargoWorkersNum() const {
	size_t num = 0;
	for (auto& unit : workers_) {
		if (worker_2_job_.at(unit) == kReturnCargo) {
			num++;
		}
	}
	return num;
}

int WorkerData::ComputeCombatWorkersNum() const {
	size_t num = 0;
	for (auto& unit : workers_) {
		if (worker_2_job_.at(unit) == kCombat) {
			num++;
		}
	}
	return num;
}

int WorkerData::ComputeIdleWorkersNum() const {
	size_t num = 0;
	for (auto& unit : workers_) {
		if (worker_2_job_.at(unit) == kIdle) {
			num++;
		}
	}
	return num;
}

enum WorkerData::WorkerJob WorkerData::GetWorkerJob(const BWAPI::Unit unit) {
	if (!unit) { return kDefault; }

	const auto it = worker_2_job_.find(unit);

	if (it != worker_2_job_.end()) {
		return it->second;
	}

	return kDefault;
}

bool WorkerData::IsDepotFull(const BWAPI::Unit depot) {
	if (!depot) { return false; }

	const int assignedWorkers = ComputeAssignedWorkersNum(depot);
	const int mineralsNearDepot = ComputeMineralNumNearDepot(depot);

	return assignedWorkers >= int(Config::Macro::WorkersPerPatch * mineralsNearDepot + 0.5);
}

//todo use a map to store the minerals near depot, to avoid search every time.
BWAPI::Unitset WorkerData::FindMineralPatchesNearDepot(const BWAPI::Unit depot) {
	// if there are minerals near the depot, add them to the set
	BWAPI::Unitset minerals_near_depot;

	const int radius = 300;

	for (auto& unit : BWAPI::Broodwar->getAllUnits()) {
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field) && unit->getDistance(depot) < radius) {
			minerals_near_depot.insert(unit);
		}
	}

	// if we didn't find any, use the whole map
	if (minerals_near_depot.empty()) {
		for (auto& unit : BWAPI::Broodwar->getAllUnits()) {
			if ((unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field)) {
				minerals_near_depot.insert(unit);
			}
		}
	}

	return minerals_near_depot;
}

int WorkerData::ComputeMineralNumNearDepot(const BWAPI::Unit depot) {
	if (!depot) { return 0; }

	int mineralsNearDepot = 0;

	for (auto& unit : BWAPI::Broodwar->getAllUnits()) {
		if ((unit->getType() == BWAPI::UnitTypes::Resource_Mineral_Field) && unit->getDistance(depot) < 200) {
			mineralsNearDepot++;
		}
	}

	return mineralsNearDepot;
}

BWAPI::Unit WorkerData::GetWorkerResource(const BWAPI::Unit worker) {
	if (!worker) { return nullptr; }

	// create the iterator
	std::map<BWAPI::Unit, BWAPI::Unit>::iterator it;

	// if the worker is mining, set the iterator to the mineral map
	if (GetWorkerJob(worker) == kMinerals) {
		it = worker_2_mineral_patch_.find(worker);
		if (it != worker_2_mineral_patch_.end()) {
			return it->second;
		}
	}
	else if (GetWorkerJob(worker) == kGas) {
		it = worker_2_refinery_.find(worker);
		if (it != worker_2_refinery_.end()) {
			return it->second;
		}
	}

	return nullptr;
}

//two condition:
//1.lease workers are assigned to the mineral 
//2.the mineral is nearest to the worker
BWAPI::Unit WorkerData::GetOptimalMineralForWorker(const BWAPI::Unit worker) {
	if (!worker) { return nullptr; }

	// get the depot associated with this unit
	const BWAPI::Unit depot = GetWorkerDepot(worker);
	BWAPI::Unit best_mineral = nullptr;
	auto best_dist = 100000;
	auto best_num_assigned = 10000;

	if (depot) {
		BWAPI::Unitset mineral_patches = FindMineralPatchesNearDepot(depot);

		for (const auto mineral : mineral_patches) {
			const auto dist = mineral->getDistance(depot);
			const auto num_assigned = mineral_patch_2_worker_num_[mineral];

			if (num_assigned < best_num_assigned ||
				num_assigned == best_num_assigned && dist < best_dist) {
				best_mineral = mineral;
				best_dist = dist;
				best_num_assigned = num_assigned;
			}
		}
	}

	return best_mineral;
}

BWAPI::Unit WorkerData::GetWorkerRepairUnit(const BWAPI::Unit unit) {
	if (!unit) { return nullptr; }

	const auto it = worker_2_repair_.find(unit);

	if (it != worker_2_repair_.end()) {
		return it->second;
	}

	return nullptr;
}

BWAPI::Unit WorkerData::GetWorkerDepot(const BWAPI::Unit unit) {
	if (!unit) { return nullptr; }

	const auto it = worker_2_depot_.find(unit);

	if (it != worker_2_depot_.end()) {
		return it->second;
	}

	return nullptr;
}

BWAPI::UnitType WorkerData::GetWorkerBuildingType(const BWAPI::Unit unit) {
	if (!unit) { return BWAPI::UnitTypes::None; }

	const auto it = worker_2_building_type_.find(unit);

	if (it != worker_2_building_type_.end()) {
		return it->second;
	}

	return BWAPI::UnitTypes::None;
}

WorkerMoveData WorkerData::GetWorkerMoveData(const BWAPI::Unit unit) {
	const auto it = worker_2_move_data_.find(unit);

	assert(it != worker_2_move_data_.end());

	return (it->second);
}

int WorkerData::ComputeAssignedWorkersNum(BWAPI::Unit unit) {
	if (!unit) { return 0; }

	std::map<BWAPI::Unit, int>::iterator it;

	// if the worker is mining, set the iterator to the mineral map
	if (unit->getType().isResourceDepot()) {
		it = depot_2_worker_num_.find(unit);

		// if there is an entry, return it
		if (it != depot_2_worker_num_.end()) {
			return it->second;
		}
	}
	else if (unit->getType().isRefinery()) {
		it = refinery_2_worker_num_.find(unit);

		// if there is an entry, return it
		if (it != refinery_2_worker_num_.end()) {
			return it->second;
		}
			// otherwise, we are only calling this on completed refineries, so set it
		else {
			refinery_2_worker_num_[unit] = 0;
		}
	}

	// when all else fails, return 0
	return 0;
}

char WorkerData::GetJobCode(BWAPI::Unit unit) {
	if (!unit) { return 'X'; }

	WorkerData::WorkerJob j = GetWorkerJob(unit);

	if (j == WorkerData::kMinerals) return 'M';
	if (j == WorkerData::kGas) return 'G';
	if (j == WorkerData::kBuild) return 'B';
	if (j == WorkerData::kCombat) return 'C';
	if (j == WorkerData::kIdle) return 'I';
	if (j == WorkerData::kRepair) return 'R';
	if (j == WorkerData::kMove) return '>';
	if (j == WorkerData::kScout) return 'S';
	if (j == WorkerData::kReturnCargo) return '$';
	if (j == WorkerData::kDefault) return '?'; // e.g. incomplete SCV
	return 'X';
}

// Add all gas workers to the given set.
std::set<BWAPI::Unit> WorkerData::FindGasWorkers() {
	std::set<BWAPI::Unit> mw;
	for (auto kv : worker_2_refinery_) {
		mw.insert(kv.first);
	}
	return mw;
}

void WorkerData::DrawDepotDebugInfo() {
	for (const auto depot : depots_) {
		int x = depot->getPosition().x - 64;
		int y = depot->getPosition().y - 32;

		if (Config::Debug::DrawWorkerInfo)
			BWAPI::Broodwar->drawBoxMap(x - 2, y - 1, x + 75, y + 14, BWAPI::Colors::Black,
			                            true);
		if (Config::Debug::DrawWorkerInfo)
			BWAPI::Broodwar->drawTextMap(x, y, "\x04 Workers: %d",
			                             ComputeAssignedWorkersNum(depot));

		BWAPI::Unitset minerals = FindMineralPatchesNearDepot(depot);

		for (const auto mineral : minerals) {
			int x = mineral->getPosition().x;
			int y = mineral->getPosition().y;

			if (mineral_patch_2_worker_num_.find(mineral) != mineral_patch_2_worker_num_.end()) {
				//if (Config::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawBoxMap(x-2, y-1, x+75, y+14, BWAPI::Colors::Black, true);
				//if (Config::Debug::DRAW_UALBERTABOT_DEBUG) BWAPI::Broodwar->drawTextMap(x, y, "\x04 Workers: %d", workersOnMineralPatch[mineral]);
			}
		}
	}
}
