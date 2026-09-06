#include "catalyst_replacement.hpp"

#include <cmath>

namespace economics {

CatalystReplacementResult catalyst_replacement_cost(double catalyst_mass_kg,
                                                     double price_USD_per_kg,
                                                     double lifetime_years,
                                                     int n_project_years) {
  CatalystReplacementResult r;
  if (!(catalyst_mass_kg > 0.0) || !(price_USD_per_kg > 0.0) ||
      !(lifetime_years > 0.0) || n_project_years <= 0) {
    return r;
  }

  r.fresh_charge_cost_USD = catalyst_mass_kg * price_USD_per_kg;

  // Full cycles only, matching we_replacement_cost()'s convention
  r.n_replacements = static_cast<int>(std::floor(n_project_years / lifetime_years));
  r.total_replacement_cost_USD =
      r.fresh_charge_cost_USD * static_cast<double>(r.n_replacements);
  r.annualized_replacement_cost_USD =
      r.total_replacement_cost_USD / static_cast<double>(n_project_years);
  return r;
}

}  // namespace economics
