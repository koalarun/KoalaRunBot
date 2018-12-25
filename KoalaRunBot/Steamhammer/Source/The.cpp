#include "The.h"

using namespace KoalaRunBot;

void The::Initialize()
{
    map_partitions_.initialize();

    ops_boss_.Initialize();
}

The& The::Root()
{
    static The root;
    return root;
};
