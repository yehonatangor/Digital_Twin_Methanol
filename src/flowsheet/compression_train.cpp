#include "compression_train.hpp"

#include "species.hpp"

namespace flowsheet {

CompressionTrainResult run_compression_train(double m_dot_CO2_fresh_kg_s,
                                              double m_dot_H2_fresh_kg_s,
                                              const CompressionTrainConfig& cfg) {
  CompressionTrainResult res;

  if (m_dot_CO2_fresh_kg_s <= 0.0 || m_dot_H2_fresh_kg_s <= 0.0) {
    res.message = "both fresh feed rates must be positive";
    return res;
  }
  if (cfg.co2_n_stages <= 0 || cfg.h2_n_units_parallel <= 0) {
    res.message = "stage and parallel-unit counts must be positive";
    return res;
  }

  // CO2: 1 -> 78 bar, intercooled
  front_end::CompressorTrainConfig co2_cfg;
  co2_cfg.stage_cfg = cfg.co2_stage_cfg;
  co2_cfg.n_stages  = cfg.co2_n_stages;
  res.co2_train = front_end::run_compressor_train(Species::CO2, m_dot_CO2_fresh_kg_s,
                                                   cfg.co2_P_in_bar, cfg.P_out_bar, co2_cfg);
  if (!res.co2_train.ok) {
    res.message = "CO2 train failed: " + res.co2_train.message;
    return res;
  }

  // H2: 30 -> 78 bar, single stage. This is Mucci's own validated case
  front_end::CompressorTrainConfig h2_cfg;
  h2_cfg.stage_cfg = cfg.h2_stage_cfg;
  h2_cfg.n_stages  = 1;
  res.h2_train = front_end::run_compressor_train(Species::H2, m_dot_H2_fresh_kg_s,
                                                  cfg.h2_P_in_bar, cfg.P_out_bar, h2_cfg);
  if (!res.h2_train.ok) {
    res.message = "H2 train failed: " + res.h2_train.message;
    return res;
  }

  res.total_power_MW   = res.co2_train.total_P_comp_MW + res.h2_train.total_P_comp_MW;
  res.total_cooling_MW = res.co2_train.total_P_cooling_MW + res.h2_train.total_P_cooling_MW;
  res.h2_unit_power_kW =
      res.h2_train.total_P_comp_MW * 1000.0 / static_cast<double>(cfg.h2_n_units_parallel);

  res.ok = true;
  res.message =
      "ok. The CO2 duty uses the same correlation Mucci validated for hydrogen, extended to CO2; "
      "near CO2's critical point that carries a real-gas caveat the H2 number does not.";
  return res;
}

}  // namespace flowsheet
