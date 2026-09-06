// Heating, cooling and condensation

#include "front_end/heat_exchanger.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== heat exchanger ===\n\n");

  Stream s;
  s.temperature = 298.15; s.pressure = 78e5;
  s.molar_flow[Species::CO2] = 10.0;
  s.molar_flow[Species::H2]  = 30.0;

  std::printf("[1] Heating to a target temperature\n");
  const auto hot = front_end::heat_to_temperature(s, 483.15);
  checkTrue("ok", hot.ok);
  check("outlet temperature", hot.outlet.temperature, 483.15, 1e-6);
  checkTrue("heating takes a positive duty", hot.duty_MW > 0.0);
  check("composition is untouched", flowOf(hot.outlet, Species::CO2), 10.0, 1e-12);

  std::printf("\n[2] Cooling is the same calculation with the opposite sign\n");
  const auto cold = front_end::heat_to_temperature(hot.outlet, 298.15);
  checkTrue("cooling duty is negative", cold.duty_MW < 0.0);
  check("the round trip closes", cold.duty_MW, -hot.duty_MW, 1e-6);

  std::printf("\n[3] Van-Dal quotes two different reactor temperatures; keep them apart\n");
  // Table A.3's LABORATORY reactor runs at 220 C, which is what this preset
  // carries. The INDUSTRIAL flowsheet inlet in Sec. 2.3.1 is 210 C, and
  // co2_h2_plant.hpp carries that one separately. Conflating them would shift
  // every plant-scale conversion figure, so both are asserted here
  check("industrial inlet, Sec. 2.3.1 HX4, is 210 C",
        front_end::presets::van_dal_industrial_reactor_inlet_T_K(), 483.15, 1e-9);
  check("laboratory case, Table A.3, is 220 C",
        front_end::presets::van_dal_lab_reactor_T_K(), 493.15, 1e-9);
  checkTrue("the two are genuinely different values",
            front_end::presets::van_dal_lab_reactor_T_K() >
            front_end::presets::van_dal_industrial_reactor_inlet_T_K());

  std::printf("\n[4] Applying a duty and reading back the temperature are inverses\n");
  const auto by_duty = front_end::heat_with_duty(s, hot.duty_MW);
  checkTrue("ok", by_duty.ok);
  check("recovers the same outlet temperature", by_duty.outlet.temperature, 483.15, 1e-3);

  std::printf("\n[5] Condensing cool splits the phases and reports latent share\n");
  Stream wet;
  wet.temperature = 500.0; wet.pressure = 78e5;
  wet.molar_flow[Species::CH3OH] = 5.0;
  wet.molar_flow[Species::H2O]   = 5.0;
  wet.molar_flow[Species::H2]    = 50.0;
  const auto cc = front_end::cool_with_condensation(wet, 308.15);
  checkTrue("ok", cc.ok);
  checkTrue("heat was removed", cc.duty_MW < 0.0);
  checkTrue("something condensed", cc.condensed);
  checkTrue("latent share is a fraction", cc.latent_fraction >= 0.0 && cc.latent_fraction <= 1.0);
  check("moles are conserved across the split",
        cc.vapor_outlet.totalMolarFlow() + cc.liquid_outlet.totalMolarFlow(),
        wet.totalMolarFlow(), 1e-6);
  checkTrue("the liquid is condensable species, not hydrogen",
            flowOf(cc.liquid_outlet, Species::H2) < 0.05 * 50.0);

  std::printf("\n[6] With nothing condensable it reduces to plain cooling\n");
  Stream drygas;
  drygas.temperature = 500.0; drygas.pressure = 78e5;
  drygas.molar_flow[Species::H2] = 50.0;
  const auto dry = front_end::cool_with_condensation(drygas, 308.15);
  checkTrue("ok", dry.ok);
  checkTrue("nothing condensed", !dry.condensed);
  check("latent share is zero", dry.latent_fraction, 0.0, 1e-12);

  std::printf("\n[7] Two-stream exchange conserves energy\n");
  Stream hot_in;  hot_in.temperature = 600.0; hot_in.pressure = 1e5;
  hot_in.molar_flow[Species::H2O] = 10.0;
  Stream cold_in; cold_in.temperature = 300.0; cold_in.pressure = 1e5;
  cold_in.molar_flow[Species::CO2] = 10.0;
  const auto x = front_end::exchange_two_streams(hot_in, cold_in, 5000.0);
  checkTrue("ok", x.ok);
  checkTrue("the hot stream cooled",  x.hot_outlet.temperature  < 600.0);
  checkTrue("the cold stream heated", x.cold_outlet.temperature > 300.0);
  checkTrue("effectiveness is a fraction", x.effectiveness >= 0.0 && x.effectiveness <= 1.0);
  // In counterflow the hot outlet may legitimately end up below the cold
  // outlet. The real thermodynamic limits are the two INLET temperatures
  checkTrue("hot outlet cannot fall below the cold inlet",
            x.hot_outlet.temperature >= 300.0 - 1e-6);
  checkTrue("cold outlet cannot rise above the hot inlet",
            x.cold_outlet.temperature <= 600.0 + 1e-6);
  checkTrue("duty is positive", x.duty_MW > 0.0);

  return report("heat exchanger");
}
