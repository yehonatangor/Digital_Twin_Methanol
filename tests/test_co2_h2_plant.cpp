// Single-pass CO2 + H2 reactor chain at industrial scale
//
// Van-Dal & Bouallou (2013): 88.0 t/h CO2, 210 C into the reactor at 78 bar,
// cooled to 35 C. The bed is Van-Dal's Table 2 catalyst in Shi's tube
// geometry, with the tube count derived from Van-Dal's own 44,500 kg charge

#include "flowsheet/co2_h2_plant.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== single-pass CO2 + H2 plant ===\n\n");

  std::printf("[1] Van-Dal's stated feed rates\n");
  const double co2 = flowsheet::presets::van_dal_industrial_co2_kg_s();
  check("88.0 t/h in kg/s", co2, 88.0 * 1000.0 / 3600.0, 1e-9);
  const double h2 = flowsheet::presets::stoichiometric_h2_feed_kg_s(co2);
  // CO2 + 3 H2, so 3 mol H2 per mol CO2
  check("stoichiometric H2, kg/s", h2, co2 / 44.010 * 3.0 * 2.016, 0.01);

  std::printf("\n[2] The bed is sized from Van-Dal's published charge\n");
  const auto bed = flowsheet::presets::van_dal_catalyst_shi_tubes();
  check("catalyst charge, kg", reactor::catalyst_mass_total_kg(bed), 44500.0, 5.0);
  check("derived tube count", static_cast<double>(bed.n_tubes), 6204.0, 1.0);
  check("Shi tube diameter, m", bed.tube_inner_diameter_m, 0.035, 1e-9);
  check("Shi tube length, m",   bed.bed_length_m,          7.0,   1e-9);
  // Van-Dal's own Table 2 catalyst properties must survive the substitution
  check("void fraction",            bed.void_fraction,          0.4,    1e-9);
  check("particle diameter, m",     bed.particle_diameter_m,    0.0055, 1e-9);
  check("particle density, kg/m3",  bed.particle_density_kg_m3, 1775.0, 1e-9);

  std::printf("\n[3] The plant runs and reproduces the documented single pass\n");
  flowsheet::Co2H2PlantConfig cfg;
  const auto r = flowsheet::run_co2_h2_plant(co2, h2, cfg);
  checkTrue("ok", r.ok);
  check("tubes used", static_cast<double>(r.n_tubes_used), 6204.0, 1.0);
  // The plant reactor is a jacketed BWR, not adiabatic, so this is the cooled
  // single-pass figure. Adiabatic gave 24.0 % and ran the bed to 284 C
  check("single-pass CO2 conversion, %", 100.0 * r.co2_conversion_per_pass, 27.1, 0.5);
  checkTrue("the plant reactor is cooled, not adiabatic",
            flowsheet::plant_reactor_defaults().thermal == reactor::ThermalMode::Cooled);
  check("outlet stays in the catalyst's safe window, C",
        r.reactor.outlet.T_K - 273.15, 245.4, 3.0);
  checkTrue("methanol is produced", r.meoh_production_kg_s > 0.0);

  std::printf("\n[4] Pressure drop across the 6204-tube bundle\n");
  const double dP_bar = (78e5 - r.reactor.outlet.P_Pa) / 1e5;
  check("single-pass drop, bar", dP_bar, 0.194, 0.02);
  checkTrue("the drop is small enough for a recycle loop to close", dP_bar < 1.0);

  std::printf("\n[5] Conditions follow Van-Dal's flowsheet\n");
  check("reactor inlet, K", r.reactor_inlet.temperature, 483.15, 1e-6);
  checkTrue("the bed heats up (exothermic)", r.reactor.outlet.T_K > 483.15);
  check("clamp absorbed nothing", r.reactor.max_clamp_rel, 0.0, 1e-12);

  std::printf("\n[6] The drum runs at the reactor's real outlet pressure\n");
  checkTrue("two phases formed", !r.knockout.single_phase);
  check("moles conserved across the drum",
        r.knockout.vapor.totalMolarFlow() + r.knockout.liquid.totalMolarFlow(),
        r.reactor_outlet_plant_scale.totalMolarFlow(), 1e-6);
  checkTrue("methanol reports to the condensate",
            flowOf(r.knockout.liquid, Species::CH3OH) > 0.0);

  std::printf("\n[7] Fresh feed conditions\n");
  const Stream c = flowsheet::co2_fresh_feed(co2);
  check("CO2 delivered at 1 bar", c.pressure / 1e5, 1.0, 1e-9);
  check("CO2 delivered at 25 C",  c.temperature, 298.15, 1e-9);
  const Stream hh = flowsheet::h2_fresh_feed(h2);
  check("H2 delivered at 30 bar", hh.pressure / 1e5, 30.0, 1e-9);

  std::printf("\n[8] Guards\n");
  checkTrue("a zero CO2 feed is refused", !flowsheet::run_co2_h2_plant(0.0, h2, cfg).ok);
  checkTrue("a zero H2 feed is refused",  !flowsheet::run_co2_h2_plant(co2, 0.0, cfg).ok);

  return report("single-pass plant");
}
