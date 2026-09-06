#pragma once
// CO2 + H2 to methanol as one connected reactor chain, at industrial scale
// Van-Dal & Bouallou (2013) J. Cleaner Production 57, 38-45
//
// Sourced from Secs. 2.3.1-2.3.2 and Table 5: CO2 fed at 1 bar and H2 at
// 30 bar, both at 25 C; heated to 210 C into the reactor at 78 bar; cooled to
// 35 C where methanol and water condense; 88.0 t/h CO2
//
// Single pass, no recycle. Van-Dal's flowsheet recycles 5 parts unreacted gas
// per part fresh feed, and Table 5's 93 % conversion is a plant figure
// achieved through that loop. Reconstructing it needs per-stream recycle
// compositions the paper does not tabulate, so this module validates against
// the stated 33 % per-pass figure instead
//
// Compression is not modelled. Feeds are taken as delivered at 78 bar; a
// caller wanting real duty can run the compressor separately
//
// Bed geometry combines Van-Dal's Table 2 catalyst properties with Shi et al
// (2020) Sec. 3.1 tube geometry, since Van-Dal publishes no tubes. Van-Dal's
// catalyst data is kept because the kinetics are calibrated for it

#include <string>
#include "stream.hpp"
#include "reactor/reactor_core.hpp"
#include "front_end/knockout_drum.hpp"

namespace flowsheet {

// The plant reactor is a jacketed multi-tubular boiling-water reactor, not an
// adiabatic bed. Measured at the canonical design point: adiabatic peaks at
// 284.2 C, outside Fichtl's 523 to 553 K band and deactivating the catalyst
// 1.94 times faster; cooled peaks at 250.2 C and declines to 247.5 C
//
// The default is set here rather than in reactor_core.hpp because Van-Dal's
// validation case is a genuinely adiabatic laboratory bed and changing that
// default would break the 33 % per-pass gate
reactor::ReactorConfig plant_reactor_defaults();

struct Co2H2PlantConfig {
  // Cooled BWR jacket: coolant 245 C (Mucci App. A.4), U = 980 W/(m^2 K)
  // DERIVED from Shi's own energy balance, U_sourced = false
  reactor::ReactorConfig reactor_cfg = plant_reactor_defaults();   // n_tubes <= 1 selects the composite bed
  double reactor_inlet_T_K = 483.15;     // 210 C, Van-Dal Sec. 2.3.1
  double reactor_inlet_P_bar = 78.0;     // Van-Dal Sec. 2.3.1
  double knockout_T_K = 308.15;          // 35 C, Van-Dal Sec. 2.3.1
  double knockout_P_bar = 78.0;          // used only if the flag below is false

  // The drum runs at the reactor's own computed outlet pressure. Set false to
  // recover the previous behaviour, where the Ergun drop was computed and then
  // discarded at the drum
  bool knockout_at_reactor_outlet_pressure = true;
};

struct Co2H2PlantResult {
  Stream fresh_feed;                        // mixed, pre-heat, plant scale
  Stream reactor_inlet;                     // heated to reactor_inlet_T_K
  reactor::ReactorResult reactor;           // per tube
  Stream reactor_outlet_plant_scale;        // scaled back up by n_tubes
  front_end::KnockoutDrumResult knockout;

  double co2_conversion_per_pass = 0.0;     // scale-invariant
  double meoh_production_kg_s = 0.0;        // plant scale
  int    n_tubes_used = 0;

  bool ok = false;
  std::string message;
};

// 1 bar, 25 C, per Van-Dal
Stream co2_fresh_feed(double m_dot_CO2_kg_s);

// 30 bar, 25 C, per Van-Dal
Stream h2_fresh_feed(double m_dot_H2_kg_s);

// Mix, heat, react, knock out. Feed rates are plant scale; the reactor call
// is per tube and the result is scaled back up
Co2H2PlantResult run_co2_h2_plant(double m_dot_CO2_kg_s, double m_dot_H2_kg_s,
                                   const Co2H2PlantConfig& cfg = Co2H2PlantConfig{});

namespace presets {

// Van-Dal Table 5, 88.0 t/h
double van_dal_industrial_co2_kg_s();

// Table 5 does not tabulate H2 separately, so this sizes it from
// CO2 + 3 H2 -> CH3OH + H2O
double stoichiometric_h2_feed_kg_s(double m_dot_CO2_kg_s);

// Van-Dal Table 2, total industrial catalyst charge
inline constexpr double kVanDalCatalystChargeKg = 44500.0;

// Van-Dal Table 2 catalyst properties and charge, with Shi et al. (2020)
// Sec. 3.1 tube diameter and length. The tube count is derived from the charge
// rather than taken from Shi; see the .cpp for why adopting Shi's 2700 tubes
// undersized the bed by a factor of 2.3
reactor::BedGeometry van_dal_catalyst_shi_tubes();

}  // namespace presets

}  // namespace flowsheet
