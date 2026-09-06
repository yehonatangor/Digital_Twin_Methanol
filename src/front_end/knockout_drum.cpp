#include "knockout_drum.hpp"

#include "flash.hpp"

namespace front_end {

namespace {

double mol_s_of(const Stream& s, Species sp) {
  auto it = s.molar_flow.find(sp);
  return (it == s.molar_flow.end()) ? 0.0 : it->second;
}

}  // namespace

KnockoutDrumResult separate(const Stream& feed, double T_K, double P_Pa) {
  KnockoutDrumResult r;

  if (!(T_K > 0.0)) {
    r.message = "knockout_drum::separate: T_K must be positive";
    return r;
  }
  if (!(P_Pa > 0.0)) {
    r.message = "knockout_drum::separate: P_Pa must be positive";
    return r;
  }
  if (!(feed.totalMolarFlow() > 0.0)) {
    r.message = "knockout_drum::separate: feed has zero total flow";
    return r;
  }

  // A knockout drum is a flash vessel. No new physics here
  flash::FlashResult f;
  try {
    f = flash::solve(feed, T_K, P_Pa);
  } catch (const std::exception& e) {
    r.message = std::string("flash::solve failed: ") + e.what();
    return r;
  }

  r.vapor          = f.vapor;
  r.liquid         = f.liquid;
  r.vapor_fraction = f.vapor_fraction;
  r.single_phase   = f.single_phase;
  r.converged      = f.converged;

  const double h2o_in = mol_s_of(feed, Species::H2O);
  r.water_removal_fraction =
      (h2o_in > 0.0) ? mol_s_of(r.liquid, Species::H2O) / h2o_in : 0.0;

  r.ok = true;
  r.message = f.converged ? "ok" : "flash did not converge; result is the last iterate";
  return r;
}

}  // namespace front_end
