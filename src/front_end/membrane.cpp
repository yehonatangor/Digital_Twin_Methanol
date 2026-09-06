#include "membrane.hpp"

namespace front_end {

MembraneResult separate_h2_membrane(const Stream& purge, const MembraneConfig& cfg) {
  MembraneResult r;

  if (!(cfg.h2_recovery_fraction >= 0.0 && cfg.h2_recovery_fraction <= 1.0)) {
    r.message = "h2_recovery_fraction must be in [0, 1]";
    return r;
  }

  r.permeate.temperature  = purge.temperature;
  r.permeate.pressure     = purge.pressure;
  r.permeate.phase        = purge.phase;
  r.retentate.temperature = purge.temperature;
  r.retentate.pressure    = purge.pressure;
  r.retentate.phase       = purge.phase;

  for (const auto& [sp, n] : purge.molar_flow) {
    if (n <= 0.0) continue;
    if (sp == Species::H2) {
      const double recovered = cfg.h2_recovery_fraction * n;
      if (recovered > 0.0) r.permeate.molar_flow[sp] = recovered;
      const double rejected = n - recovered;
      if (rejected > 0.0) r.retentate.molar_flow[sp] = rejected;
      r.h2_recovered_mol_s = recovered;
    } else {
      // No other species is given any permeate recovery; Mucci states none
      r.retentate.molar_flow[sp] = n;
    }
  }

  r.ok = true;
  r.message = "ok";
  return r;
}

}  // namespace front_end
