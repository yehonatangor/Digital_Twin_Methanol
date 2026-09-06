// Catalyst replacement bookkeeping
//
// The price and interval are flagged placeholders, not sourced values, so this
// file tests the ARITHMETIC and the guard behaviour rather than asserting that
// any particular cost is correct

#include "economics/catalyst_replacement.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== catalyst replacement ===\n\n");

  std::printf("[1] Placeholders are declared unsourced\n");
  checkTrue("price is flagged unsourced", !economics::kCatalystPriceSourced);
  checkTrue("interval is flagged unsourced", !economics::kCatalystReplacementIntervalSourced);

  std::printf("\n[2] Full cycles only, no partial-cycle proration\n");
  // 20 years at a 3-year life is 6 full cycles, not 6.67
  const auto r = economics::catalyst_replacement_cost(44500.0, 20.0, 3.0, 20);
  check("replacements", r.n_replacements, 6.0, 1e-12);
  check("one fresh charge, USD", r.fresh_charge_cost_USD, 44500.0 * 20.0, 1e-6);
  check("total, USD", r.total_replacement_cost_USD, 44500.0 * 20.0 * 6.0, 1e-6);
  check("annualized, USD/y", r.annualized_replacement_cost_USD,
        44500.0 * 20.0 * 6.0 / 20.0, 1e-6);

  std::printf("\n[3] An exact divisor gives exactly that many cycles\n");
  check("20 years at 4-year life", economics::catalyst_replacement_cost(1000.0, 1.0, 4.0, 20)
                                        .n_replacements, 5.0, 1e-12);

  std::printf("\n[4] Guards return a zeroed result rather than throwing\n");
  checkTrue("zero mass",     economics::catalyst_replacement_cost(0.0, 20.0, 3.0, 20).n_replacements == 0);
  checkTrue("zero price",    economics::catalyst_replacement_cost(100.0, 0.0, 3.0, 20).n_replacements == 0);
  checkTrue("zero lifetime", economics::catalyst_replacement_cost(100.0, 20.0, 0.0, 20).n_replacements == 0);
  checkTrue("zero horizon",  economics::catalyst_replacement_cost(100.0, 20.0, 3.0, 0).n_replacements == 0);

  return report("catalyst replacement");
}
