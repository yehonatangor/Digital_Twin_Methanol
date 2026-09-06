#pragma once
// Multi-stage compression with intercooling, species-generalised
//
// Equal pressure ratio per stage, beta_stage = (P_out/P_in)^(1/N), which is the
// optimum for identical stages. Each stage uses front_end/compressor.hpp
// unchanged and returns to the inlet temperature between stages
//
// Van-Dal Sec. 2.3.1 states the split this reproduces: CO2 multi-stage
// intercooled 1 -> 78 bar, H2 single stage 30 -> 78 bar. The CO2 stage count
// of 4 is a reasonable engineering choice, NOT stated by Van-Dal
// See docs/16-compression-and-heat-exchange.md

#include <string>
#include <vector>

#include "compressor.hpp"
#include "species.hpp"
#include "stream.hpp"

namespace front_end {

// One stage of the Mucci Sec. 3.3 equation. For Species::H2 this reproduces
// run_compressor() bit for bit
CompressorResult run_compressor_stage(Species species, double m_dot_kg_s, double P_in_bar,
                                       double P_out_bar, const CompressorConfig& cfg = CompressorConfig{});

// Mixture overload. The same equation, with cp taken as the mole-weighted
// mixture heat capacity from thermo::cp and k from the Mayer relation on that
// mixture value. Mass flow comes from the stream itself. For a single-species
// stream this reproduces the overload above exactly
//
// The mixture k is an ideal-gas quantity. For a recycle stream that is mostly
// hydrogen and carbon oxides near 78 bar this is the same approximation the
// single-species path already makes, and it carries the same caveat near CO2's
// critical region
CompressorResult run_compressor_stage_mixture(const Stream& gas, double P_in_bar, double P_out_bar,
                                               const CompressorConfig& cfg = CompressorConfig{});

struct CompressorTrainConfig {
  CompressorConfig stage_cfg;
  int n_stages = 1;             // >1 gives an intercooled train
};

struct CompressorTrainResult {
  std::vector<CompressorResult> stages;   // in order
  double overall_beta = 0.0;              // total ratio, not per stage
  double total_P_comp_MW = 0.0;
  double total_P_cooling_MW = 0.0;        // intercoolers plus aftercooler
  bool   ok = false;
  std::string message;
};

// Equal-ratio stages intercooled back to cfg.stage_cfg.T_in_K
CompressorTrainResult run_compressor_train(Species species, double m_dot_kg_s, double P_in_bar,
                                            double P_out_bar,
                                            const CompressorTrainConfig& cfg = CompressorTrainConfig{});

}  // namespace front_end
