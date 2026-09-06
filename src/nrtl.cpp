#include "nrtl.hpp"
#include <cmath>
#include <unordered_map>

namespace nrtl {

  // NRTL binaries from the ChemSep NRTL databank (Kooijman & Taylor, LGPL)
  // Stored as tau_ij = a_ij + b_ij/T with a symmetric alpha
  // Methanol/water is the only pair the databank holds for this species set 
  // Every other pair falls through to tau = 0, G = 1, an ideal-solution contribution
  namespace {

    struct Entry {
      Species i, j; // parameters stored in this orientation
      BinaryParams p;
    };

    const Entry kBinaries[] = {
      { Species::CH3OH, Species::H2O,
        { 0.00, -95.13209282738782, 0.00, 398.95345259688855, 0.2999 } },
    };

    const BinaryParams kIdeal{};

    // `forward` reports whether the caller's (i,j) matches the stored order
    const Entry* findEntry(Species i, Species j, bool& forward) {
      for (const Entry& e : kBinaries) {
        if (e.i == i && e.j == j) { forward = true;  return &e; }
        if (e.i == j && e.j == i) { forward = false; return &e; }
      }
      forward = true;
      return nullptr;
    }

  }

  bool has_binary_params(Species i, Species j) {
    if (i == j) return true; // self-pair is ideal by definition, not missing
    bool fwd = false;
    return findEntry(i, j, fwd) != nullptr;
  }

  const BinaryParams& lookupParams(Species i, Species j) {
    if (i == j) {
      static const BinaryParams zero{};
      return zero;
    }
    bool fwd = false;
    const Entry* e = findEntry(i, j, fwd);
    return e ? e->p : kIdeal;
  }

  double tau(Species i, Species j, double T) {
    if (i == j) return 0.00;
    if (!(T > 0.00)) return 0.00;

    bool fwd = false;
    const Entry* e = findEntry(i, j, fwd);
    if (!e) return 0.00; // unparameterised pair -> ideal contribution

    return fwd ? (e->p.a_ij + e->p.b_ij / T)
               : (e->p.a_ji + e->p.b_ji / T);
  }

  double G(Species i, Species j, double T) {
    if (i == j) return 1.00;
    return std::exp(-lookupParams(i, j).alpha * tau(i, j, T));
  }

  int unparameterised_pairs(const Stream& liquidStream) {
    int missing = 0;
    for (auto a = liquidStream.molar_flow.begin(); a != liquidStream.molar_flow.end(); ++a) {
      if (a->second <= 0.00) continue;
      auto b = a;
      for (++b; b != liquidStream.molar_flow.end(); ++b) {
        if (b->second <= 0.00) continue;
        if (!has_binary_params(a->first, b->first)) ++missing;
      }
    }
    return missing;
  }

  std::unordered_map<Species, double> activityCoefficients(const Stream& liquidStream, double T){
    std::unordered_map<Species, double> gamma;

    for (const auto& [i, ni] : liquidStream.molar_flow) {
      double numer1 = 0.00, denom1 = 0.00;
      for (const auto& [k, nk] : liquidStream.molar_flow) {
        double xk = liquidStream.moleFraction(k);
        numer1 += xk * tau(k, i, T) * G(k, i, T);
        denom1 += xk * G(k, i, T);
      }

      double term1 = numer1 / denom1;
      double term2 = 0.00;

      for (const auto& [j, nj] : liquidStream.molar_flow) {
        double xj = liquidStream.moleFraction(j);

        double denomJ = 0.00;
        for (const auto& [k, nk] : liquidStream.molar_flow) {
          double xk = liquidStream.moleFraction(k);
          denomJ += xk * G(k, j, T);
        }

        double numerJ = 0.00;
        for (const auto& [m, nm] : liquidStream.molar_flow) {
          double xm = liquidStream.moleFraction(m);
          numerJ += xm * tau(m, j, T) * G(m, j, T);
        }

        term2 += (xj * G(i, j, T) / denomJ) * (tau(i, j, T) - numerJ / denomJ);
      }

      gamma[i] = std::exp(term1 + term2);
    }

    return gamma;
  }
}

