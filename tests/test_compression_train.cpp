// Feed compression: CO2 from 1 bar and H2 from 30 bar, both to 78 bar

#include "flowsheet/compression_train.hpp"
#include "flowsheet/co2_h2_plant.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== feed compression train ===\n\n");

  flowsheet::CompressionTrainConfig cfg;
  std::printf("[1] Van-Dal's stated source and target pressures\n");
  check("CO2 suction, bar", cfg.co2_P_in_bar,  1.0, 1e-12);
  check("H2 suction, bar",  cfg.h2_P_in_bar,  30.0, 1e-12);
  check("discharge, bar",   cfg.P_out_bar,    78.0, 1e-12);
  check("CO2 stages",       static_cast<double>(cfg.co2_n_stages), 4.0, 0.0);
  check("H2 units in parallel",
        static_cast<double>(cfg.h2_n_units_parallel), 2.0, 0.0);

  const double co2 = flowsheet::presets::van_dal_industrial_co2_kg_s();
  const double h2  = flowsheet::presets::stoichiometric_h2_feed_kg_s(co2);
  const auto r = flowsheet::run_compression_train(co2, h2, cfg);

  std::printf("\n[2] Both trains run\n");
  checkTrue("ok", r.ok);
  checkTrue("CO2 train ok", r.co2_train.ok);
  checkTrue("H2 train ok",  r.h2_train.ok);
  check("CO2 stages recorded", static_cast<double>(r.co2_train.stages.size()), 4.0, 0.0);
  check("H2 stages recorded",  static_cast<double>(r.h2_train.stages.size()),  1.0, 0.0);
  std::printf("       [INFO] CO2 %.2f MW, H2 %.2f MW, total %.2f MW\n",
              r.co2_train.total_P_comp_MW, r.h2_train.total_P_comp_MW, r.total_power_MW);

  std::printf("\n[3] Totals add up\n");
  check("total power", r.total_power_MW,
        r.co2_train.total_P_comp_MW + r.h2_train.total_P_comp_MW, 1e-9);
  check("total cooling", r.total_cooling_MW,
        r.co2_train.total_P_cooling_MW + r.h2_train.total_P_cooling_MW, 1e-9);
  check("per-unit H2 power, kW", r.h2_unit_power_kW,
        r.h2_train.total_P_comp_MW * 1000.0 / 2.0, 1e-9);

  std::printf("\n[4] Compressing CO2 from 1 bar dominates the duty\n");
  checkTrue("CO2 costs more than H2", r.co2_train.total_P_comp_MW > r.h2_train.total_P_comp_MW);
  check("overall CO2 ratio", r.co2_train.overall_beta, 78.0, 1e-6);
  check("overall H2 ratio",  r.h2_train.overall_beta,  78.0 / 30.0, 1e-6);

  std::printf("\n[5] The CO2 extension carries a stated caveat\n");
  checkTrue("the message flags the real-gas caveat near CO2's critical point",
            r.message.find("real-gas") != std::string::npos);

  std::printf("\n[6] Guards\n");
  checkTrue("a zero feed is refused", !flowsheet::run_compression_train(0.0, h2, cfg).ok);

  return report("compression train");
}
