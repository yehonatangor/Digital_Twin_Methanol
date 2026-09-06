#pragma once
// Methanol purification as a recovery and purity specification
//
// LUMPED BY NECESSITY, not by simplification: neither Van-Dal nor Shi
// publishes the tray-level VLE a rigorous MESH column would need. Shi Sec. 3.1
// states 99.5 % methanol recovery at 99.9 wt% purity, and those are the
// defaults. There is no internal reflux ratio or stage count; column geometry
// for costing is built separately in economics/column_sizing.hpp
//
// Permanent gases dissolved in the feed are routed to a vent (light_ends)
// rather than into the wastewater bottoms, which would otherwise corrupt the
// bottoms bubble point
// See docs/17-separations.md

#include <string>
#include "stream.hpp"

namespace front_end {

struct DistillationConfig {
  // Shi et al. (2020) Sec. 2.5
  double methanol_recovery_fraction = 0.995;
  double product_purity_wt_fraction = 0.999;
};

struct DistillationResult {
  Stream distillate;   // methanol product
  Stream bottoms;      // water and unrecovered methanol
  // Permanent gases dissolved in the crude feed. At column conditions these
  // are non-condensable, so they leave the overhead system as a vent rather
  // than dissolving into the wastewater. Small in quantity, but sending them
  // to the bottoms would put carbon dioxide and hydrogen into a water stream
  // they physically cannot stay in
  Stream light_ends;

  double methanol_recovered_mol_s = 0.0;
  double actual_purity_wt_fraction = 0.0;  // equals target unless water-limited
  double light_ends_mol_s = 0.0;           // total vented, zero for a clean feed

  bool ok = false;
  std::string message;
};

DistillationResult purify_methanol(const Stream& crude_methanol,
                                    const DistillationConfig& cfg = DistillationConfig{});

namespace presets {
inline constexpr double kShiMethanolRecoveryFraction = 0.995;   // Shi et al. (2020)
inline constexpr double kShiProductPurityWtFraction   = 0.999;   // Shi et al. (2020)
inline constexpr double kMucciAAGradePurityWtFraction = 0.9985;  // Mucci et al. (2023)
}

}  // namespace front_end
