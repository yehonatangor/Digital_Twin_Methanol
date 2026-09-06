#include "plant_economics.hpp"

#include <cmath>

#include "species.hpp"
#include "thermo.hpp"

namespace economics {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kSecondsPerYear = 365.0 * 24.0 * 3600.0;
}  // namespace

// -----------------------------------------------------------------------------
// CAPEX aggregation
// -----------------------------------------------------------------------------
PlantCapexResult aggregate_capex(const std::vector<CapexLineItem>& items, int cepci_target_year) {
  PlantCapexResult res;
  res.items = items;
  res.cepci_target_year = cepci_target_year;

  for (const CapexLineItem& it : items) {
    if (it.all_in_installed) {
      // Already an installed cost in its own currency year: no 1.18x, no CEPCI
      res.sum_all_in_installed_usd += it.CBM_2001usd;
    } else {
      res.sum_CBM_2001usd += it.CBM_2001usd;
      res.sum_CBM_base_case_2001usd += it.CBM_base_case_2001usd;
    }
  }

  res.total_module_cost_2001usd = total_module_cost(res.sum_CBM_2001usd);
  res.grassroots_cost_2001usd =
      grassroots_cost(res.total_module_cost_2001usd, res.sum_CBM_base_case_2001usd);

  const CepciLookupResult idx = cepci(cepci_target_year);
  if (!idx.ok) {
    res.message = "CEPCI index not available for the requested year; Table 7.4 covers 1996-2016";
    return res;
  }

  res.total_module_cost_escalated =
      escalate_cost(res.total_module_cost_2001usd, idx.value) + res.sum_all_in_installed_usd;

  res.ok = true;
  res.message =
      "Scope: reactor (costed as shell-and-tube per Turton Appendix B.5), compressors and "
      "vessels where the caller supplies them, plus all-in installed items carried at face "
      "value. Piping, instrumentation, civil works and site development are not itemised "
      "beyond Eq. (7.15)/(7.16)'s own factors.";
  return res;
}

// -----------------------------------------------------------------------------
// Equipment-specific sizing and costing
// -----------------------------------------------------------------------------
CapexLineItem cost_compressor(const std::string& name, double power_kW,
                               MaterialOfConstruction moc) {
  CapexLineItem item;
  item.name = name;
  if (!(power_kW > 0.0)) return item;

  const auto k = presets::turton_compressor_centrifugal_axial_reciprocating();
  if (power_kW < k.min_size || power_kW > k.max_size) {
    item.name += " (outside Table A.1 450-3000 kW range)";
  }

  double FBM = presets::kFbmCompressorCS;
  if (moc == MaterialOfConstruction::StainlessSteel)   FBM = presets::kFbmCompressorSS;
  else if (moc == MaterialOfConstruction::NiAlloy)     FBM = presets::kFbmCompressorNiAlloy;

  item.Cp0_2001usd = purchased_cost_2001usd(k.K1, k.K2, k.K3, power_kW);
  item.FBM         = FBM;
  item.CBM_2001usd = item.Cp0_2001usd * FBM;
  item.CBM_base_case_2001usd = item.Cp0_2001usd * presets::kFbmCompressorCS;
  return item;
}

CapexLineItem cost_reactor_as_shell_and_tube(const std::string& name,
                                              const reactor::BedGeometry& bed,
                                              double FM, double FP) {
  CapexLineItem item;
  item.name = name;
  if (bed.n_tubes <= 0 || !(bed.tube_inner_diameter_m > 0.0) || !(bed.bed_length_m > 0.0)) {
    return item;
  }

  // Pure geometry: total internal tube surface
  const double area_m2 =
      static_cast<double>(bed.n_tubes) * kPi * bed.tube_inner_diameter_m * bed.bed_length_m;

  const auto k  = presets::turton_heat_exchanger_fixed_tube();
  const auto bm = presets::turton_bm_heat_exchanger_floating_head_fixed_u_kettle();

  if (area_m2 < k.min_size || area_m2 > k.max_size) {
    item.name += " (outside Table A.1 10-1000 m^2 range)";
  }

  item.Cp0_2001usd = purchased_cost_2001usd(k.K1, k.K2, k.K3, area_m2);
  item.FBM         = bare_module_factor(bm.B1, bm.B2, FM, FP);
  item.CBM_2001usd = item.Cp0_2001usd * item.FBM;
  item.CBM_base_case_2001usd = item.Cp0_2001usd * bare_module_factor(bm.B1, bm.B2, 1.0, 1.0);
  return item;
}

CapexLineItem cost_electrolyzer(const std::string& name, double power_kW, double usd_per_kW) {
  CapexLineItem item;
  item.name = name;
  if (!(power_kW > 0.0) || !(usd_per_kW > 0.0)) return item;

  // Lim Table 2 is an all-in installed cost; FBM = 1 by construction and the
  // item bypasses Turton's markup pipeline entirely
  item.Cp0_2001usd = power_kW * usd_per_kW;
  item.FBM         = 1.0;
  item.CBM_2001usd = item.Cp0_2001usd;
  item.CBM_base_case_2001usd = item.Cp0_2001usd;
  item.all_in_installed = true;
  return item;
}

// -----------------------------------------------------------------------------
// OPEX
// -----------------------------------------------------------------------------
double natural_gas_lhv_MJ_per_kg() {
  // CH4 + 2 O2 -> CO2 + 2 H2O(g), same derivation route as hydrogen's LHV
  const thermo::Reaction combustion = {
      {Species::CH4, -1.0}, {Species::O2, -2.0}, {Species::CO2, 1.0}, {Species::H2O, 2.0}};
  const double dH_J_per_mol = thermo::deltaH(combustion, 298.15);
  const double M_g_per_mol  = properties(Species::CH4).molar_mass;
  return -dH_J_per_mol / M_g_per_mol / 1000.0;
}

PlantOpexResult compute_opex(const ReactantFlowsKgPerS& flows, double electricity_MW,
                              const ReactantPrices& prices, double stream_factor,
                              double trm_and_meoh_capex_USD,
                              const WEReplacementResult& we_replacement) {
  PlantOpexResult res;

  if (!(stream_factor > 0.0) || stream_factor > 1.0) {
    res.message = "stream_factor must be in (0, 1]";
    return res;
  }

  const double operating_s_per_y = kSecondsPerYear * stream_factor;

  // Reactants. Natural gas is priced per GJ of LHV, the rest per tonne or kg
  const double lhv_MJ_per_kg = natural_gas_lhv_MJ_per_kg();
  const double gas_GJ_per_y =
      flows.natural_gas_kg_s * lhv_MJ_per_kg * 1.0e-3 * operating_s_per_y;

  const double co2_t_per_y   = flows.co2_kg_s   * operating_s_per_y / 1000.0;
  const double water_t_per_y = flows.water_kg_s * operating_s_per_y / 1000.0;
  const double h2_kg_per_y   = flows.hydrogen_makeup_kg_s * operating_s_per_y;

  res.reactant_cost_USD_per_y = gas_GJ_per_y   * prices.natural_gas_USD_per_GJ
                              + co2_t_per_y    * prices.co2_USD_per_t
                              + water_t_per_y  * prices.water_USD_per_t
                              + h2_kg_per_y    * prices.hydrogen_USD_per_kg;

  // Electricity, flat-price route. A caller with a dispatch run should
  // overwrite this with that run's own price-weighted cost
  res.electricity_cost_USD_per_y =
      electricity_MW * 1000.0 * (operating_s_per_y / 3600.0) * prices.electricity_USD_per_kWh;

  res.maintenance_cost_USD_per_y = annual_maintenance_cost(trm_and_meoh_capex_USD);
  res.we_replacement_cost_USD_per_y = we_replacement.annualized_replacement_cost_USD;

  res.total_opex_USD_per_y = res.reactant_cost_USD_per_y + res.electricity_cost_USD_per_y
                           + res.maintenance_cost_USD_per_y + res.we_replacement_cost_USD_per_y;

  res.ok = true;
  res.message = "ok";
  return res;
}

// -----------------------------------------------------------------------------
PlantEconomicsResult run_plant_economics(double capex_USD, const PlantOpexResult& opex,
                                          double interest_rate, int n_years,
                                          double annual_production_t) {
  PlantEconomicsResult res;
  res.crf = capital_recovery_factor(interest_rate, n_years);
  res.annualized_capex_USD_per_y = capex_USD * res.crf;
  res.total_annual_cost_USD_per_y = res.annualized_capex_USD_per_y + opex.total_opex_USD_per_y;
  res.annual_production_t = annual_production_t;
  if (annual_production_t > 0.0) {
    res.unit_cost_USD_per_t =
        unit_production_cost_USD_per_t(res.total_annual_cost_USD_per_y, annual_production_t);
  }
  return res;
}

}  // namespace economics
