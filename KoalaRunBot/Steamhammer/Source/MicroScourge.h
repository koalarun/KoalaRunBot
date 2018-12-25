#pragma once;

#include "MicroManager.h"

namespace KoalaRunBot
{
	class MicroScourge : public MicroManager
	{
	public:
		MicroScourge();

		void executeMicro(const BWAPI::Unitset & targets, const UnitCluster & cluster);
		void assignTargets(const BWAPI::Unitset & rangedUnits, const BWAPI::Unitset & targets);

		static int getAttackPriority(BWAPI::UnitType targetType);
		BWAPI::Unit getTarget(BWAPI::Unit rangedUnit, const BWAPI::Unitset & targets);
	};
}