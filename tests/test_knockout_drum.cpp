// Vapour/liquid knockout at the reactor outlet conditions

#include "front_end/knockout_drum.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== knockout drum ===\n\n");

  Stream feed;
  feed.temperature = 500.0; feed.pressure = 78e5;
  feed.molar_flow[Species::CH3OH] = 5.0;
  feed.molar_flow[Species::H2O]   = 5.0;
  feed.molar_flow[Species::CO2]   = 20.0;
  feed.molar_flow[Species::H2]    = 60.0;

  std::printf("[1] Van-Dal's stated drum conditions, 35 C and 78 bar\n");
  const auto r = front_end::separate(feed, 308.15, 78e5);
  checkTrue("ok", r.ok);
  checkTrue("two phases formed", !r.single_phase);
  checkTrue("the flash converged", r.converged);

  std::printf("\n[2] Conservation across the drum\n");
  check("total moles", r.vapor.totalMolarFlow() + r.liquid.totalMolarFlow(),
        feed.totalMolarFlow(), 1e-6);
  check("total mass", r.vapor.totalMassFlow() + r.liquid.totalMassFlow(),
        feed.totalMassFlow(), 1e-9);
  for (Species sp : {Species::CH3OH, Species::H2O, Species::CO2, Species::H2}) {
    check("species balance", flowOf(r.vapor, sp) + flowOf(r.liquid, sp), flowOf(feed, sp), 1e-6);
  }

  std::printf("\n[3] The split is physically the right way round\n");
  checkTrue("methanol mostly condenses",
            flowOf(r.liquid, Species::CH3OH) > 0.5 * 5.0);
  checkTrue("water mostly condenses",
            flowOf(r.liquid, Species::H2O) > 0.5 * 5.0);
  checkTrue("hydrogen stays in the vapour",
            flowOf(r.vapor, Species::H2) > 0.95 * 60.0);
  checkTrue("water removal fraction is a fraction",
            r.water_removal_fraction >= 0.0 && r.water_removal_fraction <= 1.0);
  checkTrue("vapour fraction is a fraction",
            r.vapor_fraction >= 0.0 && r.vapor_fraction <= 1.0);

  std::printf("\n[4] Colder means more condensate\n");
  const auto warm = front_end::separate(feed, 350.0, 78e5);
  checkTrue("ok", warm.ok);
  checkTrue("less liquid at the warmer temperature",
            warm.liquid.totalMolarFlow() <= r.liquid.totalMolarFlow() + 1e-9);

  std::printf("\n[5] A permanent-gas-only feed stays single phase\n");
  Stream gas;
  gas.temperature = 500.0; gas.pressure = 78e5;
  gas.molar_flow[Species::H2] = 50.0;
  const auto g = front_end::separate(gas, 308.15, 78e5);
  checkTrue("ok", g.ok);
  checkTrue("nothing condensed", g.single_phase || g.liquid.totalMolarFlow() < 1e-9);

  return report("knockout drum");
}
