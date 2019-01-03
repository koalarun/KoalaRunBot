#include <BWAPI.h>
#include <BWAPI/Client.h>

#include <iostream>
#include <thread>
#include <chrono>
#include "BotCore.h"
#include <KoalaRunBotCore.h>

using namespace BWAPI;
using namespace KoalaRunBot;

//Note:
//If one game is finish, and you want to start a new game ,the client must be restart.
//Because many class(GameCommander,InformationManager) is used Instance() to call their static instance, 
//and some of their properties' memory address in the first game can't be accessed in next game(IE. InformationManager.resourceDepot).
//Initialize them again when new game start can solve this problem. 

void connectLoop() {
  while (!BWAPIClient.connect()) {
    std::this_thread::sleep_for(std::chrono::milliseconds{2000});
  }
}

void gameLoop() {
  // std::shared_ptr<KoalaRunBotCore> bot_core(new KoalaRunBotCore());
  std::shared_ptr<KoalaRunBotCore> bot_core(new KoalaRunBotCore());

  while (Broodwar->isInGame()) {
    for (auto& e : Broodwar->getEvents()) {
      switch (e.getType()) {
        case EventType::MatchStart:
          bot_core->onStart();
          break;
        case EventType::MatchEnd:
          bot_core->onEnd(e.isWinner());
          break;
        case EventType::MatchFrame:
          bot_core->onFrame();
          break;
        case EventType::SendText:
          bot_core->onSendText(e.getText());
          break;
        case EventType::UnitCreate:
          bot_core->onUnitCreate(e.getUnit());
          break;
        case EventType::UnitDestroy:
          bot_core->onUnitDestroy(e.getUnit());
          break;
        case EventType::UnitMorph:
          bot_core->onUnitMorph(e.getUnit());
          break;
        case EventType::UnitShow:
          bot_core->onUnitShow(e.getUnit());
          break;
        case EventType::UnitHide:
          bot_core->onUnitHide(e.getUnit());
          break;
        case EventType::UnitRenegade:
          bot_core->onUnitRenegade(e.getUnit());
          break;
        default:
          break;
      }
    }

    BWAPIClient.update();

    if (!BWAPIClient.isConnected()) {
      std::cout << "Reconnecting..." << std::endl;
      connectLoop();
    }
  }
}

int main(int argc, const char* argv[]) {
  std::cout << "Connecting..." << std::endl;;
  connectLoop();

  //application loop
  while (true) {
    std::cout << "waiting to enter match" << std::endl;
    while (!Broodwar->isInGame()) {
      BWAPIClient.update();
      if (!BWAPIClient.isConnected()) {
        std::cout << "Reconnecting..." << std::endl;;
        connectLoop();
      }
    }
    std::cout << "starting match!" << std::endl;

    //game loop
    gameLoop();
  }

  std::cout << "Press ENTER to continue..." << std::endl;
  std::cin.ignore();
  return 0;
}



