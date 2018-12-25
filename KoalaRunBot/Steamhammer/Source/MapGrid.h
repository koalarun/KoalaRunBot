#pragma once

#include "Common.h"
#include "MicroManager.h"

namespace KoalaRunBot
{
	class The;

	class GridCell
	{
	public:

		int time_last_visited_;
		int time_last_opponent_seen_;
		int time_last_scan_;
		BWAPI::Unitset our_units_;
		BWAPI::Unitset opp_units_;
		BWAPI::Position center_;

		// Not the ideal place for this constant, but this is where it is used.
		const static int kScanDuration = 240; // approximate time that a comsat scan provides vision

		GridCell()
			: time_last_visited_(0)
			  , time_last_opponent_seen_(0)
			  , time_last_scan_(-kScanDuration)
		{
		}
	};

	class MapGrid
	{
		MapGrid();
		MapGrid(int map_width, int map_height, int cell_size);

		The& the_;

		int map_width_, map_height_;
		int cell_size_;
		int rows_, cols_;
		//int last_updated_;

		std::vector<GridCell> cells_;

		void CalculateCellCenters();

		void ClearGrid();
		BWAPI::Position GetCellCenter(int x, int y);

	public:

		// yay for singletons!
		static MapGrid& Instance();

		void update();
		void GetUnits(BWAPI::Unitset& units, BWAPI::Position center, int radius, bool our_units, bool opp_units);
		BWAPI::Position GetLeastExplored() { return GetLeastExplored(false, 1); };
		BWAPI::Position GetLeastExplored(bool by_ground, int map_partition);

		GridCell& GetCellByIndex(const int r, const int c) { return cells_[r * cols_ + c]; }

		GridCell& GetCell(const BWAPI::Position pos)
		{
			return GetCellByIndex(pos.y / cell_size_, pos.x / cell_size_);
		}

		GridCell& GetCell(const BWAPI::Unit unit) { return GetCell(unit->getPosition()); }

		// Track comsat scans so we don't scan the same place again too soon.
		void ScanAtPosition(const BWAPI::Position& pos);
		bool ScanIsActiveAt(const BWAPI::Position& pos);
	};
}
