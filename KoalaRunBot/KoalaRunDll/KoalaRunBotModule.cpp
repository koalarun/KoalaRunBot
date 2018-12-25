/* 
 *----------------------------------------------------------------------
 * Steamhammer entry point.
 *----------------------------------------------------------------------
 */


#include "KoalaRunBotModule.h"


using namespace KoalaRunBot;


// This gets called when the bot starts.
void KoalaRunBotModule::onStart()
{
	bot_core_.onStart();
}

void KoalaRunBotModule::onEnd(bool isWinner)
{
	bot_core_.onEnd(isWinner);
}

void KoalaRunBotModule::onFrame()
{
	bot_core_.onFrame();
}

void KoalaRunBotModule::onUnitDestroy(BWAPI::Unit unit)
{
	bot_core_.onUnitDestroy(unit);
}

void KoalaRunBotModule::onUnitMorph(BWAPI::Unit unit)
{
	bot_core_.onUnitMorph(unit);
}

void KoalaRunBotModule::onSendText(std::string text) 
{ 
	bot_core_.onSendText(text);
}

void KoalaRunBotModule::onUnitCreate(BWAPI::Unit unit)
{ 
	bot_core_.onUnitCreate(unit);
}

void KoalaRunBotModule::onUnitComplete(BWAPI::Unit unit)
{
	bot_core_.onUnitComplete(unit);
}

void KoalaRunBotModule::onUnitShow(BWAPI::Unit unit)
{
	bot_core_.onUnitShow(unit);
}

void KoalaRunBotModule::onUnitHide(BWAPI::Unit unit)
{ 
	bot_core_.onUnitHide(unit);
}

void KoalaRunBotModule::onUnitRenegade(BWAPI::Unit unit)
{ 
	bot_core_.onUnitRenegade(unit);
}