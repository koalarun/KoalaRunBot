#pragma once
#include <BWAPI/Unitset.h>

namespace KoalaRunBot {
	class ResourceDepotCenter {
	private:
		//depot data
		BWAPI::Unitset incomplete_depots_;
		BWAPI::Unitset depots_;
    std::map<BWAPI::Unit, BWAPI::Unitset> depot_2_minerals_;

    void ShiftCompleteDepot();
    void AddMineralsNearDepot(BWAPI::Unit depot);
	public:
		//get set
		BWAPI::Unitset Depots() const { return depots_; }
		//depot data
		int GetCompleteDepotTotalCount();
		int GetIncompleteDepotTotalCount();
    void AddNewDepot(BWAPI::Unit depot);
    void DestroyDepot(BWAPI::Unit depot);
    int ComputeMineralsWorkerNum(BWAPI::Unit depot);
    BWAPI::Unit GetNearestDepot(BWAPI::Unit worker);
		//depot judgment
    bool IsDepotMineralWorkerFull(BWAPI::Unit depot);

		//depot update
    void Update();

		static ResourceDepotCenter& Instance();
		void Clear();
	};
}
