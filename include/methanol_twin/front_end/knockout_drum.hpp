#pragma once
// Vapour-liquid knockout drum
//
// A named wrapper around flash::solve, not a split-fraction approximation:
// the real VLE at the specified T and P decides what condenses, so there is no
// removal efficiency to source and none is invented
//
// T and P are required arguments, not defaults. Van-Dal states 35 C and Shi
// 40 C, so a default would be picking one plant's condition for all
//
// Reports how many liquid pairs lacked fitted NRTL parameters, which matters
// most here: the condensate is the CO2/methanol/water mixture for which
// ChemSep has no CO2 binaries. At loop pressure the flash takes the fugacity
// route, which does have them, so the gap is bounded
// See docs/17-separations.md

#include <string>
#include "stream.hpp"

namespace front_end {

struct KnockoutDrumResult {
  Stream vapor;
  Stream liquid;   // condensate

  double water_removal_fraction = 0.0;  // liquid H2O / feed H2O, molar
  double vapor_fraction = 0.0;
  bool   single_phase = false;          // nothing condensed
  bool   converged = false;

  bool   ok = false;
  std::string message;
};

// Flashes at (T_K, P_Pa). The feed's own T and P are not used; pass them
// explicitly to flash the stream as-is
KnockoutDrumResult separate(const Stream& feed, double T_K, double P_Pa);

}  // namespace front_end
