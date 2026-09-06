#include "separator_capex.hpp"

#include <cmath>

namespace economics {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kAtmBar = 1.01325;
}  // namespace

CapexLineItem cost_process_vessel(const std::string& name, double diameter_m, double length_m,
                                   double design_pressure_bar,
                                   const presets::EquipmentCostTableEntry& k_table,
                                   const presets::BareModuleConstants& bm_table, double FM,
                                   double E_weld) {
  CapexLineItem item;
  item.name = name;

  if (!(diameter_m > 0.0) || !(length_m > 0.0)) return item;

  // Table A.1 costs process vessels on volume
  const double volume_m3 = 0.25 * kPi * diameter_m * diameter_m * length_m;

  const double P_barg = design_pressure_bar - kAtmBar;   // approximate, stated in the header
  const double FP = vessel_pressure_factor(P_barg, diameter_m, kTurtonVesselS_CS_bar, E_weld,
                                            kTurtonVesselCA_m, kTurtonVesselTmin_m);

  item.Cp0_2001usd = purchased_cost_2001usd(k_table.K1, k_table.K2, k_table.K3, volume_m3);
  item.FBM         = bare_module_factor(bm_table.B1, bm_table.B2, FM, FP);
  item.CBM_2001usd = item.Cp0_2001usd * item.FBM;
  // Base case is the same Eq. (A.4) item evaluated at FM = FP = 1
  item.CBM_base_case_2001usd =
      item.Cp0_2001usd * bare_module_factor(bm_table.B1, bm_table.B2, 1.0, 1.0);
  return item;
}

CapexLineItem cost_sieve_trays(const std::string& name, double column_diameter_m, int n_trays,
                                MaterialOfConstruction moc) {
  CapexLineItem item;
  item.name = name;

  if (!(column_diameter_m > 0.0) || n_trays <= 0) return item;

  const double area_m2 = 0.25 * kPi * column_diameter_m * column_diameter_m;
  const auto k = presets::turton_tray_sieve();

  // Table A.5 route: one Fig. A.19 lookup, no FM/FP step
  double FBM = presets::kFbmSieveTrayCS;
  if (moc == MaterialOfConstruction::StainlessSteel || moc == MaterialOfConstruction::NiAlloy) {
    FBM = presets::kFbmSieveTraySS;
  }

  const double cp0_per_tray = purchased_cost_2001usd(k.K1, k.K2, k.K3, area_m2);
  item.Cp0_2001usd = cp0_per_tray * static_cast<double>(n_trays);
  item.FBM         = FBM;
  item.CBM_2001usd = item.Cp0_2001usd * FBM;
  // Carbon steel is the base case for a remaining-equipment item
  item.CBM_base_case_2001usd = item.Cp0_2001usd * presets::kFbmSieveTrayCS;
  return item;
}

}  // namespace economics
