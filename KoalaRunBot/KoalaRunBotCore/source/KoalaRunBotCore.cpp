#include "KoalaRunBotCore.h"

using namespace KoalaRunBot;
using namespace BWAPI;


KoalaRunBotCore::KoalaRunBotCore() {}

// This gets called when the bot starts.
void KoalaRunBotCore::onStart() {
  //Broodwar->setLocalSpeed(42);
  Broodwar->setLocalSpeed(0);
  Broodwar->setFrameSkip(0);

  Broodwar->enableFlag(BWAPI::Flag::UserInput);
  // Broodwar->printf("--printf--: Hello world!");
  // Broodwar->sendText("--sendText--: Hello world!");
  Broodwar->printf("latency frames:%d", Broodwar->getLatencyFrames());


  // Set the command optimization level so that common commands can be grouped
  // and reduce the bot's APM (Actions Per Minute).
  Broodwar->setCommandOptimizationLevel(2);

  // Turn on latency compensation, in case somebody sets it off by default.
  BWAPI::Broodwar->setLatCom(true);

  //when debug, draw some additional info
  // if (Broodwar->isDebug()) {
  //   Broodwar->setGUI(true);
  // }
  // else {
  //   Broodwar->setGUI(false);
  // }
}

void KoalaRunBotCore::onEnd(bool isWinner) { }

void KoalaRunBotCore::BuildSupply(int supply_in_build_count, Error last_err) {
  //supply
  static int supply_last_checked = 0;
  UnitType supply_type = Broodwar->self()->getRace().getSupplyProvider();
  // if (Broodwar->self()->minerals() > supply_type->)
  if (last_err == Errors::Insufficient_Supply &&
    supply_last_checked + 200 < Broodwar->getFrameCount() &&
    supply_in_build_count == 0) {

    supply_last_checked = Broodwar->getFrameCount();
    UnitType supply_builder_type = supply_type.whatBuilds().first;
    //zerg
    if (supply_builder_type == UnitTypes::Zerg_Larva) {
      //find a larva
      Unit larva = Broodwar->self()->getUnits().getLarva().getClosestUnit();
      larva->train(supply_type);
    }
    else {
      //find a worker
      Unit supply_builder = Broodwar->self()->getUnits().getClosestUnit(Filter::IsWorker &&
        (Filter::IsIdle || Filter::IsGatheringMinerals));
      if (supply_builder != nullptr) {
        const TilePosition build_location = Broodwar->getBuildLocation(supply_type, supply_builder->getTilePosition());
        if (build_location != TilePositions::Unknown) {
          supply_builder->build(supply_type, build_location);
        }
      }
    }
  }
}

Error KoalaRunBotCore::GetAndShowLastError(const Unitset::value_type& u) {
  Error last_err = Broodwar->getLastError();
  Position pos = u->getPosition();
  Broodwar->registerEvent([pos, last_err](Game*) {
                            Broodwar->drawTextMap(pos, "%c%s", Text::White, last_err.c_str());
                          }, // action
                          nullptr, // condition
                          Broodwar->getLatencyFrames());
  return last_err;
}

void KoalaRunBotCore::onFrame() {
  //Broodwar << "--<--: Hello world!"; ?
  Broodwar->drawTextScreen(200, 0, "FPS:%d  APM:%d", Broodwar->getFPS(), Broodwar->getAPM());
  Broodwar->drawTextScreen(200, 10, "Elapsed Time:%d Frame Count:%d", Broodwar->elapsedTime(),
                           Broodwar->getFrameCount());
  // int count = Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Drone);
  // Broodwar->drawTextScreen(200, 0, "incomplete unit drone:%d", count);
  //test 1: send worker to mineral

  // Prevent spamming by only running our onFrame once every number of latency frames.
  // Latency frames are the number of frames before commands are processed.
  // if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
  //   return;

  Unitset depots;
  Unitset workers;
  Unit pool = nullptr;
  int carry_gas_worker_count = 0;
  int supply_in_build_count = 0;

  for (auto& u : Broodwar->self()->getUnits()) {
    if (!u->exists())
      continue;

    if (u->getType().isResourceDepot()) {
      depots.insert(u);
    }
    else if (u->getType().isWorker()) {
      workers.insert(u);
      if (u->isGatheringGas()) {
        ++carry_gas_worker_count;
      }
    }
    else if (u->getType() == UnitTypes::Zerg_Egg && u->getBuildType() == UnitTypes::Zerg_Overlord) {
      ++supply_in_build_count;
    }
    else if ((u->getType() == UnitTypes::Protoss_Pylon || u->getType() == UnitTypes::Terran_Supply_Depot) && u->
      isBeingConstructed()) {
      ++supply_in_build_count;
    }
    else if (u->getType() == UnitTypes::Zerg_Spawning_Pool) {
      pool = u;
    }
  }


  Error last_err;
  for (auto& u : Broodwar->self()->getUnits()) {
    if (!u->exists())
      continue;

    //resource depot product worker
    if (u->getType().isResourceDepot()) {
      if (u->isIdle() && workers.size() < 19) {
        const bool train_worker_success = u->train(u->getType().getRace().getWorker());
        if (!train_worker_success) {
          last_err = GetAndShowLastError(u);
        }
      }
    }
    else if (u->getType().isWorker()) {
      //worker to gather mineral
      if (u->isIdle()) {
        if (u->isCarryingGas() || u->isCarryingMinerals()) {
          u->returnCargo();
        }
        else {
          //TODO improve to find optimal mineral field
          u->gather(u->getClosestUnit(Filter::IsMineralField));
        }
      }
    }
  }

  BuildSupply(supply_in_build_count, last_err);

  if (Broodwar->self()->getRace() == Races::Zerg) {
    bool is_build_pool = IsZergAlreadyBuildPool();
    //build pool
    static int pool_last_check = 0;
    if (pool_last_check + 200 < Broodwar->getFrameCount() &&
      !is_build_pool &&
      Broodwar->canMake(UnitTypes::Zerg_Spawning_Pool)) {

      pool_last_check = Broodwar->getFrameCount();

      //find a worker
      const auto spawning_pool_builder = Broodwar->self()->getUnits().getClosestUnit(Filter::IsWorker &&
        (Filter::IsIdle || Filter::IsGatheringMinerals));

      if (spawning_pool_builder != nullptr) {
        const auto build_location = Broodwar->getBuildLocation(UnitTypes::Zerg_Spawning_Pool,
                                                               spawning_pool_builder->getTilePosition());
        if (build_location != TilePositions::Unknown) {
          spawning_pool_builder->build(UnitTypes::Zerg_Spawning_Pool, build_location);
        }
      }
    }

    //build Extractor
    static int extractor_last_check = 0;
    if (extractor_last_check + 200 < Broodwar->getFrameCount() &&
      is_build_pool &&
      Broodwar->self()->completedUnitCount(UnitTypes::Zerg_Extractor) <= 0 &&
      Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Extractor) <= 0 &&
      Broodwar->canMake(UnitTypes::Zerg_Extractor)) {
      
      extractor_last_check = Broodwar->getFrameCount();

      //find a worker
      const auto extractor_pool_builder = Broodwar->self()->getUnits().getClosestUnit(Filter::IsWorker &&
        (Filter::IsIdle || Filter::IsGatheringMinerals));

      if (extractor_pool_builder != nullptr) {
        const auto build_location = Broodwar->getBuildLocation(UnitTypes::Zerg_Extractor,
          extractor_pool_builder->getTilePosition());
        if (build_location != TilePositions::Unknown) {
          extractor_pool_builder->build(UnitTypes::Zerg_Extractor, build_location);
        }
      }
    }


    //build another depot
    static int hatchery_last_check = 0;
    if (depots.size() < 5 &&
      hatchery_last_check + 200 < Broodwar->getFrameCount() &&
      Broodwar->canMake(UnitTypes::Zerg_Hatchery)) {
      hatchery_last_check = Broodwar->getFrameCount();

      //find a worker
      const auto hatchery_builder = Broodwar->self()->getUnits().getClosestUnit(Filter::IsWorker &&
        (Filter::IsIdle || Filter::IsGatheringMinerals));

      if (hatchery_builder != nullptr) {
        const auto build_location = Broodwar->getBuildLocation(UnitTypes::Zerg_Hatchery,
                                                               hatchery_builder->getTilePosition());
        if (build_location != TilePositions::Unknown) {
          hatchery_builder->build(UnitTypes::Zerg_Hatchery, build_location);
        }
      }
    }

    //pool ok, upgrade metabolic boost
    static int metabolic_boost_last_check = 0;
    if (metabolic_boost_last_check + 200 < Broodwar->getFrameCount() &&
      Broodwar->self()->getUpgradeLevel(UpgradeTypes::Metabolic_Boost) == 0 &&
      !Broodwar->self()->isUpgrading(UpgradeTypes::Metabolic_Boost) &&
      Broodwar->canUpgrade(UpgradeTypes::Metabolic_Boost)) {

      metabolic_boost_last_check = Broodwar->getFrameCount();
      //find the pool
      
      //Unit pool = Broodwar->self()->getUnits().getClosestUnit(Filter::IsBuilding);
      pool->upgrade(UpgradeTypes::Metabolic_Boost);
    }

    //extractor ok, worker gather gas
    if (Broodwar->self()->completedUnitCount(UnitTypes::Zerg_Extractor) == 1 &&
      carry_gas_worker_count < 3) {
      const auto gas_gather_worker = Broodwar->self()->getUnits().getClosestUnit(Filter::IsWorker &&
        (Filter::IsIdle || Filter::IsGatheringMinerals));
      gas_gather_worker->gather(gas_gather_worker->getClosestUnit(Filter::IsRefinery));
    }

    //pool ok, hatchery = 2, train zergling
    if (Broodwar->self()->completedUnitCount(UnitTypes::Zerg_Spawning_Pool) == 1 &&
      depots.size() >= 2) {
      for (auto depot : depots) {
        const bool train_success = depot->train(UnitTypes::Zerg_Zergling);
        if (!train_success) {
          last_err = GetAndShowLastError(depot);
          BuildSupply(supply_in_build_count, last_err);
        }
        
      }
    }

  }


  //build barracks
  //Broodwar->self()->get
  // for (auto region : Broodwar->getAllRegions()) {
  //   Broodwar->drawLineMap(region->getBoundsLeft(), region->getBoundsBottom(), 
  //     region->getBoundsRight(), region->getBoundsBottom(), BWAPI::Colors::Blue);
  //
  //   Broodwar->drawLineMap(region->getBoundsRight(), region->getBoundsBottom(),
  //     region->getBoundsRight(), region->getBoundsTop(), BWAPI::Colors::Blue);
  // }


}

bool KoalaRunBotCore::IsZergAlreadyBuildPool() {
  return Broodwar->self()->completedUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0 ||
    Broodwar->self()->incompleteUnitCount(UnitTypes::Zerg_Spawning_Pool) > 0;
}

void KoalaRunBotCore::onUnitDestroy(BWAPI::Unit unit) {}

void KoalaRunBotCore::onUnitMorph(BWAPI::Unit unit) {}

void KoalaRunBotCore::onSendText(std::string text) {}

void KoalaRunBotCore::onUnitCreate(BWAPI::Unit unit) {}

void KoalaRunBotCore::onUnitComplete(BWAPI::Unit unit) {}

void KoalaRunBotCore::onUnitShow(BWAPI::Unit unit) {}

void KoalaRunBotCore::onUnitHide(BWAPI::Unit unit) {}

void KoalaRunBotCore::onUnitRenegade(BWAPI::Unit unit) {}
