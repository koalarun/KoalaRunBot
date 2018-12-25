#include "KoalaRunBotCore.h"

using namespace KoalaRunBot;
using namespace BWAPI;


KoalaRunBotCore::KoalaRunBotCore() {}

// This gets called when the bot starts.
void KoalaRunBotCore::onStart() {
  BWAPI::Broodwar->setLocalSpeed(42);
  BWAPI::Broodwar->setFrameSkip(0);

  BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
  BWAPI::Broodwar->printf("--printf--: Hello world!");
  BWAPI::Broodwar->sendText("--sendText--: Hello world!");
  BWAPI::Broodwar << "--<--: Hello world!";
  BWAPI::Broodwar->drawTextScreen(200, 0, "--drawTextScreen--: FPS: %d", BWAPI::Broodwar->getFPS());

  // Set the command optimization level so that common commands can be grouped
  // and reduce the bot's APM (Actions Per Minute).
  BWAPI::Broodwar->setCommandOptimizationLevel(2);

  // Turn on latency compensation, in case somebody sets it off by default.
  //BWAPI::Broodwar->setLatCom(true);
}

void KoalaRunBotCore::onEnd(bool isWinner) { }

void KoalaRunBotCore::onFrame() {
  //test 1: send worker to mineral

  // Prevent spamming by only running our onFrame once every number of latency frames.
  // Latency frames are the number of frames before commands are processed.
  if (BWAPI::Broodwar->getFrameCount() % BWAPI::Broodwar->getLatencyFrames() != 0)
    return;

  for (auto& u : Broodwar->self()->getUnits()) {
    if (!u->exists())
      continue;
    if (!u->getType().isWorker())
      continue;
    if (u->isIdle()) {
      if (u->isCarryingGas() || u->isCarryingMinerals()) {
        u->returnCargo();
      } else {
        u->gather(u->getClosestUnit(Filter::IsMineralField || Filter::IsRefinery));
      }
    }


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
