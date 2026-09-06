#include "hybrid_plant_design_point_full.hpp"

#include <cmath>

#include "eos.hpp"
#include "reactor/transport.hpp"
#include "species.hpp"
#include "units.hpp"

namespace flowsheet {

namespace {

double mean_molar_mass_kg_per_mol(const Stream& s) {
  double M = 0.0;
  for (const auto& kv : s.molar_flow) {
    const double x = s.moleFraction(kv.first);
    if (x > 0.0) M += x * properties(kv.first).molar_mass / 1000.0;
  }
  return M;
}

double mass_density_kg_m3(const Stream& s, double T, double P_Pa, eos::RootSelect which) {
  Stream t = s;
  t.temperature = T;
  t.pressure = P_Pa;
  const auto mix = eos::mixtureParams(t, T);
  const double Z = eos::compressibilityFactor(mix, T, P_Pa, which);
  if (!(Z > 0.0)) return -1.0;
  const double v = eos::molarVolume(Z, T, P_Pa);
  const double M = mean_molar_mass_kg_per_mol(s);
  if (!(v > 0.0) || !(M > 0.0)) return -1.0;
  return M / v;
}

}  // namespace

HybridPlantDesignPointFullResult evaluate_design_point_full(
    const std::vector<double>& price_USD_per_MWh_series,
    const dispatch::StorageState& initial_storage_state,
    const HybridPlantDesignPointFullConfig& cfg) {
  HybridPlantDesignPointFullResult res;

  res.base = evaluate_design_point(price_USD_per_MWh_series, initial_storage_state, cfg.base_cfg);
  if (!res.base.ok) {
    res.message = "design point failed: " + res.base.message;
    return res;
  }
  res.unit_cost_base_USD_per_t = res.base.unit_cost_USD_per_t;

  const auto& loop = res.base.reactor_chain.recycle;

  // 1. Knockout drum, sized on the vapour leaving it
  const Stream& vap = loop.knockout.vapor;
  const Stream& liq = loop.knockout.liquid;
  const double T_drum = cfg.base_cfg.distillation_cfg.methanol_recovery_fraction > 0.0
                            ? loop.knockout.vapor.temperature
                            : 308.15;
  const double P_drum = vap.pressure > 0.0 ? vap.pressure : loop.reactor.outlet.P_Pa;

  const double rho_g = mass_density_kg_m3(vap, T_drum, P_drum, eos::RootSelect::Vapor);
  const double rho_l = mass_density_kg_m3(liq, T_drum, P_drum, eos::RootSelect::Liquid);
  const double mu_g = reactor::mixture_viscosity_Pa_s(
      reactor::ReactorState::fromStream(vap).mole_fractions(), T_drum, reactor::ViscosityConfig{});

  if (rho_g > 0.0 && rho_l > rho_g && mu_g > 0.0) {
    const double M_vap = mean_molar_mass_kg_per_mol(vap);
    const double Qg_m3_s = vap.totalMolarFlow() * M_vap / rho_g;
    res.knockout_drum_sizing = economics::size_vertical_vessel(Qg_m3_s, mu_g, rho_l, rho_g,
                                                                units::paToBar(P_drum));
  }
  if (!res.knockout_drum_sizing.ok) {
    res.message = "knockout drum sizing failed: " + res.knockout_drum_sizing.message;
    return res;
  }

  res.knockout_drum_item = economics::cost_process_vessel(
      "knockout drum", res.knockout_drum_sizing.diameter_m, res.knockout_drum_sizing.length_m,
      units::paToBar(P_drum), economics::presets::turton_process_vessel_vertical(),
      economics::presets::turton_bm_process_vessel_vertical_incl_towers(), cfg.vessel_FM);

  // 2. Distillation column
  res.column_sizing = economics::size_distillation_column(liq, res.base.reactor_chain.distillation,
                                                           cfg.column_cfg);
  if (!res.column_sizing.ok) {
    res.message = "column sizing failed: " + res.column_sizing.message;
    return res;
  }

  // Shell height from the tray count and spacing, plus Turton's usual
  // allowance for the sumps and disengaging space at each end
  const double tray_spacing_m = cfg.column_cfg.tray_spacing_cfg.tray_spacing_in * 0.0254;
  const double shell_height_m = cfg.n_trays * tray_spacing_m + 2.0 * res.column_sizing.diameter_m;

  res.column_shell_item = economics::cost_process_vessel(
      "distillation column shell", res.column_sizing.diameter_m, shell_height_m,
      cfg.column_cfg.column_pressure_bar, economics::presets::turton_tower_tray_and_packed(),
      economics::presets::turton_bm_process_vessel_vertical_incl_towers(), cfg.vessel_FM);

  res.column_trays_item = economics::cost_sieve_trays("sieve trays", res.column_sizing.diameter_m,
                                                       cfg.n_trays, cfg.vessel_moc);

  // 3. Re-aggregate with the three new items
  std::vector<economics::CapexLineItem> items = res.base.capex.items;
  items.push_back(res.knockout_drum_item);
  items.push_back(res.column_shell_item);
  items.push_back(res.column_trays_item);

  res.capex_full = economics::aggregate_capex(items, cfg.base_cfg.cepci_target_year);
  if (!res.capex_full.ok) {
    res.message = "full CAPEX aggregation failed: " + res.capex_full.message;
    return res;
  }

  res.plant_economics_full = economics::run_plant_economics(
      res.capex_full.total_module_cost_escalated, res.base.annual_opex,
      cfg.base_cfg.reactant_prices.interest_rate, cfg.base_cfg.project_years,
      res.base.annual_production_t);
  res.unit_cost_full_USD_per_t = res.plant_economics_full.unit_cost_USD_per_t;

  res.ok = true;
  res.message = "ok. The knockout drum, column shell and trays are costed here, so the "
                "vessel-and-column gap the base design point reports is closed.";
  return res;
}

}  // namespace flowsheet
