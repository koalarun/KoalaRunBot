#pragma once;

#include "Common.h"

namespace KoalaRunBot
{
class MicroManager;

class MicroHighTemplar : public MicroManager
{
public:

	MicroHighTemplar();
	void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);
	void update();
};
}