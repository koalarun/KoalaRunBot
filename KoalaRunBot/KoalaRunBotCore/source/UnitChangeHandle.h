#pragma once
#include <BWAPI.h>

namespace KoalaRunBot {
  class UnitChangeHandle
  {
  public:
    UnitChangeHandle() = default;
    ~UnitChangeHandle() = default;

	  static void OnUnitCreate(BWAPI::Unit unit);
    static void OnUnitDestroy(BWAPI::Unit unit);
    static void OnUnitShow(BWAPI::Unit unit);
    static void OnUnitHide(BWAPI::Unit unit);
    static void OnUnitMorph(BWAPI::Unit unit);
    static void OnUnitComplete(BWAPI::Unit unit);
    static void OnUnitRenegade(BWAPI::Unit unit);

  };
}


