#include "KoalaRunBotCore.h"

using namespace KoalaRunBot;
using namespace BWAPI;


KoalaRunBotCore::KoalaRunBotCore() {}

// This gets called when the bot starts.
void KoalaRunBotCore::onStart() {
  //Broodwar->setLocalSpeed(42);
  Broodwar->setLocalSpeed(10);
  Broodwar->setFrameSkip(0);

  Broodwar->enableFlag(BWAPI::Flag::UserInput);
  Broodwar->printf("--printf--: Hello world!");
  Broodwar->sendText("--sendText--: Hello world!");


  // Set the command optimization level so that common commands can be grouped
  // and reduce the bot's APM (Actions Per Minute).
  Broodwar->setCommandOptimizationLevel(2);

  // Turn on latency compensation, in case somebody sets it off by default.
  BWAPI::Broodwar->setLatCom(true);
}

void KoalaRunBotCore::onEnd(bool isWinner) { }

void KoalaRunBotCore::onFrame() {
  //Broodwar << "--<--: Hello world!"; ?
  Broodwar->drawTextScreen(200, 0, "--drawTextScreen--: FPS: %d", BWAPI::Broodwar->getFPS());

  //test 1: send worker to mineral

  // Prevent spamming by only running our onFrame once every number of latency frames.
  // Latency frames are the number of frames before commands are processed.
  if (Broodwar->getFrameCount() % Broodwar->getLatencyFrames() != 0)
    return;

  Error last_err;
  for (auto& u : Broodwar->self()->getUnits()) {
    if (!u->exists())
      continue;

    //resource depot product worker
    if (u->getType().isResourceDepot()) {
      if (u->isIdle()) {
        const bool train_worker_success = u->train(u->getType().getRace().getWorker());
        if (!train_worker_success) {
          last_err = Broodwar->getLastError();
          Position pos = u->getPosition();
          Broodwar->registerEvent([pos, last_err](Game*) {
                                    Broodwar->drawTextMap(pos, "%c%s", Text::White, last_err.c_str());
                                  }, // action
                                  nullptr, // condition
                                  Broodwar->getLatencyFrames());
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
          u->gather(u->getClosestUnit(Filter::IsMineralField || Filter::IsRefinery));
        }
      }
    }
  }

  //supply
  static int lastChecked = 0;
  UnitType supply_type = Broodwar->self()->getRace().getSupplyProvider();
  if (last_err == Errors::Insufficient_Supply &&
    lastChecked + 72 < Broodwar->getFrameCount() &&
    Broodwar->self()->incompleteUnitCount(supply_type) == 0) {

    lastChecked = Broodwar->getFrameCount();

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

  //build barracks
  //Broodwar->self()->get
  for (auto region : Broodwar->getAllRegions()) {
    BWAPI::Broodwar->drawLineMap(region->getBoundsLeft(), region->getBoundsBottom(), 
      region->getBoundsRight(), region->getBoundsBottom(), BWAPI::Colors::Blue);

    BWAPI::Broodwar->drawLineMap(region->getBoundsRight(), region->getBoundsBottom(),
      region->getBoundsRight(), region->getBoundsTop(), BWAPI::Colors::Blue);
  }
  
  
}


void KoalaRunBotCore::onUnitDestroy(BWAPI::Unit unit) {}

void KoalaRunBotCore::onUnitMorph(BWAPI::Unit unit) {}

void KoalaRunBotCore::onSendText(std::string text) {}

void KoalaRunBotCore::onUnitCreate(BWAPI::Unit unit) {}

void KoalaRunBotCore::onUnitComplete(BWAPI::Unit unit) {}

void KoalaRunBotCore::onUnitShow(BWAPI::Unit unit) {}

void KoalaRunBotCore::onUnitHide(BWAPI::Unit unit) {}

void KoalaRunBotCore::onUnitRenegade(BWAPI::Unit unit) {}
