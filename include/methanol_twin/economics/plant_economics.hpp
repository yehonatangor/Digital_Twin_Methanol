#pragma once
// Plant-level CAPEX, OPEX and unit methanol cost from real process results
//
// Partial by design; the result message lists which categories are included
// Costed: compressor, packed-bed reactor as a shell-and-tube exchanger,
// electrolyser (Lim Table 2 $/kW), and the drum and column when the caller
// supplies geometry. Not costed: generic two-stream exchangers (no sourced U)
// and the membrane (no cost model in this library)
//
// Natural gas LHV is derived from the formation-enthalpy table so there is one
// source of thermochemical truth. See docs/21-plant-economics.md

#include <string>
#include <vector>

#include "capex_opex.hpp"
#include "reactor/ergun.hpp"

namespace economics {

// -----------------------------------------------------------------------------
// CAPEX aggregation
// -----------------------------------------------------------------------------
struct CapexLineItem {
  std::string name;
  double Cp0_2001usd = 0.0;
  double FBM = 0.0;
  double CBM_2001usd = 0.0;              // Cp0 * FBM
  double CBM_base_case_2001usd = 0.0;    // at FM=FP=1 (Eq. A.4 items) or CS FBM (remaining-equipment items)

  // True for a cost that is already an all-in installed figure in its own
  // currency year, Lim's Table 2 electrolyzer $/kW being the case. Such an item
  // bypasses Turton Eq. (7.15)'s 1.18 markup and CEPCI escalation, both already
  // contained in the quote; applying them anyway inflates it by 1.61
  // aggregate_capex() accumulates these separately and reports both parts
  bool all_in_installed = false;
};

struct PlantCapexResult {
  std::vector<CapexLineItem> items;

  // Turton-pipeline items only (all_in_installed == false)
  double sum_CBM_2001usd = 0.0;
  double sum_CBM_base_case_2001usd = 0.0;
  double total_module_cost_2001usd = 0.0;      // Sec. 7.3.7, Eq. (7.15)
  double grassroots_cost_2001usd = 0.0;        // Sec. 7.3.7, Eq. (7.16)

  // All-in installed items, carried at face value (no 1.18x, no CEPCI)
  double sum_all_in_installed_usd = 0.0;

  // Escalated Turton total PLUS the all-in items. This is the number a caller
  // should hand to run_plant_economics(); the two components above are exposed
  // so the split can be inspected
  double total_module_cost_escalated = 0.0;
  int    cepci_target_year = 0;
  bool ok = false;
  std::string message;   // states which equipment categories are/aren't included -- see header
};

PlantCapexResult aggregate_capex(const std::vector<CapexLineItem>& items, int cepci_target_year);

// ---- Equipment-specific sizing+costing helpers -----------------------------

enum class MaterialOfConstruction { CarbonSteel, StainlessSteel, NiAlloy };

// Turton Table A.5 path: Cp0 from the fluid-power K1/K2/K3 correlation, FBM
// digitized from Fig. A.19. Table A.1's valid range is 450 to 3000 kW; outside
// it the returned item's name carries a flag rather than throwing
CapexLineItem cost_compressor(const std::string& name, double power_kW,
                               MaterialOfConstruction moc);

// Packed-bed reactor costed as a shell-and-tube exchanger, Turton Appendix
// B.5 convention. Area = n_tubes * pi * tube_inner_diameter_m * bed_length_m
// preset selects the heat-exchanger constants; fixed tube sheet is usual
CapexLineItem cost_reactor_as_shell_and_tube(const std::string& name,
                                              const reactor::BedGeometry& bed,
                                              double FM, double FP);

// Electrolyzer. Lim Table 2 is all-in installed, so FBM = 1 and
// CBM = Cp0 = power_kW * usd_per_kW. Not a Turton bare-module calculation
CapexLineItem cost_electrolyzer(const std::string& name, double power_kW,
                                 double usd_per_kW);

// OPEX. Every reactant and utility flow is taken from an actual process-chain
// result, not a placeholder
struct ReactantFlowsKgPerS {
  double natural_gas_kg_s      = 0.0;  // converted to GJ/s via natural_gas_lhv_MJ_per_kg()
  double co2_kg_s               = 0.0;
  double water_kg_s             = 0.0;
  double hydrogen_makeup_kg_s   = 0.0;  // purchased H2 top-up, if any (0 if WE covers all H2)
};

// LHV of methane [MJ/kg] from thermo::deltaH(), the same route as
// front_end::pem_lhv_h2_MJ_per_kg, so the formation-enthalpy table stays the
// only source. CH4 + 2 O2 -> CO2 + 2 H2O(g), LHV = -deltaH(298.15 K) / M_CH4
double natural_gas_lhv_MJ_per_kg();

struct PlantOpexResult {
  double reactant_cost_USD_per_y      = 0.0;
  double electricity_cost_USD_per_y   = 0.0;
  double maintenance_cost_USD_per_y   = 0.0;
  double we_replacement_cost_USD_per_y = 0.0;
  double total_opex_USD_per_y         = 0.0;
  bool ok = false;
  std::string message;
};

// stream_factor: Lim Table 1, 0.9 for the grid-electricity case
// trm_and_meoh_capex_USD: the base for maintenance, Lim Sec. 2.2.1's 20 % of
// TRM plus methanol-synthesis CAPEX, excluding the electrolyzer item
PlantOpexResult compute_opex(const ReactantFlowsKgPerS& flows, double electricity_MW,
                              const ReactantPrices& prices, double stream_factor,
                              double trm_and_meoh_capex_USD,
                              const WEReplacementResult& we_replacement);

// -----------------------------------------------------------------------------
// Top-level: CRF-annualized CAPEX + OPEX -> unit production cost (Lim Eq. 6)
// -----------------------------------------------------------------------------
struct PlantEconomicsResult {
  double crf = 0.0;
  double annualized_capex_USD_per_y = 0.0;
  double total_annual_cost_USD_per_y = 0.0;
  double annual_production_t = 0.0;
  double unit_cost_USD_per_t = 0.0;
};

PlantEconomicsResult run_plant_economics(double capex_USD, const PlantOpexResult& opex,
                                          double interest_rate, int n_years,
                                          double annual_production_t);

}  // namespace economics
