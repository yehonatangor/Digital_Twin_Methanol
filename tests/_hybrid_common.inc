// Shared fixture for the hybrid-plant tests: a short two-day price series and
// a storage state, so each test states only what it is actually asserting.
#pragma once
#include <vector>
#include "dispatch/storage.hpp"

namespace {

[[maybe_unused]] std::vector<double> demoPrices() {
  std::vector<double> p;
  for (int h = 0; h < 48; ++h) p.push_back((h % 24) < 8 ? 12.0 : 55.0);
  return p;
}

[[maybe_unused]] dispatch::StorageState demoStorage() {
  dispatch::StorageState s;
  s.M_H2_kg = 1000.0;
  return s;
}

}  // namespace
