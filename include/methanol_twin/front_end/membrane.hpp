#pragma once
// =============================================================================
// membrane.hpp -- H2-selective membrane recovery from the purge stream
// =============================================================================
// SOURCE. Mucci et al. (2023), p.16 text (same paragraph recycle.hpp is
// sourced from): "Also, a membrane separation process [60] was considered
// to partially (90%) recover H2 from the purge stream." Ref [60]:
// Ramirez-Santos, A.A.; Castel, C.; Favre, E. "Utilization of blast furnace
// flue gas: Opportunities and challenges for polymeric membrane gas
// separation processes." J. Membr. Sci. 2017, 526, 191-204 -- Mucci's basis
// for treating a polymeric H2-selective membrane as a single lumped
// recovery fraction rather than modelling permeance/selectivity directly
//
// SCOPE -- LUMPED RECOVERY, NOT A TRANSPORT MODEL. Mucci publishes one
// number (90% H2 recovery) and no permeance, selectivity, membrane area, or
// pressure-drop data for this unit. Rather than inventing those (which
// would be exactly the kind of fabricated, unmeasured parameter this
// project avoids -- see trm_reactor.hpp's header on why a calibrated DRM
// term was rejected), this module implements ONLY what is stated: a
// fraction of the purge stream's H2 reports to the permeate, and
// EVERYTHING ELSE -- all other species, plus the remaining (1-recovery)
// share of H2 -- reports to the retentate. No other species is given any
// permeate recovery (Mucci states none), and permeate/retentate are left
// at the purge stream's own temperature and pressure (Mucci gives no
// membrane pressure drop either -- a real polymeric membrane does have one,
// this is a stated simplification, not a hidden one)
//
// USER-TUNABLE BY DESIGN. MembraneConfig::h2_recovery_fraction defaults to
// Mucci's sourced 0.90 but is a plain, overridable field
// =============================================================================

#include <string>
#include "stream.hpp"

namespace front_end {

struct MembraneConfig {
  double h2_recovery_fraction = 0.90;   // Mucci et al. (2023), p.16
};

struct MembraneResult {
  Stream permeate;    // recovered H2 only
  Stream retentate;   // everything else, plus the unrecovered H2
  double h2_recovered_mol_s = 0.0;
  bool   ok = false;
  std::string message;
};

MembraneResult separate_h2_membrane(const Stream& purge, const MembraneConfig& cfg = MembraneConfig{});

namespace presets {
inline constexpr double kMucciH2RecoveryFraction = 0.90;
}

}  // namespace front_end
