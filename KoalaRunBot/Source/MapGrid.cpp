#include "Common.h"
#include "MapGrid.h"
#include "Bases.h"
#include "The.h"

using namespace KoalaRunBot;

MapGrid& MapGrid::Instance()
{
    static MapGrid instance(BWAPI::Broodwar->mapWidth() * 32, BWAPI::Broodwar->mapHeight() * 32,
                            Config::Tools::MAP_GRID_SIZE);
    return instance;
}

MapGrid::MapGrid()
    : the_(The::Root())
{
}

MapGrid::MapGrid(const int map_width, const int map_height, const int cell_size)
    : the_(The::Root())
      , map_width_(map_width)
      , map_height_(map_height)
      , cell_size_(cell_size)
      , rows_((map_height + cell_size - 1) / cell_size)
      , cols_((map_width + cell_size - 1) / cell_size)
      //, last_updated_(0)
      , cells_(rows_ * cols_)
{
    CalculateCellCenters();
}

// Return the first of:
// 1. Any starting base location that has not been explored.
// 2. The least-recently explored accessible cell (with attention to byGround).
// Item 1 ensures that, if early game scouting failed, we scout with force.
// If byGround, only locations that are accessible by ground from the given location.
// If not byGround, the location fromHere is not used.
BWAPI::Position MapGrid::GetLeastExplored(const bool by_ground, const int partition)
{
    // 1. Any starting location that has not been explored.
    for (auto tile : BWAPI::Broodwar->getStartLocations())
    {
        if (!BWAPI::Broodwar->isExplored(tile) &&
            (!by_ground || partition == the_.map_partitions_.id(tile)))
        {
            return BWAPI::Position(tile);
        }
    }

    // 2. The most distant of the least-recently explored tiles.
    auto min_seen = 1000000;
    double min_seen_dist = 0;
    auto least_row(0), least_col(0);

    for (auto r = 0; r < rows_; ++r)
    {
        for (auto c = 0; c < cols_; ++c)
        {
            // get the center of this cell
            auto cell_center = GetCellCenter(r, c);

            // Skip places that we can't get to.
            if (by_ground && partition != the_.map_partitions_.id(cell_center))
            {
                continue;
            }

            BWAPI::Position home(BWAPI::Broodwar->self()->getStartLocation());
            const auto dist = home.getDistance(GetCellByIndex(r, c).center_);
            const auto last_visited = GetCellByIndex(r, c).time_last_visited_;
            if (last_visited < min_seen || ((last_visited == min_seen) && (dist > min_seen_dist)))
            {
                least_row = r;
                least_col = c;
                min_seen = GetCellByIndex(r, c).time_last_visited_;
                min_seen_dist = dist;
            }
        }
    }

    return GetCellCenter(least_row, least_col);
}

void MapGrid::CalculateCellCenters()
{
    for (int r = 0; r < rows_; ++r)
    {
        for (int c = 0; c < cols_; ++c)
        {
            GridCell& cell = GetCellByIndex(r, c);

            int centerX = (c * cell_size_) + (cell_size_ / 2);
            int centerY = (r * cell_size_) + (cell_size_ / 2);

            // if the x position goes past the end of the map
            if (centerX > map_width_)
            {
                // when did the last cell start
                int lastCellStart = c * cell_size_;

                // how wide did we go
                int tooWide = map_width_ - lastCellStart;

                // go half the distance between the last start and how wide we were
                centerX = lastCellStart + (tooWide / 2);
            }
            else if (centerX == map_width_)
            {
                centerX -= 50;
            }

            if (centerY > map_height_)
            {
                // when did the last cell start
                int lastCellStart = r * cell_size_;

                // how wide did we go
                int tooHigh = map_height_ - lastCellStart;

                // go half the distance between the last start and how wide we were
                centerY = lastCellStart + (tooHigh / 2);
            }
            else if (centerY == map_height_)
            {
                centerY -= 50;
            }

            cell.center_ = BWAPI::Position(centerX, centerY);
            assert(cell.center_.isValid());
        }
    }
}

BWAPI::Position MapGrid::GetCellCenter(int row, int col)
{
    return GetCellByIndex(row, col).center_;
}

// clear the vectors in the grid
void MapGrid::ClearGrid()
{
    for (size_t i(0); i < cells_.size(); ++i)
    {
        cells_[i].our_units_.clear();
        cells_[i].opp_units_.clear();
    }
}

// Populate the grid with units.
// Include all buildings, but other units only if they are completed.
// For the enemy, only include visible units (InformationManager remembers units which are out of sight).
void MapGrid::update()
{
    if (Config::Debug::DrawMapGrid)
    {
        for (int i = 0; i < cols_; i++)
        {
            BWAPI::Broodwar->drawLineMap(i * cell_size_, 0, i * cell_size_, map_height_, BWAPI::Colors::Blue);
        }

        for (int j = 0; j < rows_; j++)
        {
            BWAPI::Broodwar->drawLineMap(0, j * cell_size_, map_width_, j * cell_size_, BWAPI::Colors::Blue);
        }

        for (int r = 0; r < rows_; ++r)
        {
            for (int c = 0; c < cols_; ++c)
            {
                GridCell& cell = GetCellByIndex(r, c);

                BWAPI::Broodwar->drawTextMap(cell.center_.x, cell.center_.y, "Last Seen %d", cell.time_last_visited_);
                BWAPI::Broodwar->drawTextMap(cell.center_.x, cell.center_.y + 10, "Row/Col (%d, %d)", r, c);
            }
        }
    }

    ClearGrid();

    //BWAPI::Broodwar->printf("MapGrid info: WH(%d, %d)  CS(%d)  RC(%d, %d)  C(%d)", mapWidth, mapHeight, cellSize, rows, cols, cells.size());

    for (const auto unit : BWAPI::Broodwar->self()->getUnits())
    {
        if (unit->isCompleted() || unit->getType().isBuilding())
        {
            GetCell(unit).our_units_.insert(unit);
            GetCell(unit).time_last_visited_ = BWAPI::Broodwar->getFrameCount();
        }
    }

    for (const auto unit : BWAPI::Broodwar->enemy()->getUnits())
    {
        if (unit->exists() &&
            (unit->isCompleted() || unit->getType().isBuilding()) &&
            unit->getHitPoints() > 0 &&
            unit->getType() != BWAPI::UnitTypes::Unknown)
        {
            GetCell(unit).opp_units_.insert(unit);
            GetCell(unit).time_last_opponent_seen_ = BWAPI::Broodwar->getFrameCount();
        }
    }
}

// Return the set of units in the given circle.
void MapGrid::GetUnits(BWAPI::Unitset& units, BWAPI::Position center, int radius, bool ourUnits, bool oppUnits)
{
    const int x0(std::max((center.x - radius) / cell_size_, 0));
    const int x1(std::min((center.x + radius) / cell_size_, cols_ - 1));
    const int y0(std::max((center.y - radius) / cell_size_, 0));
    const int y1(std::min((center.y + radius) / cell_size_, rows_ - 1));
    const int radiusSq(radius * radius);

    for (int y(y0); y <= y1; ++y)
    {
        for (int x(x0); x <= x1; ++x)
        {
            int row = y;
            int col = x;

            const GridCell& cell(GetCellByIndex(row, col));
            if (ourUnits)
            {
                for (const auto unit : cell.our_units_)
                {
                    BWAPI::Position d(unit->getPosition() - center);
                    if (d.x * d.x + d.y * d.y <= radiusSq)
                    {
                        if (!units.contains(unit))
                        {
                            units.insert(unit);
                        }
                    }
                }
            }
            if (oppUnits)
            {
                for (const auto unit : cell.opp_units_)
                    if (unit->getType() != BWAPI::UnitTypes::Unknown)
                    {
                        BWAPI::Position d(unit->getPosition() - center);
                        if (d.x * d.x + d.y * d.y <= radiusSq)
                        {
                            if (!units.contains(unit))
                            {
                                units.insert(unit);
                            }
                        }
                    }
            }
        }
    }
}

// We scanned the given position. Record it so we don't scan the same position
// again before it wears off.
void MapGrid::ScanAtPosition(const BWAPI::Position& pos)
{
    GridCell& cell = GetCell(pos);
    cell.time_last_scan_ = BWAPI::Broodwar->getFrameCount();
}

// Is a comsat scan already active at the given position?
// This implementation is a rough appromimation; it only checks whether an ongoing scan
// is in the same grid cell as the given position.
bool MapGrid::ScanIsActiveAt(const BWAPI::Position& pos)
{
    const GridCell& cell = GetCell(pos);

    return cell.time_last_scan_ + GridCell::kScanDuration > BWAPI::Broodwar->getFrameCount();
}
