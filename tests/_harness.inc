// Shared PASS/FAIL harness. Included by each test rather than compiled
// separately, so every test binary stays self-contained.
#pragma once
#include <cmath>
#include <cstdio>

#include "species.hpp"

namespace {

int failures = 0;

[[maybe_unused]] void check(const char* what, double got, double want, double abs_tol) {
  const bool ok = std::fabs(got - want) <= abs_tol;
  if (!ok) ++failures;
  std::printf("[%s] %-56s got %12.5g  want %12.5g  (+/-%.3g)\n",
              ok ? "PASS" : "FAIL", what, got, want, abs_tol);
}

[[maybe_unused]] void checkRel(const char* what, double got, double want, double rel_tol) {
  const double denom = std::fabs(want) > 0.0 ? std::fabs(want) : 1.0;
  const bool ok = std::fabs(got - want) / denom <= rel_tol;
  if (!ok) ++failures;
  std::printf("[%s] %-56s got %12.5g  want %12.5g  (rel %.3g)\n",
              ok ? "PASS" : "FAIL", what, got, want, rel_tol);
}

[[maybe_unused]] void checkTrue(const char* what, bool cond) {
  if (!cond) ++failures;
  std::printf("[%s] %s\n", cond ? "PASS" : "FAIL", what);
}

// Reads a species flow from a possibly-const Stream. unordered_map's
// operator[] is non-const, so tests cannot use it on a returned result.
template <class StreamT>
[[maybe_unused]] double flowOf(const StreamT& s, Species sp) {
  const auto it = s.molar_flow.find(sp);
  return it == s.molar_flow.end() ? 0.0 : it->second;
}

[[maybe_unused]] int report(const char* title) {
  std::printf("\n%s: %s -- %d failure(s)\n", title, failures ? "FAILED" : "ALL PASSED", failures);
  return failures ? 1 : 0;
}

}  // namespace
