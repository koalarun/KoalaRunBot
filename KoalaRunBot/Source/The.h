#pragma once

#include "MapPartitions.h"
#include "Micro.h"
#include "OpsBoss.h"

namespace KoalaRunBot
{
    class The
    {
    public:
        The() = default;
        void Initialize();

        OpsBoss ops_boss_;

        MapPartitions map_partitions_;

        Micro micro_;

        static The& Root();
    };
}
