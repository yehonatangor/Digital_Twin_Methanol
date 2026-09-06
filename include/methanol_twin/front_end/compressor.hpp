#pragma once
// Isentropic compression with efficiency losses
//
// Mucci et al. (2023) Sec. 3.3:
//   P_comp = m_dot*cp*T_in / (eta_is*eta_mech) * (beta^((k-1)/k) - 1)
// k = cp/(cp - R) is COMPUTED at the inlet temperature from the same heat
// capacity correlation everything else uses, not tabulated, so hydrogen's cp
// has one source of truth
//
// eta_is = 0.8, stated by Mucci
// eta_mech = 0.877 is DERIVED, back-solved as the mean of the ratios implied
// by Mucci Table A.2's validation rows. eta_mechanical_sourced = false
//
// Below half of rated per-unit flow an advisory turndown flag is set. It is
// advisory because there is no surge correlation here
// See docs/16-compression-and-heat-exchange.md

#include <string>
#include "species.hpp"

namespace front_end {

struct CompressorConfig {
  double T_in_K = 298.15;          // Mucci Sec. 3.3
  double eta_isentropic = 0.8;     // Mucci Sec. 3.3

  double eta_mechanical = 0.877;   // derived, mean of 4 points back-solved from Table A.2
  bool   eta_mechanical_sourced = false;
  const char* eta_mechanical_source =
      "Derived: back-solved from Mucci Table A.2, not printed in the text.";

  int    n_units_parallel = 1;             // Sec. 3.3
  double min_load_fraction_single = 0.5;   // Sec. 3.3, single-unit range
  double capacity_per_unit_kg_s = 0.0;     // 0 disables the turndown check
};

struct CompressorInput {
  double m_dot_H2_kg_s;   // total hydrogen mass flow through the compression unit
  double P_in_bar;        // suction pressure
  double P_out_bar;       // discharge pressure
};

struct CompressorResult {
  double beta = 0.0;                   // P_out / P_in
  double cp_J_per_kgK = 0.0;           // thermo::cp(H2, T_in), the value actually used
  double k_isentropic_exponent = 0.0;  // cp / (cp - R)

  double P_comp_MW = 0.0;              // shaft power
  double P_cooling_MW = 0.0;           // equals P_comp_MW per Mucci

  bool below_min_turndown = false;     // advisory
  bool ok = false;
  std::string message;
};

CompressorResult run_compressor(const CompressorInput& in, const CompressorConfig& cfg);

// -----------------------------------------------------------------------------
// Presets
// -----------------------------------------------------------------------------
namespace presets {

CompressorConfig mucci_h2_compressor();

// Mucci Table A.2 validation case: 1900 kg/h H2, 40 bar inlet, 298 K
// beta of 2.0, 2.5, 3.0 or 3.5 reproduces the table's four rows
CompressorInput mucci_table_a2_case(double beta);

inline constexpr bool kMucciEtaMechanicalSourced = false;

}  // namespace presets

}  // namespace front_end
