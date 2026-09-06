#include "co2_h2_plant.hpp"

#include <cmath>

#include "front_end/feed_blend.hpp"
#include "front_end/heat_exchanger.hpp"
#include "species.hpp"
#include "units.hpp"

namespace flowsheet {

namespace {
constexpr double kRefFeedTemperatureK = 298.15;   // 25 C, Van-Dal Sec. 2.3.1
}  // namespace

reactor::ReactorConfig plant_reactor_defaults() {
  reactor::ReactorConfig c;
  c.thermal = reactor::ThermalMode::Cooled;   // jacketed BWR, see the header
  return c;                                    // cooling{} carries 245 C and U = 980
}

Stream co2_fresh_feed(double m_dot_CO2_kg_s) {
  Stream s;
  s.temperature = kRefFeedTemperatureK;
  s.pressure    = units::barToPa(1.0);
  if (m_dot_CO2_kg_s > 0.0) {
    const double M_g_per_mol = properties(Species::CO2).molar_mass;
    s.molar_flow[Species::CO2] = m_dot_CO2_kg_s * 1000.0 / M_g_per_mol;
  }
  return s;
}

Stream h2_fresh_feed(double m_dot_H2_kg_s) {
  Stream s;
  s.temperature = kRefFeedTemperatureK;
  s.pressure    = units::barToPa(30.0);
  if (m_dot_H2_kg_s > 0.0) {
    const double M_g_per_mol = properties(Species::H2).molar_mass;
    s.molar_flow[Species::H2] = m_dot_H2_kg_s * 1000.0 / M_g_per_mol;
  }
  return s;
}

namespace presets {

double van_dal_industrial_co2_kg_s() {
  return 88.0 * 1000.0 / 3600.0;   // 88.0 t/h
}

double stoichiometric_h2_feed_kg_s(double m_dot_CO2_kg_s) {
  if (!(m_dot_CO2_kg_s > 0.0)) return 0.0;
  const double n_CO2 = m_dot_CO2_kg_s * 1000.0 / properties(Species::CO2).molar_mass;
  const double n_H2  = 3.0 * n_CO2;   // CO2 + 3 H2 -> CH3OH + H2O
  return n_H2 * properties(Species::H2).molar_mass / 1000.0;
}

reactor::BedGeometry van_dal_catalyst_shi_tubes() {
  const reactor::BedGeometry vd  = reactor::presets::van_dal_industrial();
  const reactor::BedGeometry shi = reactor::presets::shi_bwr();

  reactor::BedGeometry b = vd;   // Van-Dal's catalyst properties
  b.tube_inner_diameter_m = shi.tube_inner_diameter_m;
  b.bed_length_m          = shi.bed_length_m;

  // The tube count is derived from Van-Dal's own published charge rather than
  // adopted from Shi. Shi's 2700 tubes hold 19,366 kg at these dimensions
  // against Van-Dal's stated 44,500 kg, so adopting the count wholesale
  // undersized the bed by a factor of 2.3. That was invisible while the loop
  // pressure was being forced to a constant; once the drum runs at the real
  // reactor outlet, the undersized bundle produces a pressure drop of tens of
  // bar and the recycle loop cannot close. Deriving the count uses one more
  // sourced number and one fewer substitution
  const double per_tube_kg = reactor::catalyst_mass_per_tube_kg(b);
  b.n_tubes = per_tube_kg > 0.0
                  ? static_cast<int>(kVanDalCatalystChargeKg / per_tube_kg + 0.5)
                  : shi.n_tubes;

  b.source = "Van-Dal Table 2 catalyst properties and 44,500 kg charge; Shi et al. (2020) "
             "Sec. 3.1 tube diameter and length; tube count derived from the charge";
  return b;
}

}  // namespace presets

Co2H2PlantResult run_co2_h2_plant(double m_dot_CO2_kg_s, double m_dot_H2_kg_s,
                                   const Co2H2PlantConfig& cfg) {
  Co2H2PlantResult res;

  if (m_dot_CO2_kg_s <= 0.0 || m_dot_H2_kg_s <= 0.0) {
    res.message = "m_dot_CO2_kg_s and m_dot_H2_kg_s must both be positive";
    return res;
  }

  // 1. Fresh feeds, mixed
  const Stream co2_feed = co2_fresh_feed(m_dot_CO2_kg_s);
  const Stream h2_feed  = h2_fresh_feed(m_dot_H2_kg_s);
  res.fresh_feed = front_end::mix_streams({co2_feed, h2_feed});
  res.fresh_feed.pressure = units::barToPa(cfg.reactor_inlet_P_bar);

  // 2. Heat to the reactor inlet temperature
  const front_end::HXResult heated =
      front_end::heat_to_temperature(res.fresh_feed, cfg.reactor_inlet_T_K);
  if (!heated.ok) {
    res.message = "feed pre-heat failed: " + heated.message;
    return res;
  }
  res.reactor_inlet = heated.outlet;
  res.reactor_inlet.pressure = units::barToPa(cfg.reactor_inlet_P_bar);

  // 3. Resolve the bed, then run one tube
  reactor::ReactorConfig rcfg = cfg.reactor_cfg;
  if (rcfg.bed.n_tubes <= 1) rcfg.bed = presets::van_dal_catalyst_shi_tubes();
  res.n_tubes_used = rcfg.bed.n_tubes;
  if (res.n_tubes_used <= 0) {
    res.message = "bed geometry gives a non-positive tube count";
    return res;
  }

  Stream per_tube = res.reactor_inlet;
  for (auto& kv : per_tube.molar_flow) kv.second /= static_cast<double>(res.n_tubes_used);

  const reactor::ReactorState inlet = reactor::ReactorState::fromStream(per_tube);
  res.reactor = reactor::integrate_reactor(inlet, rcfg);
  if (!res.reactor.ok) {
    res.message = "reactor integration failed: " + res.reactor.message;
    return res;
  }

  res.co2_conversion_per_pass = res.reactor.co2_conversion(inlet);

  // 4. Back to plant scale
  res.reactor_outlet_plant_scale = res.reactor.outlet.toStream();
  for (auto& kv : res.reactor_outlet_plant_scale.molar_flow) {
    kv.second *= static_cast<double>(res.n_tubes_used);
  }

  const double M_meoh = properties(Species::CH3OH).molar_mass;
  res.meoh_production_kg_s =
      res.reactor_outlet_plant_scale.moleFraction(Species::CH3OH) *
      res.reactor_outlet_plant_scale.totalMolarFlow() * M_meoh / 1000.0;

  // 5. Knockout. The drum runs at the reactor's own outlet pressure unless the
  // caller asks for the old fixed-pressure behaviour
  const double drum_P_Pa = cfg.knockout_at_reactor_outlet_pressure
                               ? res.reactor.outlet.P_Pa
                               : units::barToPa(cfg.knockout_P_bar);
  res.knockout = front_end::separate(res.reactor_outlet_plant_scale, cfg.knockout_T_K, drum_P_Pa);
  if (!res.knockout.ok) {
    res.message = "knockout drum failed: " + res.knockout.message;
    return res;
  }

  res.ok = true;
  res.message = "ok";
  return res;
}

}  // namespace flowsheet
