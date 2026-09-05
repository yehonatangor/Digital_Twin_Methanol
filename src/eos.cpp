#include "eos.hpp"
#include "units.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>
#include <vector>

namespace eos {

constexpr double SQRT2 = 1.4142135623730950;
constexpr double PI = 3.1415926535897932;

namespace {

// Peng-Robinson kappa. The 1976 form is valid to omega ~ 0.49; methanol
// (omega = 0.566) needs the 1978 branch.
constexpr double kOmegaSwitch = 0.49;

double kappaFromOmega(double omega){
  if (omega > kOmegaSwitch) {
    // Robinson & Peng (1978), GPA RR-28.
    return 0.379642 + omega * (1.48503 + omega * (-0.164423 + 0.016666 * omega));
  }
  // Peng & Robinson (1976), Eq. (18).
  return 0.37464 + 1.54226 * omega - 0.26992 * omega * omega;
}

// Depressed-cubic solver, t^3 + p t + q = 0 after the shift.
//
// The trigonometric branch is guarded against the degenerate case p = q = 0,
// which a cubic with a triple root produces. There r = sqrt(-p^3/27) is -0,
// -q/(2r) is 0/0 = NaN, and std::clamp does NOT sanitise NaN: both of its
// comparisons are false, so the NaN is returned unchanged. acos then yields
// NaN and cos raises FE_INVALID, which traps as SIGFPE in an unoptimised
// build. Comparisons below are written so NaN takes the guarded branch.
std::vector<double> solveCubic(double c2, double c1, double c0){
  const double p = c1 - c2 * c2 / 3.00;
  const double q = (2.00 * c2 * c2 * c2) / 27.00 - (c2 * c1) / 3.00 + c0;
  const double shift = c2 / 3.00;

  const double disc = (q * q) / 4.00 + (p * p * p) / 27.00;

  std::vector<double> roots;
  if (disc > 0.00) {
    const double sqrtDisc = std::sqrt(disc);
    const double u = std::cbrt(-q / 2.00 + sqrtDisc);
    const double v = std::cbrt(-q / 2.00 - sqrtDisc);
    roots.push_back(u + v - shift);
  }
  else {
    const double r = std::sqrt(std::max(0.00, -p * p * p / 27.00));
    double phi = 0.00;
    if (r > 0.00) {
      double t = -q / (2.00 * r);
      if (!(t >= -1.00)) t = -1.00;        // also catches NaN
      else if (!(t <= 1.00)) t = 1.00;
      phi = std::acos(t);
    }
    const double m = 2.00 * std::sqrt(std::max(0.00, -p / 3.00));
    for (int k = 0; k < 3; ++k){
      roots.push_back(m * std::cos((phi + 2.00 * PI * k) / 3.00) - shift);
      }
  }
  // A non-finite root is not a root. Dropping it here lets the caller's
  // "no physical root" path report the failure instead of propagating NaN.
  roots.erase(std::remove_if(roots.begin(), roots.end(),
                             [](double z){ return !std::isfinite(z); }),
              roots.end());
  return roots;
}
}

double kij(Species i, Species j){
  if (i == j) return 0.00;

  auto a = static_cast<int>(i);
  auto b = static_cast<int>(j);
  if (a > b) std::swap(a, b);
  const auto lo = static_cast<Species>(a);
  const auto hi = static_cast<Species>(b);

  // ChemSep PR binary interaction databank (Kooijman & Taylor, LGPL). These
  // are all 17 pairs it holds for this species set; anything else returns 0,
  // the van der Waals one-fluid default.
  struct Entry { Species lo, hi; double k; };
  static constexpr Entry table[] = {
    {Species::CO2, Species::H2, -0.16220},
    {Species::CO2, Species::H2O, 0.09520},
    {Species::CO2, Species::CH3OH, 0.05830},
    {Species::CO2, Species::CH4, 0.09780},
    {Species::CO2, Species::N2, -0.01220},
    {Species::H2, Species::CO, 0.09190},
    {Species::H2, Species::CH4, -0.00440},
    {Species::H2, Species::N2, 0.07110},
    {Species::CO, Species::CH4, 0.03000},
    {Species::CO, Species::N2, 0.03000},
    {Species::H2O, Species::CH3OH, -0.07780},
    {Species::CH3OH, Species::N2, -0.21410},
    {Species::CH4, Species::N2, 0.02890},
    {Species::CH4, Species::Ar, 0.01520},
    {Species::N2, Species::Ar, -0.00040},
    {Species::N2, Species::O2, -0.01590},
    {Species::Ar, Species::O2, 0.00890},
  };

  for (const Entry& e : table)
    if (e.lo == lo && e.hi == hi) return e.k;

  return 0.00;
}

PureParams pureComponentParams(Species sp, double T){
  const Properties& props = properties(sp);
  double kappa = kappaFromOmega(props.omega);
  double sqrtTr = std::sqrt(T / props.Tc);
  double alpha = (1.00 + kappa * (1.00 - sqrtTr));
  alpha *= alpha;

  double a = 0.45724 * units::R * units::R * props.Tc * props.Tc / props.Pc * alpha;
  double b = 0.07780 * units::R * props.Tc / props.Pc;
  return PureParams{a, b};
}

MixtureParams mixtureParams(const Stream& stream, double T) {
  MixtureParams mix;
  std::unordered_map<Species, double> x;

  for (const auto& [sp, n] : stream.molar_flow) {
    mix.pure[sp] = pureComponentParams(sp, T);
    x[sp] = stream.moleFraction(sp);
  }

  for (const auto& [sp, xi] : x) {
    mix.b_mix += xi * mix.pure.at(sp).b;
  }

  for (const auto& [spi, xi] : x) {
    for (const auto& [spj, xj] : x) {
      double aij = std::sqrt(mix.pure.at(spi).a * mix.pure.at(spj).a) * (1.00 - kij(spi, spj));
      mix.a_mix += xi * xj * aij;
    }
  }
  return mix;
}

double compressibilityFactor(const MixtureParams& mix, double T, double P, RootSelect which) {
    double A = mix.a_mix * P / (units::R * units::R * T * T);
    double B = mix.b_mix * P / (units::R * T);

    double c2 = -(1.00 - B);
    double c1 = A - 3.00 * B * B - 2.00 * B;
    double c0 = -(A * B - B * B - B * B * B);

    std::vector<double> roots = solveCubic(c2, c1, c0);
    std::vector<double> physical;

    for (double z : roots) {
        if (z > B) physical.push_back(z);
    }
    if (physical.empty()) {
        throw std::runtime_error("PR EOS: no physical root found for given T, P");
    }

    return (which == RootSelect::Vapor) 
    ? *std::max_element(physical.begin(), physical.end()) 
    : *std::min_element(physical.begin(), physical.end());
}

double molarVolume(double Z, double T, double P) {
    return Z * units::R * T / P;
}

std::unordered_map<Species, double> fugacityCoefficients(
    const MixtureParams& mix, const Stream& stream, double T, double P, double Z) {

    double A = mix.a_mix * P / (units::R * units::R * T * T);
    double B = mix.b_mix * P / (units::R * T);

    std::unordered_map<Species, double> phi;

    for (const auto& [spi, pi] : mix.pure) {
        double sumTerm = 0.00;
        for (const auto& [spj, pj] : mix.pure) {
            double xj  = stream.moleFraction(spj);
            double aij = std::sqrt(pi.a * pj.a) * (1.00 - kij(spi, spj));
            sumTerm += xj * aij;
        }

        double bRatio = pi.b / mix.b_mix;

        double lnPhi = bRatio * (Z - 1.00) - std::log(Z - B) - (A / (2.00 * SQRT2 * B))
                       * (2.00 * sumTerm / mix.a_mix - bRatio)
                       * std::log((Z + (1.00 + SQRT2) * B) / (Z - (SQRT2 - 1.00) * B));

        phi[spi] = std::exp(lnPhi);
    }
    return phi;
}
}
