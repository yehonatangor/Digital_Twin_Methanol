#include "capex_opex.hpp"

#include <cmath>

namespace economics {

// -----------------------------------------------------------------------------
// Part 1 -- Turton CAPCOST primitives
// -----------------------------------------------------------------------------
double purchased_cost_2001usd(double K1, double K2, double K3, double A) {
  if (!(A > 0.0)) return 0.0;
  const double lA = std::log10(A);
  return std::pow(10.0, K1 + K2 * lA + K3 * lA * lA);
}

double pressure_factor_general(double C1, double C2, double C3, double P_barg) {
  // Table A.2's own convention: all-zero constants mean FP = 1
  if (C1 == 0.0 && C2 == 0.0 && C3 == 0.0) return 1.0;
  if (!(P_barg > 0.0)) return 1.0;
  const double lP = std::log10(P_barg);
  return std::pow(10.0, C1 + C2 * lP + C3 * lP * lP);
}

double bare_module_factor(double B1, double B2, double FM, double FP) {
  return B1 + B2 * FM * FP;
}

double total_module_cost(double sum_CBM_2001usd) {
  return 1.18 * sum_CBM_2001usd;   // Eq. (7.15): contingency and fee, 18 %
}

double grassroots_cost(double total_module_cost_2001usd, double sum_CBM_base_case_2001usd) {
  return total_module_cost_2001usd + 0.50 * sum_CBM_base_case_2001usd;   // Eq. (7.16)
}

double escalate_cost(double cost, double cepci_target, double cepci_base) {
  if (!(cepci_base > 0.0)) return cost;
  return cost * cepci_target / cepci_base;
}

CepciLookupResult cepci(int year) {
  // Table 7.4, printed values only. No extrapolation past what the edition
  // prints, which is why 2017 onward returns ok = false
  struct Row { int year; double value; };
  static constexpr Row table[] = {
      {1996, 382.0}, {1997, 386.5}, {1998, 389.5}, {1999, 390.6}, {2000, 394.1},
      {2001, 397.0}, {2002, 395.6}, {2003, 402.0}, {2004, 444.2}, {2005, 468.2},
      {2006, 499.6}, {2007, 525.4}, {2008, 575.4}, {2009, 521.9}, {2010, 550.8},
      {2011, 585.7}, {2012, 584.6}, {2013, 567.3}, {2014, 576.1}, {2015, 556.8},
      {2016, 541.7},
  };
  CepciLookupResult r;
  for (const Row& row : table) {
    if (row.year == year) { r.value = row.value; r.ok = true; return r; }
  }
  return r;
}

double vessel_wall_thickness_m(double P_barg, double D_m, double S_bar,
                                double E_weld, double CA_m) {
  const double denom = 2.0 * S_bar * E_weld - 1.2 * P_barg;
  if (!(denom > 0.0)) return 0.0;
  return P_barg * D_m / denom + CA_m;
}

double vessel_pressure_factor(double P_barg, double D_m, double S_bar,
                               double E_weld, double CA_m, double t_min_m) {
  if (P_barg < -0.5) return 1.25;   // vacuum branch, no thickness calculation
  const double t = vessel_wall_thickness_m(P_barg, D_m, S_bar, E_weld, CA_m);
  if (!(t_min_m > 0.0)) return 1.0;
  return (t < t_min_m) ? 1.0 : t / t_min_m;
}

double vessel_bare_module_cost_2001usd(double A_volume_m3, const PressureVesselCostConfig& cfg) {
  const double Cp0 = purchased_cost_2001usd(cfg.K1, cfg.K2, cfg.K3, A_volume_m3);
  const double FBM = bare_module_factor(cfg.B1, cfg.B2, cfg.FM, cfg.FP);
  return Cp0 * FBM;
}

// -----------------------------------------------------------------------------
// Part 2 -- annualization and OPEX
// -----------------------------------------------------------------------------
double capital_recovery_factor(double interest_rate, int n_years) {
  if (n_years <= 0) return 0.0;
  if (interest_rate == 0.0) return 1.0 / static_cast<double>(n_years);
  const double f = std::pow(1.0 + interest_rate, n_years);
  return interest_rate * f / (f - 1.0);
}

WEReplacementResult we_replacement_cost(double we_initial_investment_USD,
                                         double lifetime_years, int n_project_years) {
  WEReplacementResult r;
  if (!(we_initial_investment_USD > 0.0) || !(lifetime_years > 0.0) || n_project_years <= 0) {
    return r;
  }
  r.n_replacements = static_cast<int>(std::floor(n_project_years / lifetime_years));
  r.total_replacement_cost_USD =
      0.30 * we_initial_investment_USD * static_cast<double>(r.n_replacements);
  r.annualized_replacement_cost_USD =
      r.total_replacement_cost_USD / static_cast<double>(n_project_years);
  return r;
}

// -----------------------------------------------------------------------------
// Part 3 -- Mucci's six-tenths rule, cross-check only
// -----------------------------------------------------------------------------
double capex_scale(double capex_ref, double M_ref_t_per_y, double M_t_per_y, double b) {
  if (!(M_ref_t_per_y > 0.0) || !(M_t_per_y > 0.0)) return 0.0;
  return capex_ref * std::pow(M_t_per_y / M_ref_t_per_y, b);
}

// -----------------------------------------------------------------------------
namespace presets {

// Turton Table A.1. Verified against the printed table 2026-08:
// 9 rows, 45 constants, zero mismatches
EquipmentCostTableEntry turton_heat_exchanger_floating_head() {
  return {4.8306, -0.8509, 0.3187, 10.0, 1000.0, "Area, m^2"};
}
EquipmentCostTableEntry turton_heat_exchanger_fixed_tube() {
  return {4.3247, -0.3030, 0.1634, 10.0, 1000.0, "Area, m^2"};
}
EquipmentCostTableEntry turton_heat_exchanger_u_tube() {
  return {4.1884, -0.2503, 0.1974, 10.0, 1000.0, "Area, m^2"};
}
EquipmentCostTableEntry turton_heat_exchanger_kettle_reboiler() {
  return {4.4646, -0.5277, 0.3955, 10.0, 100.0, "Area, m^2"};
}
EquipmentCostTableEntry turton_heat_exchanger_double_pipe() {
  return {3.3444, 0.2745, -0.0472, 1.0, 10.0, "Area, m^2"};
}
EquipmentCostTableEntry turton_process_vessel_horizontal() {
  return {3.5565, 0.3776, 0.0905, 0.1, 628.0, "Volume, m^3"};
}
EquipmentCostTableEntry turton_process_vessel_vertical() {
  return {3.4974, 0.4485, 0.1074, 0.3, 520.0, "Volume, m^3"};
}
EquipmentCostTableEntry turton_tower_tray_and_packed() {
  // Table A.1 has no separate tower row; a tower is a vertical vessel
  return {3.4974, 0.4485, 0.1074, 0.3, 520.0, "Volume, m^3"};
}
EquipmentCostTableEntry turton_pump_centrifugal() {
  return {3.3892, 0.0536, 0.1538, 1.0, 300.0, "Shaft power, kW"};
}
EquipmentCostTableEntry turton_tray_sieve() {
  return {2.9949, 0.4465, 0.3961, 0.07, 12.30, "Area, m^2 (per tray)"};
}
EquipmentCostTableEntry turton_compressor_centrifugal_axial_reciprocating() {
  return {2.2897, 1.3604, -0.1027, 450.0, 3000.0, "Fluid power, kW"};
}

BareModuleConstants turton_bm_heat_exchanger_floating_head_fixed_u_kettle() { return {1.63, 1.66}; }
BareModuleConstants turton_bm_heat_exchanger_double_pipe_scraped_spiral()   { return {1.74, 1.55}; }
BareModuleConstants turton_bm_process_vessel_horizontal()                   { return {1.49, 1.52}; }
BareModuleConstants turton_bm_process_vessel_vertical_incl_towers()         { return {2.25, 1.82}; }
BareModuleConstants turton_bm_pump_centrifugal_recip_pd()                   { return {1.89, 1.35}; }

PressureFactorConstants turton_fp_heat_exchanger_5_to_140_barg() {
  return {0.03881, -0.11272, 0.08183, 5.0, 140.0};
}
PressureFactorConstants turton_fp_pump_centrifugal_10_to_100_barg() {
  return {-0.3935, 0.3957, -0.00226, 10.0, 100.0};
}

ReactantPrices lim_table1_prices_2020() {
  return ReactantPrices{};   // defaults already are Lim Table 1
}

std::vector<ElectrolyzerCapexPoint> lim_electrolyzer_capex_AEL() {
  return {
      {2020, 76.5, 1097.0, 10.0},
      {2030, 79.0,  932.0, 11.5},
      {2040, 80.0,  617.0, 14.4},
      {2050, 81.0,  521.0, 15.3},
  };
}
std::vector<ElectrolyzerCapexPoint> lim_electrolyzer_capex_PEMEL() {
  return {
      {2020, 72.5, 1188.0,  8.0},
      {2030, 75.5,  701.0, 11.3},
      {2040, 78.0,  382.0, 13.4},
      {2050, 80.0,  314.0, 13.9},
  };
}
std::vector<ElectrolyzerCapexPoint> lim_electrolyzer_capex_SOEL() {
  return {
      {2020, 94.00, 2222.0, 4.0},
      {2030, 95.50, 1273.0, 5.7},
      {2040, 96.25,  656.0, 6.8},
      {2050, 97.00,  518.0, 7.1},
  };
}

CapexAnchor mucci_capex_anchor() { return {27.8e6, 0.08e6}; }

}  // namespace presets

}  // namespace economics
