#pragma once
// Costed line items for vessels and tray stacks
//
// Composes economics/vessel_sizing.hpp and column_sizing.hpp geometry with
// capex_opex.hpp's Turton primitives. Vessels take the ASME thickness route
// (Eq. 7.9/7.10); trays take the Table A.5 single-lookup F_BM
// Weld efficiency defaults to Example 7.14's back-solved 0.750
// See docs/20-equipment-costing.md

#include <string>

#include "economics/capex_opex.hpp"
#include "economics/plant_economics.hpp"

namespace economics {

// Generic process-vessel costing via Turton's Eq. (A.4) path (Sec. 7.3.3 /
// Table A.4) -- the SAME function shape works for a knockout drum (pass
// presets::turton_process_vessel_vertical() + presets::
// turton_bm_process_vessel_vertical_incl_towers()) and a distillation
// column SHELL (pass presets::turton_tower_tray_and_packed() +
// presets::turton_bm_process_vessel_vertical_incl_towers() -- same B1/B2
// pair, "Vertical, incl. towers", per Table A.4's own row label)
//
// design_pressure_bar is treated as approximately gauge (barg = bar -
// 1.01325) -- a simplification stated here, not hidden; for the pressures
// this project actually costs (a knockout drum at tens of bar, a column
// near atmospheric), the ~1 bar atmospheric offset is small relative to the
// drum case and directly correct-order for the column case. E_weld
// defaults to capex_opex.hpp's own kTurtonVesselE_Example714Derived (0.75)
// -- the one weld-efficiency value this project has ANY grounding for
// (back-solved from Turton's own Example 7.14), reused here as a
// documented, caller-overridable choice, not a new invented default (see
// capex_opex.hpp's own header: E "genuinely varies by weld type and
// inspection level," no default is claimed to be universally correct)
CapexLineItem cost_process_vessel(const std::string& name, double diameter_m, double length_m,
                                    double design_pressure_bar,
                                    const presets::EquipmentCostTableEntry& k_table,
                                    const presets::BareModuleConstants& bm_table, double FM = 1.0,
                                    double E_weld = kTurtonVesselE_Example714Derived);

// Sieve trays: Table A.5 path, per-tray cost on the column cross-section
// times n_trays. Outside Table A.1's 0.07-12.30 m2 range the item is still
// returned, matching that table's own extrapolate-with-caution stance
CapexLineItem cost_sieve_trays(const std::string& name, double column_diameter_m, int n_trays,
                                 MaterialOfConstruction moc);

}  // namespace economics
