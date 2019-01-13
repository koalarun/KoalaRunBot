#include "UnitChangeHandle.h"
#include "WorkerCenter.h"
#include "ResourceDepotCenter.h"

using namespace KoalaRunBot;

void UnitChangeHandle::OnUnitCreate(BWAPI::Unit unit) {
  if (BWAPI::Broodwar->self() == unit->getPlayer()) {
    if (unit->getType().isWorker()) {
      WorkerCenter::Instance().AddNewWorker(unit);
      return;
    }

    if (unit->getType().isResourceDepot()) {
	    ResourceDepotCenter::Instance().AddNewDepot(unit);
			return;
    }


    return;
  }
}

void UnitChangeHandle::OnUnitDestroy(BWAPI::Unit unit) {
  if (BWAPI::Broodwar->self() == unit->getPlayer()) {
    if (unit->getType().isWorker()) {
      WorkerCenter::Instance().DestroyWorker(unit);
			return;
    }

    if (unit->getType().isResourceDepot()) {
      ResourceDepotCenter::Instance().DestroyDepot(unit);
			return;
    }

  }
}

void UnitChangeHandle::OnUnitShow(BWAPI::Unit unit) {


  // if (BWAPI::Broodwar->self()->isEnemy(unit->getPlayer())) {
  //   return;
  // }
  //
  // if (BWAPI::Broodwar->self()->isAlly(unit->getPlayer())){
  //   return;
  // }
  //
  // if (unit->getPlayer()->isNeutral()) {
  //   return;
  // }

}

void UnitChangeHandle::OnUnitHide(BWAPI::Unit unit) {
	
}

void UnitChangeHandle::OnUnitMorph(BWAPI::Unit unit) {
  if (BWAPI::Broodwar->self() == unit->getPlayer()) {
    // if (unit->getType().isWorker()) {
    //   WorkerCenter::Instance().AddNewWorker(unit);
    //   return;
    // }

    if (unit->getType().isBuilding())
    {
      if (unit->getPlayer()->getRace() == BWAPI::Races::Zerg)
        WorkerCenter::Instance().DestroyWorker(unit);
    }

    return;
  }


}

void UnitChangeHandle::OnUnitComplete(BWAPI::Unit unit) {
	
}

void UnitChangeHandle::OnUnitRenegade(BWAPI::Unit unit) {
	
}
