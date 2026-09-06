#pragma once
// Fresh CO2 and H2 compression to loop pressure, for the economics layer
//
// The reactor chain assumes the mixed feed is delivered at reactor pressure
// and computes no compression work; that is correct for its mass and energy
// balance. This supplies the missing power and cost, from the real source
// pressures Van-Dal Sec. 2.3.1 states: CO2 1 bar multi-stage, H2 30 bar single
// stage, both to 78 bar
//
// Does not touch and is not called from the reactor chain. CO2 stage count of
// 4 is a reasonable choice, not stated by Van-Dal
// See docs/23-flowsheet-composition.md

#include <string>
#include "front_end/compressor_train.hpp"

namespace flowsheet {

struct CompressionTrainConfig {
  front_end::CompressorConfig co2_stage_cfg;   // T_in_K=298.15, eta_is=0.8, eta_mech=0.877 defaults -- see compressor_train.hpp header for the CO2-extension reasoning and the real-gas caveat near CO2's critical point
  front_end::CompressorConfig h2_stage_cfg;    // same defaults -- this IS Mucci's own validated H2 case

  double co2_P_in_bar = 1.0;    // SOURCED: Van-Dal Sec. 2.3.1, fresh CO2 source condition
  double h2_P_in_bar = 30.0;    // SOURCED: Van-Dal Sec. 2.3.1, fresh H2 source condition
  double P_out_bar = 78.0;      // SOURCED: Van-Dal Sec. 2.3.1, reactor inlet pressure (co2_h2_plant.hpp's own default)

  int co2_n_stages = 4;             // reasonable engineering default, NOT Van-Dal-sourced -- see header
  int h2_n_units_parallel = 2;      // Mucci Sec. 3.3's own stated practice -- see header
};

struct CompressionTrainResult {
  front_end::CompressorTrainResult co2_train;   // 4-stage (default) CO2 result
  front_end::CompressorTrainResult h2_train;    // single-stage H2 result (matches compressor.hpp's own model)

  double total_power_MW = 0.0;     // co2_train.total_P_comp_MW + h2_train.total_P_comp_MW
  double total_cooling_MW = 0.0;   // co2_train.total_P_cooling_MW + h2_train.total_P_cooling_MW
  double h2_unit_power_kW = 0.0;   // h2_train.total_P_comp_MW * 1000 / h2_n_units_parallel -- see header note on Turton range

  bool ok = false;
  std::string message;
};

CompressionTrainResult run_compression_train(double m_dot_CO2_fresh_kg_s, double m_dot_H2_fresh_kg_s,
                                              const CompressionTrainConfig& cfg = CompressionTrainConfig{});

}  // namespace flowsheet
