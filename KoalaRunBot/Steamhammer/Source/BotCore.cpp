#include "BotCore.h"
#include "Bases.h"
#include "Common.h"
#include "OpponentModel.h"
#include "ParseUtils.h"

using namespace KoalaRunBot;


BotCore::BotCore()
	: the(The::Root())
{
}

// This gets called when the bot starts.
void BotCore::onStart()
{
	the.Initialize();

	// Initialize BOSS, the Build Order Search System
	BOSS::init();

	// Call BWTA to read and analyze the current map.
	// Very slow if the map has not been seen before, so that info is not cached.
	BWTA::readMap();
	BWTA::analyze();

	// Our own map analysis.
	Bases::Instance().initialize();

	// Parse the bot's configuration file.
	// Change this file path to point to your config file.
	// Any relative path name will be relative to Starcraft installation folder
	// The config depends on the map and must be read after the map is analyzed.
	ParseUtils::ParseConfigFile(Config::ConfigFile::ConfigFileLocation);

	// Set our BWAPI options according to the configuration. 
	BWAPI::Broodwar->setLocalSpeed(Config::BWAPIOptions::SetLocalSpeed);
	BWAPI::Broodwar->setFrameSkip(Config::BWAPIOptions::SetFrameSkip);

	if (Config::BWAPIOptions::EnableCompleteMapInformation)
	{
		BWAPI::Broodwar->enableFlag(BWAPI::Flag::CompleteMapInformation);
	}

	if (Config::BWAPIOptions::EnableUserInput)
	{
		BWAPI::Broodwar->enableFlag(BWAPI::Flag::UserInput);
	}

	StrategyManager::Instance().setOpeningGroup();    // may depend on config and/or opponent model

	if (Config::BotInfo::PrintInfoOnStart)
	{
		BWAPI::Broodwar->printf("%s by %s, based on SteamHammer.", Config::BotInfo::BotName.c_str(), Config::BotInfo::Authors.c_str());
	}

	// Turn off latency compensation, which is on by default.
	// BWAPI::Broodwar->setLatCom(false);

	// Turn on latency compensation, in case somebody sets it off by default.
	BWAPI::Broodwar->setLatCom(true);
}

void BotCore::onEnd(bool isWinner)
{
	OpponentModel::Instance().setWin(isWinner);
	OpponentModel::Instance().write();
}

void BotCore::onFrame()
{
	if (!Config::ConfigFile::ConfigFileFound)
	{
		BWAPI::Broodwar->drawBoxScreen(0, 0, 450, 100, BWAPI::Colors::Black, true);
		BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
		BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Not Found", kRed, Config::BotInfo::BotName.c_str());
		BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
		BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without its configuration file", kWhite, Config::BotInfo::BotName.c_str());
		BWAPI::Broodwar->drawTextScreen(10, 45, "%cCheck that the file below exists. Incomplete paths are relative to Starcraft directory", kWhite);
		BWAPI::Broodwar->drawTextScreen(10, 60, "%cYou can change this file location in Config::ConfigFile::ConfigFileLocation", kWhite);
		BWAPI::Broodwar->drawTextScreen(10, 75, "%cFile Not Found (or is empty): %c %s", kWhite, kGreen, Config::ConfigFile::ConfigFileLocation.c_str());
		return;
	}
	else if (!Config::ConfigFile::ConfigFileParsed)
	{
		BWAPI::Broodwar->drawBoxScreen(0, 0, 450, 100, BWAPI::Colors::Black, true);
		BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Huge);
		BWAPI::Broodwar->drawTextScreen(10, 5, "%c%s Config File Parse Error", kRed, Config::BotInfo::BotName.c_str());
		BWAPI::Broodwar->setTextSize(BWAPI::Text::Size::Default);
		BWAPI::Broodwar->drawTextScreen(10, 30, "%c%s will not run without a properly formatted configuration file", kWhite, Config::BotInfo::BotName.c_str());
		BWAPI::Broodwar->drawTextScreen(10, 45, "%cThe configuration file was found, but could not be parsed. Check that it is valid JSON", kWhite);
		BWAPI::Broodwar->drawTextScreen(10, 60, "%cFile Not Parsed: %c %s", kWhite, kGreen, Config::ConfigFile::ConfigFileLocation.c_str());
		return;
	}

	GameCommander::Instance().Update();
}

void BotCore::onUnitDestroy(BWAPI::Unit unit)
{
	GameCommander::Instance().OnUnitDestroy(unit);
}

void BotCore::onUnitMorph(BWAPI::Unit unit)
{
	GameCommander::Instance().OnUnitMorph(unit);
}

void BotCore::onSendText(std::string text)
{
	ParseUtils::ParseTextCommand(text);
}

void BotCore::onUnitCreate(BWAPI::Unit unit)
{
	GameCommander::Instance().OnUnitCreate(unit);
}

void BotCore::onUnitComplete(BWAPI::Unit unit)
{
	GameCommander::Instance().OnUnitComplete(unit);
}

void BotCore::onUnitShow(BWAPI::Unit unit)
{
	GameCommander::Instance().OnUnitShow(unit);
}

void BotCore::onUnitHide(BWAPI::Unit unit)
{
	GameCommander::Instance().OnUnitHide(unit);
}

void BotCore::onUnitRenegade(BWAPI::Unit unit)
{
	GameCommander::Instance().OnUnitRenegade(unit);
}
