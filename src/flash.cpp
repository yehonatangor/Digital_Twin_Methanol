#include "flash.hpp"
#include "units.hpp"
#include "eos.hpp"
#include "nrtl.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>

namespace flash {

  namespace {

    constexpr double Regime_boundary_Pa = 10.0 * 1e5; // gamma-phi below, phi-phi above
    constexpr int Max_outer_iter = 100;
    constexpr double K_tol = 1e-10;
    constexpr double Trivial_K_tol = 1e-8;
    constexpr int Max_bisec_iter = 100;
    constexpr double Psi_tol = 1e-10;

    Stream makeStream(const std::unordered_map<Species, double>& flows, double T, double P, Phase phase){
      Stream s;
      s.molar_flow = flows;
      s.temperature = T;
      s.pressure = P;
      s.phase = phase;
      return s;
    }

    double wilsonK(Species sp, double T, double P) {
      const Properties& props = properties(sp);
      return (props.Pc / P) * std::exp(5.373 * (1.0 + props.omega) * (1.0 - props.Tc /T));
    }

    double rachfordRiceResidual(const std::unordered_map<Species, double>& z,
                                const std::unordered_map<Species, double>& K,
                                double psi) {
        double f = 0.00;
        for (const auto& [sp, zi] : z) {
          double Ki = K.at(sp);
          f += zi * (Ki - 1.00) / (1.00 + psi *(Ki - 1.00));
        }
        return f;              
      }
      
    double solveRachfordRice(const std::unordered_map<Species, double>& z,
                            const std::unordered_map<Species, double>& K) {
      if (rachfordRiceResidual(z, K, 0.00) <= 0.00) return 0.00;
      if (rachfordRiceResidual(z, K, 1.00) >= 0.00) return 1.00;

      double lo = 0.00, hi = 1.00;
      for (int i = 0.00; i < Max_bisec_iter; ++i) {
        double mid = 0.50 * (lo + hi);
        double fmid = rachfordRiceResidual(z, K, mid);
        if (std::fabs(fmid) < Psi_tol) return mid;
        if (fmid > 0.00) lo = mid; else hi = mid;
      }
      return 0.50 * (lo + hi);
    }

    std::unordered_map<Species, double> gammaPhiK(
    const std::unordered_map<Species, double>& x_flows, double T, double P) {

    Stream liquidGuess = makeStream(x_flows, T, P, Phase::Liquid);
    auto gamma = nrtl::activityCoefficients(liquidGuess, T);

    std::unordered_map<Species, double> K;
    for (const auto& [sp, xi] : x_flows) {
        double gamma_i = gamma.count(sp) ? gamma.at(sp) : 1.0;
        K[sp] = gamma_i * vaporPressure(sp, T) / P;
    }
    return K;
  }

  std::unordered_map<Species, double> phiPhiK(
    const std::unordered_map<Species, double>& x_flows,
    const std::unordered_map<Species, double>& y_flows,
    double T, double P) {

    Stream liquidGuess = makeStream(x_flows, T, P, Phase::Liquid);
    Stream vaporGuess  = makeStream(y_flows, T, P, Phase::Vapor);

    auto mixL = eos::mixtureParams(liquidGuess, T);
    auto mixV = eos::mixtureParams(vaporGuess, T);

    double Zl = eos::compressibilityFactor(mixL, T, P, eos::RootSelect::Liquid);
    double Zv = eos::compressibilityFactor(mixV, T, P, eos::RootSelect::Vapor);

    auto phiL = eos::fugacityCoefficients(mixL, liquidGuess, T, P, Zl);
    auto phiV = eos::fugacityCoefficients(mixV, vaporGuess, T, P, Zv);

    std::unordered_map<Species, double> K;
    for (const auto& [sp, xi] : x_flows) {
        double phiL_i = phiL.count(sp) ? phiL.at(sp) : 1.0;
        double phiV_i = phiV.count(sp) ? phiV.at(sp) : 1.0;
        K[sp] = phiL_i / phiV_i;
    }
    return K;
  }
  }

  Regime decideRegime(double P) {
    return (P > Regime_boundary_Pa) ? Regime::PhiPhi : Regime::GammaPhi;
}

FlashResult solve(const Stream& feed, double T, double P) {
    return solve(feed, T, P, decideRegime(P));
}

FlashResult solve(const Stream& feed, double T, double P, Regime regime) {
    double F = feed.totalMolarFlow();
    if (F <= 0.0) throw std::runtime_error("flash::solve: feed has zero total flow");

    std::unordered_map<Species, double> z;
    for (const auto& [sp, n] : feed.molar_flow) z[sp] = n / F;

    std::unordered_map<Species, double> K;
    for (const auto& [sp, zi] : z) K[sp] = wilsonK(sp, T, P);

    std::unordered_map<Species, double> x = z, y = z;
    double psi = 0.5;
    bool singlePhase = false;
    bool converged = false;

    for (int iter = 0; iter < Max_outer_iter; ++iter) {
        psi = solveRachfordRice(z, K);

        std::unordered_map<Species, double> x_new, y_new;
        for (const auto& [sp, zi] : z) {
            double Ki = K.at(sp);
            double xi = zi / (1.0 + psi * (Ki - 1.0));
            x_new[sp] = xi;
            y_new[sp] = Ki * xi;
        }
        x = x_new;
        y = y_new;

        auto K_new = (regime == Regime::GammaPhi) ? gammaPhiK(x, T, P) : phiPhiK(x, y, T, P);

        double maxLogK = 0.0;
        for (const auto& [sp, Ki_new] : K_new)
            maxLogK = std::max(maxLogK, std::fabs(std::log(Ki_new)));
        if (maxLogK < Trivial_K_tol) { K = K_new; singlePhase = true; break; }

        double maxDelta = 0.0;
        for (const auto& [sp, Ki_new] : K_new)
            maxDelta = std::max(maxDelta, std::fabs(std::log(Ki_new / K.at(sp))));
        K = K_new;
        if (maxDelta < K_tol) { converged = true; break; }
    }

    if (singlePhase) {
        double sumWilson = 0.0;
        for (const auto& [sp, zi] : z) sumWilson += zi * wilsonK(sp, T, P);
        psi = (sumWilson >= 1.0) ? 1.0 : 0.0;
        for (const auto& [sp, zi] : z) { x[sp] = zi; y[sp] = zi; }
        converged = true;
    }

    std::unordered_map<Species, double> vaporFlows, liquidFlows;

    for (const auto& [sp, zi] : z) {
        liquidFlows[sp] = x.at(sp) * (1.0 - psi) * F;
        vaporFlows[sp] = y.at(sp) * psi * F;
    }

    FlashResult result;
    result.vapor_fraction = psi;
    result.vapor  = makeStream(vaporFlows, T, P, Phase::Vapor);
    result.liquid = makeStream(liquidFlows, T, P, Phase::Liquid);
    result.K = K;
    result.regime_used = regime;
    result.converged = converged;
    result.single_phase = singlePhase;

    // phi-phi does not consult NRTL, so the count stays zero there
    if (regime == Regime::GammaPhi && !singlePhase) {
      result.ideal_binary_pairs = nrtl::unparameterised_pairs(result.liquid);
    }
    return result;
}
}
