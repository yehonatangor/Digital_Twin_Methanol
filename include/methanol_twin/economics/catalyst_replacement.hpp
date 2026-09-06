#pragma once
// Synthesis catalyst charge cost on a replacement cycle

//   n_replacements = floor(n_project_years / lifetime_years)   [full cycles]
//   cost per replacement = catalyst_mass_kg * price_USD_per_kg
// Annualised by simple division, not NPV-discounted

// TWO UNSOURCED NUMBERS, both flagged and caller-overridable:
//   price 20 USD/kg          -- kCatalystPriceSourced = false, no paper in
//                               this library publishes a Cu/ZnO/Al2O3 price
//   interval 3 years         -- kCatalystReplacementIntervalSourced = false,
//                               typical industrial charge life. This is NOT
//                               degradation/activity_decay.hpp's ~1600 h
//                               fitted range; those are different quantities

// See docs/21-plant-economics.md

namespace economics {

// PLACEHOLDER, not sourced from any paper in this project's library. 
// See header. Caller-overridable; not a silent default meant to look like real data
inline constexpr double kCatalystPricePlaceholder_USD_per_kg = 20.0;
inline constexpr bool kCatalystPriceSourced = false;

// Flagged ENGINEERING ASSUMPTION (typical industrial methanol-catalyst charge life), NOT derived from Fichtl's fitted decay law. 
// See header
inline constexpr double kCatalystReplacementIntervalPlaceholder_years = 3.0;
inline constexpr bool kCatalystReplacementIntervalSourced = false;

// Same discrete-replacement-cycle bookkeeping convention as
// capex_opex.hpp's we_replacement_cost() (n_replacements = full cycles
// completed within the project horizon, no partial-cycle proration), but a
// DIFFERENT cost basis: catalyst_mass_kg * price_USD_per_kg per replacement,
// not a fraction of an unrelated "initial investment" figure. 
// Returns a zeroed result (silently, matching we_replacement_cost()'s own guard convention) if any input is non-positive
struct CatalystReplacementResult {
  int    n_replacements = 0;
  double fresh_charge_cost_USD = 0.0;             // catalyst_mass_kg * price_USD_per_kg, one charge
  double total_replacement_cost_USD = 0.0;
  double annualized_replacement_cost_USD = 0.0;    // simple /n_project_years, not NPV-discounted
};
CatalystReplacementResult catalyst_replacement_cost(double catalyst_mass_kg,
                                                      double price_USD_per_kg,
                                                      double lifetime_years,
                                                      int n_project_years);

}
