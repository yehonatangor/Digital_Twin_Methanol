#include "recycle.hpp"

namespace front_end {

RecycleSplitResult split_recycle_purge(const Stream& unreacted_gas, const RecycleConfig& cfg) {
  RecycleSplitResult r;

  if (!(cfg.recycle_fraction >= 0.0 && cfg.recycle_fraction <= 1.0)) {
    r.message = "recycle_fraction must be in [0, 1]";
    return r;
  }

  r.recycle.temperature = unreacted_gas.temperature;
  r.recycle.pressure    = unreacted_gas.pressure;
  r.recycle.phase       = unreacted_gas.phase;
  r.purge.temperature   = unreacted_gas.temperature;
  r.purge.pressure      = unreacted_gas.pressure;
  r.purge.phase         = unreacted_gas.phase;

  // Uniform split: identical composition in both outlets, only the scale differs
  for (const auto& [sp, n] : unreacted_gas.molar_flow) {
    if (n <= 0.0) continue;
    const double rec = cfg.recycle_fraction * n;
    const double pur = n - rec;
    if (rec > 0.0) r.recycle.molar_flow[sp] = rec;
    if (pur > 0.0) r.purge.molar_flow[sp]   = pur;
  }

  r.ok = true;
  r.message = "ok";
  return r;
}

}  // namespace front_end
