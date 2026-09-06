#pragma once

namespace trm_kinetics {

// Reaction enthalpies at 298.15 K, J/mol. Lim et al. (2022) Table 2
// Shi et al. (2020) Sec. 2.1 for the SRM cross-check
constexpr double dH_SRM_Lim2022 = 206300.00;
constexpr double dH_SRM_Shi2020 = 206800.00;
constexpr double dH_DRM = 247300.00;
constexpr double dH_POM = -35600.00;
constexpr double dH_SRM = dH_SRM_Lim2022;

struct FeedMoles {
    double CH4 = 0.00, H2O = 0.00, CO2 = 0.00, O2 = 0.00, CO = 0.00, H2 = 0.00;
};

struct ExtentResult {
    double xi_POM = 0.00;
    double xi_SRM = 0.00;
    double xi_DRM = 0.00;
    FeedMoles outlet;
    bool converged = false;
};

double Keq_SRM(double T);
double Keq_DRM(double T);

ExtentResult solveEquilibrium(const FeedMoles& feed, double T, double P);

double conversionCH4(const FeedMoles& in, const FeedMoles& out);
double conversionCO2(const FeedMoles& in, const FeedMoles& out);

}
