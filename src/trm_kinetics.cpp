#include "trm_kinetics.hpp"
#include "thermo.hpp"
#include <cmath>
#include <algorithm>

namespace trm_kinetics {

namespace {

// CH4 + 0.5 O2 -> CO + 2 H2, taken as complete. Extent is the limiting reagent: min(CH4, 2*O2)
// Since one extent consumes half a mol of O2
FeedMoles combustPOM(const FeedMoles& f0, double& xi_POM_out) {
    const double xi = std::min(f0.CH4, 2.0 * f0.O2);
    xi_POM_out = xi;

    FeedMoles out = f0;
    out.CH4 -= xi;
    out.O2 -= 0.5 * xi;
    out.CO += xi;
    out.H2 += 2.0 * xi;
    return out;
}

// Applies both extents at once
// SRM: CH4 + H2O -> CO + 3H2 (xi1)
// DRM: CH4 + CO2 -> 2CO + 2H2 (xi2)
FeedMoles applyExtents(const FeedMoles& f0, double xi1, double xi2) {
    FeedMoles out = f0;
    out.CH4 -= xi1 + xi2;
    out.H2O -= xi1;
    out.CO2 -= xi2;
    out.CO += xi1 + 2.0 * xi2;
    out.H2 += 3.0 * xi1 + 2.0 * xi2;
    return out;
}

double totalMoles(const FeedMoles& f) {
    return f.CH4 + f.H2O + f.CO2 + f.O2 + f.CO + f.H2;
}

constexpr double FLOOR = 1e-12; // guards log(0) and division in residuals()

// Log-form equilibrium residuals, driven to zero by the Newton solve:
// ln Keq_SRM(T) - ln Kp_SRM,   ln Keq_DRM(T) - ln Kp_DRM
// Log form keeps the residuals scaled; Kp spans many orders of magnitude
void residuals(const FeedMoles& feed, double T, double P,
               double xi1, double xi2, double out[2]) {
    const FeedMoles n = applyExtents(feed, xi1, xi2);
    const double Ftot = std::max(totalMoles(n), FLOOR);
    auto pp = [&](double ni) { return P * std::max(ni, FLOOR) / Ftot; };

    const double P_CH4 = pp(n.CH4), P_H2O = pp(n.H2O), P_CO2 = pp(n.CO2);
    const double P_CO  = pp(n.CO), P_H2 = pp(n.H2);

    const double lnKp_SRM = std::log(P_CO) + 3.0*std::log(P_H2)
                          - std::log(P_CH4) - std::log(P_H2O);
    const double lnKp_DRM = 2.0*std::log(P_CO) + 2.0*std::log(P_H2)
                          - std::log(P_CH4) - std::log(P_CO2);

    out[0] = std::log(Keq_SRM(T)) - lnKp_SRM;
    out[1] = std::log(Keq_DRM(T)) - lnKp_DRM;
}

// Damping factor in (0,1] that keeps CH4, H2O and CO2 non-negative after the Newton step
// The 0.9 margin keeps it strictly inside the boundary
double maxFeasibleStep(const FeedMoles& feed, double xi1, double xi2,
                       double d1, double d2) {
    double alpha = 1.0;
    const FeedMoles n = applyExtents(feed, xi1, xi2);
    auto shrinkIfNeeded = [&](double current, double delta) {
        if (delta < 0.0) {
            const double limit = -current / delta;
            if (limit > 0.0) alpha = std::min(alpha, 0.9 * limit);
        }
    };
    shrinkIfNeeded(n.CH4, -(d1 + d2));
    shrinkIfNeeded(n.H2O, -d1);
    shrinkIfNeeded(n.CO2, -d2);
    return alpha;
}

}

// Keq from thermo::Keq (Gibbs energy of reaction)
double Keq_SRM(double T) {
    return thermo::Keq({{Species::CH4,-1.0},{Species::H2O,-1.0},
                        {Species::CO,1.0},{Species::H2,3.0}}, T);
}

double Keq_DRM(double T) {
    return thermo::Keq({{Species::CH4,-1.0},{Species::CO2,-1.0},
                        {Species::CO,2.0},{Species::H2,2.0}}, T);
}

// POM is applied first as a complete reaction
// SRM and DRM extents are solved together by Newton-Raphson with a finite-difference Jacobian
ExtentResult solveEquilibrium(const FeedMoles& feed, double T, double P) {
    ExtentResult result;
    const FeedMoles postCombustion = combustPOM(feed, result.xi_POM);

    double xi1 = 0.01 * postCombustion.H2O;
    double xi2 = 0.01 * postCombustion.CO2;

    constexpr int MAX_ITER = 200;
    constexpr double TOL = 1e-10;
    constexpr double H = 1e-7;

    for (int iter = 0; iter < MAX_ITER; ++iter) {
        double f[2];
        residuals(postCombustion, T, P, xi1, xi2, f);
        if (std::sqrt(f[0]*f[0] + f[1]*f[1]) < TOL) {
            result.converged = true;
            break;
        }

        double f1p[2], f2p[2];
        residuals(postCombustion, T, P, xi1 + H, xi2, f1p);
        residuals(postCombustion, T, P, xi1, xi2 + H, f2p);

        const double J[2][2] = {
            { (f1p[0]-f[0])/H, (f2p[0]-f[0])/H },
            { (f1p[1]-f[1])/H, (f2p[1]-f[1])/H }
        };

        // J * (d1,d2)^T = -f, by Cramer's rule
        const double det = J[0][0]*J[1][1] - J[0][1]*J[1][0];
        if (std::fabs(det) < 1e-30) break;

        const double d1 = (-f[0]*J[1][1] + f[1]*J[0][1]) / det;
        const double d2 = (-J[0][0]*f[1] + J[1][0]*f[0]) / det;

        const double alpha = maxFeasibleStep(postCombustion, xi1, xi2, d1, d2);
        xi1 += alpha * d1;
        xi2 += alpha * d2;
    }

    result.xi_SRM = xi1;
    result.xi_DRM = xi2;
    result.outlet = applyExtents(postCombustion, xi1, xi2);
    return result;
}

double conversionCH4(const FeedMoles& in, const FeedMoles& out) {
    return (in.CH4 <= 0.0) ? 0.0 : (in.CH4 - out.CH4) / in.CH4;
}

double conversionCO2(const FeedMoles& in, const FeedMoles& out) {
    return (in.CO2 <= 0.0) ? 0.0 : (in.CO2 - out.CO2) / in.CO2;
}

}

