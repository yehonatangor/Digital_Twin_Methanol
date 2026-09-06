#pragma once
// =============================================================================
// recycle.hpp -- unreacted-gas recycle/purge split
// =============================================================================
// SOURCE. Mucci et al. (2023), Sec. "Methanol synthesis plant" (p.16, the
// same page electrolyzer.hpp's Table A.2 compressor validation sits near --
// same core paper): "After the second stage of the reactor, the unreacted
// gases are separated from crude methanol, the mixture of water and
// methanol, via condensation and partially (98%) recycled back to the
// reactor." The condensation step is knockout_drum.hpp (already generic,
// reused here too -- see its header); THIS module is the split that happens
// to the resulting unreacted-gas vapor stream: 98% recycled, the remainder
// purged (to prevent inert/impurity buildup in the loop, standard recycle-
// loop practice, implicit in Mucci's own "partially" wording -- if 100%
// were recycled there would be no purge stream for the membrane step in
// membrane.hpp to act on)
//
// SCOPE -- a single lumped split fraction, not species-selective. Mucci
// gives one number (98%) for the whole unreacted-gas stream, not a per-
// species split -- so this module applies it uniformly (same composition
// in both outlets, just scaled flow). Any species-selective separation
// (e.g. preferentially recovering H2) is membrane.hpp's job, applied
// downstream to the purge stream this module produces, exactly as Mucci
// describes: "Also, a membrane separation process [60] was considered to
// partially (90%) recover H2 from the purge stream."
//
// MERGING THE RECYCLE STREAM BACK WITH FRESH FEED. There is no dedicated
// function for this here -- it is exactly what feed_blend::mix_streams()
// already does generically (mole + energy balance over any number of
// streams). Call front_end::mix_streams({fresh_feed, recycle.recycle}) to
// get the combined reactor-inlet stream
//
// USER-TUNABLE BY DESIGN. RecycleConfig::recycle_fraction defaults to
// Mucci's sourced 0.98 but is a plain, overridable field
// =============================================================================

#include <string>
#include "stream.hpp"

namespace front_end {

struct RecycleConfig {
  double recycle_fraction = 0.98;   // Mucci et al. (2023), p.16
};

struct RecycleSplitResult {
  Stream recycle;   // same composition, T and P as the feed
  Stream purge;
  bool   ok = false;
  std::string message;
};

// Uniform split across every species; only the flow scale differs
RecycleSplitResult split_recycle_purge(const Stream& unreacted_gas,
                                        const RecycleConfig& cfg = RecycleConfig{});

namespace presets {
inline constexpr double kMucciRecycleFraction = 0.98;
}

}  // namespace front_end
