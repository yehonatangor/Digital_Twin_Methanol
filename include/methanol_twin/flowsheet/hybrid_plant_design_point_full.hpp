#pragma once
// The design point with the vessel and column capital costs actually in the
// number, rather than named as a gap
//
// Composition. evaluate_design_point() is called unchanged; this file sizes
// the knockout drum and the distillation column from the converged streams,
// costs them through the same Turton path everything else uses, and reports a
// unit cost that includes them alongside the base one that does not, so the
// difference the added scope makes is visible rather than buried

#include <string>
#include <vector>

#include "economics/column_sizing.hpp"
#include "economics/separator_capex.hpp"
#include "economics/vessel_sizing.hpp"
#include "flowsheet/hybrid_plant_design_point.hpp"

namespace flowsheet {

struct HybridPlantDesignPointFullConfig {
  HybridPlantDesignPointConfig base_cfg;

  economics::DistillationColumnSizingConfig column_cfg;
  economics::MaterialOfConstruction vessel_moc = economics::MaterialOfConstruction::CarbonSteel;
  double vessel_FM = 1.0;

  // Van-Dal Sec. 2.3.3: 44 rectifying plus 13 stripping stages
  int n_trays = 57;
};

struct HybridPlantDesignPointFullResult {
  HybridPlantDesignPointResult base;   // unchanged

  economics::VesselSizingResult knockout_drum_sizing;
  economics::DistillationColumnSizingResult column_sizing;

  economics::CapexLineItem knockout_drum_item;
  economics::CapexLineItem column_shell_item;
  economics::CapexLineItem column_trays_item;

  economics::PlantCapexResult capex_full;             // base items plus the three above
  economics::PlantEconomicsResult plant_economics_full;

  double unit_cost_base_USD_per_t = 0.0;   // without the vessel and column
  double unit_cost_full_USD_per_t = 0.0;   // with them

  bool ok = false;
  std::string message;
};

HybridPlantDesignPointFullResult evaluate_design_point_full(
    const std::vector<double>& price_USD_per_MWh_series,
    const dispatch::StorageState& initial_storage_state,
    const HybridPlantDesignPointFullConfig& cfg = HybridPlantDesignPointFullConfig{});

}  // namespace flowsheet
