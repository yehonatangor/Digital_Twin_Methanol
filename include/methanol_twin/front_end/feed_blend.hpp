#pragma once
// Stoichiometric-number blending of two feed streams
//
// Lim et al. (2022) Eq. (4):  SN = (H2 - CO2) / (CO + CO2)
// SN = 2 is the methanol synthesis target. Solves for the mixing ratio that
// puts a blend on a target SN, or reports the SN of a given blend
// Unreachable targets are reported, not silently clamped
// See docs/18-feed-blending.md

#include <string>
#include <vector>
#include "species.hpp"
#include "stream.hpp"

namespace front_end {

struct BlendConfig {
  double target_SN = 2.0;       // Lim et al. (2022) Eq. (4)

  int    max_iter    = 100;
  double T_tol_K      = 1e-6;
  double T_floor_K    = 100.0;  // guard rails, not physical bounds
  double T_ceiling_K  = 3000.0;
};

// (H2 - CO2) / (CO + CO2), Lim Eq. (4). Throws if (CO + CO2) <= 0
double stoichiometric_number(const Stream& s);

// Returns a zero-flow Stream if every inlet is empty
Stream mix_streams(const std::vector<Stream>& inlets,
                    const BlendConfig& cfg = BlendConfig{});

struct BlendResult {
  Stream blended;
  double SN_before_topup = 0.0;   // syngas alone
  double SN_after = 0.0;
  double h2_topup_mol_s = 0.0;    // hydrogen actually added
  bool   ok = false;
  std::string message;
};

// Adds hydrogen from h2_stream to syngas until cfg.target_SN is reached,
// then mixes them (mole + energy balance, via mix_streams())
//
// Closed form, from SN's definition: with x mol/s of pure H2 added,
//   target = (H2 + x - CO2) / (CO + CO2)  =>  x = target*(CO+CO2) - H2 + CO2
//
// If x <= 0, syngas is already at/above the target SN: no hydrogen is added
// (NOT force-diluted down to exactly the target), h2_topup_mol_s = 0,
// SN_after = SN_before_topup, ok = true
//
// If x exceeds h2_stream's available H2 flow, only what is available is
// added, ok = false, and message explains the shortfall -- this is an
// electrolyzer-capacity / plant-sizing question the caller must resolve
// (e.g. add a module via ElectrolyzerConfig::n_modules), not something this
// function silently papers over
//
// h2_stream's non-H2 species (if any -- e.g. trace O2 carryover) are scaled
// by the same factor as H2 when only part of the stream is used, so its
// composition ratio is preserved in what gets blended in
BlendResult blend_to_target_SN(const Stream& syngas, const Stream& h2_stream,
                                const BlendConfig& cfg = BlendConfig{});

}  // namespace front_end
