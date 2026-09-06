#pragma once
// Turton CAPCOST equipment costing, and Lim annualisation

// Turton et al. 5th ed. (2018) Ch. 7 and App. A. Table A.1 verified against
// the printed table, 9 rows, 45 constants
//   (A.1)  log10(Cp0) = K1 + K2 log10(A) + K3 log10(A)^2   2001 USD, CEPCI 397
//   (A.3)  log10(FP)  = C1 + C2 log10(P) + C3 log10(P)^2   P in barg
//   (A.4)  F_BM = B1 + B2 * FM * FP     exchangers, vessels, pumps
//   (7.9)  t = P D / (2 S E - 1.2 P) + CA        vessels take this, not (A.3)
//   (7.10) F_P = 1 below t_min, t/t_min above, 1.25 under vacuum
//   (7.15) C_TM = 1.18 * sum(C_BM)
//   (7.16) C_GR = C_TM + 0.50 * sum(C_BM at FM = FP = 1)
//   CEPCI: cost(year) = cost(2001) * CEPCI(year) / 397, Table 7.4, 1996-2016
// Table A.5 equipment (compressors, trays) takes F_BM directly from Fig. A.19
// with no separate FM/FP step. Reproduces Example 7.14 end to end

// FM has NO default: Fig. A.18 is a graph, so a value read from it is a
// reading. Only the values Turton's worked examples print are exposed
// Compressor F_BM values are DIGITIZED from Fig. A.19 at 400 dpi
// Weld efficiency E has no default; (7.10) prints 0.9, Example 7.14 uses 0.750

// Lim et al. (2022): CRF = i(1+i)^n / ((1+i)^n - 1), Table 1 prices, 20 % of
// TRM and synthesis CAPEX for maintenance excluding the electrolyser, 30 % of
// initial WE investment per stack replacement

// Mucci's six-tenths anchor is a CROSS-CHECK only and is never combined with
// Turton's markups; F_BM already covers what Biegler's 1.85x does
// See docs/20-equipment-costing.md and docs/21-plant-economics.md

#include <string>
#include <vector>

namespace economics {

// Part 1: Turton CAPCOST primitives

// Eq. (A.1). K1/K2/K3 and A per Table A.1. The valid size range is not
// enforced; see the presets for units and limits
double purchased_cost_2001usd(double K1, double K2, double K3, double A);

// Eq. (A.3). C1=C2=C3=0 gives FP=1 outside a stated range, per Table A.2
double pressure_factor_general(double C1, double C2, double C3, double P_barg);

// Eq. (A.4). Heat exchangers, process vessels and pumps only
double bare_module_factor(double B1, double B2, double FM, double FP);

inline double bare_module_cost(double Cp0_2001usd, double F_BM) {
  return Cp0_2001usd * F_BM;
}

// Table A.5 equipment: F_BM is a single Fig. A.19 lookup, no FM/FP step
// Same arithmetic as above, named separately so the conventions stay clear
inline double bare_module_cost_remaining_equipment(double Cp0_2001usd, double F_BM) {
  return Cp0_2001usd * F_BM;
}

// Eq. (7.15)/(7.16). sum_CBM is the actual bare module total;
// sum_CBM_base_case is the same equipment list evaluated at FM = FP = 1
double total_module_cost(double sum_CBM_2001usd); // = 1.18 * sum_CBM
double grassroots_cost(double total_module_cost_2001usd,
                        double sum_CBM_base_case_2001usd); // = TMC + 0.50*sum_CBM_base

// cepci_base defaults to Turton's K-table basis of 397
double escalate_cost(double cost, double cepci_target, double cepci_base = 397.0);

// Table 7.4, CEPCI 1996-2016. ok = false outside that range; no extrapolation
struct CepciLookupResult {
  double value = 0.0;
  bool ok = false;
};
CepciLookupResult cepci(int year);

// Eq. (7.9), metres. E_weld is 0.6-1.0 with no default
double vessel_wall_thickness_m(double P_barg, double D_m, double S_bar,
                                double E_weld, double CA_m);

// Eq. (7.10). 1.25 under vacuum, else 1.0 below t_min_m or t/t_min_m above
double vessel_pressure_factor(double P_barg, double D_m, double S_bar,
                               double E_weld, double CA_m, double t_min_m);

// Constants Eq. (7.10) and Fig. 7.6 assume throughout
inline constexpr double kTurtonVesselS_CS_bar = 944.0; // carbon steel max. allowable stress
inline constexpr double kTurtonVesselCA_m = 0.00315; // corrosion allowance, 1/8 in
inline constexpr double kTurtonVesselTmin_m = 0.0063; // minimum wall thickness, 1/4 in
inline constexpr double kTurtonVesselE_AsPrinted = 0.9; // Eq. (7.10) as literally printed

// Derived: both of Example 7.14's vessels back-solve to 0.750. What that
// example used, not a general recommendation
inline constexpr double kTurtonVesselE_Example714Derived = 0.75;

// FP has no default: vessel_pressure_factor() computes one, but the caller
// chooses E and passes the result in
struct PressureVesselCostConfig {
  double K1, K2, K3; // Table A.1 (Horizontal or Vertical process vessels)
  double B1, B2; // Table A.4 (Horizontal or Vertical, resp.)
  double FM; // no default
  double FP; // from vessel_pressure_factor()
  bool   FP_sourced = false;
};

double vessel_bare_module_cost_2001usd(double A_volume_m3, const PressureVesselCostConfig& cfg);

// Part 2: annualization and OPEX (Lim et al. 2022)

// Eq. (5)
double capital_recovery_factor(double interest_rate, int n_years);

struct ReactantPrices {
  // Lim Table 1, 2020-basis USD. All seven verified against the printed table
  double natural_gas_USD_per_GJ = 2.99;
  double co2_USD_per_t = 86.4;
  // Table 1 prints a unit only where it changes, so water's blank cell
  // inherits USD/t from the carbon dioxide row above it
  double water_USD_per_t = 0.15;
  bool   water_price_unit_sourced = true;
  double hydrogen_USD_per_kg = 3.2;
  double electricity_USD_per_kWh = 0.06;
  double interest_rate = 0.045;
  double stream_factor = 0.9;  // grid-electricity case
};

struct ElectrolyzerCapexPoint {
  int year;
  double system_efficiency_pct;
  double investment_cost_USD_per_kW;
  double lifetime_years;
};

// Sec. 2.2.1: 20 % of TRM and methanol-synthesis CAPEX, excluding the
// electrolyser, which is handled by we_replacement_cost() below
inline double annual_maintenance_cost(double trm_and_meoh_capex_USD) {
  return 0.20 * trm_and_meoh_capex_USD;
}

// 30 % of initial WE investment per replacement, with
// n_replacements = floor(n_project_years / lifetime_years). Full cycles
// only; the source does not specify prorating a partial final cycle
struct WEReplacementResult {
  int    n_replacements = 0;
  double total_replacement_cost_USD = 0.0;
  double annualized_replacement_cost_USD = 0.0; // simple average, not discounted
};
WEReplacementResult we_replacement_cost(double we_initial_investment_USD,
                                         double lifetime_years,
                                         int n_project_years);

// Eq. (6)
inline double unit_production_cost_USD_per_t(double total_annual_cost_USD,
                                              double annual_production_t) {
  return total_annual_cost_USD / annual_production_t;
}

// Part 3: Mucci's six-tenths rule, CAPEX = CAPEX_ref * (M/M_ref)^0.6
// Cross-check only against their 0.08 Mt/y, 27.8 M-EUR anchor. Different
// method, currency basis and equipment scope, so not a reproduction
double capex_scale(double capex_ref, double M_ref_t_per_y, double M_t_per_y, double b = 0.6);

struct CapexAnchor {
  double capex_ref_EUR;
  double M_ref_t_per_y;
};

namespace presets {

// Turton Tables A.1, A.2 and A.4
struct EquipmentCostTableEntry {
  double K1, K2, K3;
  double min_size, max_size; // Table A.1 min/max size, same units as A
  const char* capacity_unit;
};

EquipmentCostTableEntry turton_heat_exchanger_floating_head(); // Area, m^2, 10-1000
EquipmentCostTableEntry turton_heat_exchanger_fixed_tube(); // Area, m^2, 10-1000
EquipmentCostTableEntry turton_heat_exchanger_u_tube(); // Area, m^2, 10-1000
EquipmentCostTableEntry turton_heat_exchanger_kettle_reboiler(); // Area, m^2, 10-100
EquipmentCostTableEntry turton_heat_exchanger_double_pipe(); // Area, m^2, 1-10
EquipmentCostTableEntry turton_process_vessel_horizontal(); // Volume, m^3, 0.1-628
EquipmentCostTableEntry turton_process_vessel_vertical(); // Volume, m^3, 0.3-520
EquipmentCostTableEntry turton_tower_tray_and_packed(); // Volume, m^3, 0.3-520
EquipmentCostTableEntry turton_pump_centrifugal(); // Shaft power, kW, 1-300; unverified, unused
EquipmentCostTableEntry turton_tray_sieve(); // Area, m^2 per tray, 0.07-12.30
EquipmentCostTableEntry turton_compressor_centrifugal_axial_reciprocating(); // Fluid power, kW, 450-3000

struct BareModuleConstants { double B1, B2; };
BareModuleConstants turton_bm_heat_exchanger_floating_head_fixed_u_kettle(); // B1=1.63, B2=1.66
BareModuleConstants turton_bm_heat_exchanger_double_pipe_scraped_spiral(); // B1=1.74, B2=1.55
BareModuleConstants turton_bm_process_vessel_horizontal(); // B1=1.49, B2=1.52
BareModuleConstants turton_bm_process_vessel_vertical_incl_towers(); // B1=2.25, B2=1.82
BareModuleConstants turton_bm_pump_centrifugal_recip_pd(); // B1=1.89, B2=1.35

// Table A.2, Eq. (A.3). Heat exchangers over 5-140 barg; below 5 barg the
// constants are zero and FP = 1
struct PressureFactorConstants { double C1, C2, C3; double p_min_barg, p_max_barg; };
PressureFactorConstants turton_fp_heat_exchanger_5_to_140_barg();
PressureFactorConstants turton_fp_pump_centrifugal_10_to_100_barg();

// The only Fig. A.18 FM values Turton's worked examples print. Not a general
// table; use only for the combination named
inline constexpr double kFmShellTubeCS_CS = 1.0; // Example 7.10
inline constexpr double kFmShellTubeCS_SS = 1.81; // CS shell / SS tube
inline constexpr double kFmShellTubeSS_SS = 2.73; // Example 7.12
inline constexpr double kFmVerticalVesselSS = 3.11; // Example 7.13

// Fig. A.19 bare-module factors for Table A.5 equipment
inline constexpr double kFbmCompressorCS = 2.75; // DIGITIZED, Fig. A.19
inline constexpr double kFbmCompressorSS = 5.7; // DIGITIZED, Fig. A.19
inline constexpr double kFbmCompressorNiAlloy = 11.5; // DIGITIZED, Fig. A.19
inline constexpr double kFbmSieveTraySS = 1.83; // printed, Example 7.14
inline constexpr double kFbmSieveTrayCS = 1.0185; // derived, Example 7.14

// Example 7.14's printed FP values
inline constexpr double kTurtonExample714TowerFP = 1.681; // T-101, 5 barg
inline constexpr double kTurtonExample714HorizontalVesselFP = 1.513; // V-101, 5 barg

// Lim et al. (2022)
ReactantPrices lim_table1_prices_2020();

std::vector<ElectrolyzerCapexPoint> lim_electrolyzer_capex_AEL();
std::vector<ElectrolyzerCapexPoint> lim_electrolyzer_capex_PEMEL();
std::vector<ElectrolyzerCapexPoint> lim_electrolyzer_capex_SOEL();

// Mucci (2023) cross-check anchor
CapexAnchor mucci_capex_anchor(); // {27.8e6 EUR, 0.08e6 t/y}, Appendix A.5

}

}
