#include "thermo.hpp"
#include "units.hpp"
#include <cmath>
#include <cstdio>
#include <stdexcept>

namespace thermo {

namespace {

constexpr double T_REF = 298.15;

struct Formation { double dHf298; double S298; };

// {dHf at 298.15 K [J/mol], S at 298.15 K [J/(mol K)]}, ideal gas.
// NIST-JANAF / CRC Handbook.
constexpr Formation form[static_cast<size_t>(Species::Count)] = {
    {-393.51e3, 213.78}, // CO2
    {   0.00,   130.68}, // H2
    {-110.53e3, 197.66}, // CO
    {-241.83e3, 188.84}, // H2O (g)
    {-201.00e3, 239.90}, // CH3OH (g)
    { -74.60e3, 186.25}, // CH4
    {   0.00,   191.60}, // N2
    {   0.00,   154.85}, // Ar
    {   0.00,   205.15}, // O2
};

constexpr double LN2 = 0.6931471805599453;

double lnSinh(double u) {
    if (u > 20.0) return u - LN2 + std::log1p(-std::exp(-2.0 * u));
    return std::log(std::sinh(u));
}
double lnCosh(double u) {
    if (u > 20.0) return u - LN2 + std::log1p(std::exp(-2.0 * u));
    return std::log(std::cosh(u));
}

constexpr double KMOL = 1000.0;   // DIPPR Cp is per kmol; results are per mol

double sinhTerm(double c, double T) {
    if (c == 0.0) return 0.0;
    const double u = c / T;
    const double r = u / std::sinh(u);
    return r * r;
}

double coshTerm(double e, double T) {
    if (e == 0.0) return 0.0;
    const double v = e / T;
    const double r = v / std::cosh(v);
    return r * r;
}

double intSinh(double coef, double c, double T) {
    if (c == 0.0) return 0.0;
    return coef * c / std::tanh(c / T);
}

double intSinhOverT(double coef, double c, double T) {
    if (c == 0.0) return 0.0;
    const double u = c / T;
    return coef * (u / std::tanh(u) - lnSinh(u));
}

double intCosh(double coef, double e, double T) {
    if (e == 0.0) return 0.0;
    return -coef * e * std::tanh(e / T);
}

double intCoshOverT(double coef, double e, double T) {
    if (e == 0.0) return 0.0;
    const double v = e / T;
    return -coef * (v * std::tanh(v) - lnCosh(v));
}

double cpRaw(const CpParams& p, double T) {
    if (p.eq == 1) {
        return p.a + T*(p.b + T*(p.c + T*(p.d + T*p.e)));
    }
    if (p.eq == 3) {
        return p.a + p.b * sinhTerm(p.c / 2.0, T) + p.d * sinhTerm(p.e / 2.0, T) + p.f * sinhTerm(p.g / 2.0, T);
    }
    return p.a + p.b * sinhTerm(p.c, T) + p.d * coshTerm(p.e, T);
}

double intCp(const CpParams& p, double T) {
    if (p.eq == 1) {
        return T*(p.a + T*(p.b/2.0 + T*(p.c/3.0 + T*(p.d/4.0 + T*p.e/5.0))));
    }
    if (p.eq == 3) {
        return p.a * T + intSinh(p.b, p.c / 2.0, T) + intSinh(p.d, p.e / 2.0, T) + intSinh(p.f, p.g / 2.0, T);
    }
    return p.a * T + intSinh(p.b, p.c, T) + intCosh(p.d, p.e, T);
}

double intCpOverT(const CpParams& p, double T) {
    if (p.eq == 1) {
        return p.a*std::log(T) + T*(p.b + T*(p.c/2.0 + T*(p.d/3.0 + T*p.e/4.0)));
    }
    if (p.eq == 3) {
        return p.a * std::log(T) + intSinhOverT(p.b, p.c / 2.0, T) + intSinhOverT(p.d, p.e / 2.0, T) + intSinhOverT(p.f, p.g / 2.0, T);
    }
    return p.a * std::log(T) + intSinhOverT(p.b, p.c, T) + intCoshOverT(p.d, p.e, T);
}

const Formation& formation(Species sp) {
    auto i = static_cast<size_t>(sp);
    if (i >= static_cast<size_t>(Species::Count))
        throw std::out_of_range("thermo: invalid species");
    return form[i];
}
}

double cp(Species sp, double T) {
    return cpRaw(cpParams(sp), T) / KMOL;
}

double enthalpy(Species sp, double T) {
    const CpParams& p = cpParams(sp);
    return formation(sp).dHf298 + (intCp(p, T) - intCp(p, T_REF)) / KMOL;
}

double entropy(Species sp, double T) {
    const CpParams& p = cpParams(sp);
    return formation(sp).S298 + (intCpOverT(p, T) - intCpOverT(p, T_REF)) / KMOL;
}

double deltaH(Reaction rxn, double T) {
    double s = 0.0;
    for (const Term& t : rxn) s += t.nu * enthalpy(t.sp, T);
    return s;
}

double deltaS(Reaction rxn, double T) {
    double s = 0.0;
    for (const Term& t : rxn) s += t.nu * entropy(t.sp, T);
    return s;
}

double deltaG(Reaction rxn, double T) {
    return deltaH(rxn, T) - T * deltaS(rxn, T);
}

double lnKeq(Reaction rxn, double T) {
    return -deltaG(rxn, T) / (units::R * T);
}

double Keq(Reaction rxn, double T) {
    return std::exp(lnKeq(rxn, T));
}

void selfTest() {
    const Reaction SRM = {{Species::CH4,-1},{Species::H2O,-1},{Species::CO,1},{Species::H2,3}};
    const Reaction DRM = {{Species::CH4,-1},{Species::CO2,-1},{Species::CO,2},{Species::H2,2}};
    const Reaction POM = {{Species::CH4,-1},{Species::O2,-0.5},{Species::CO,1},{Species::H2,2}};

    std::printf("thermo::selfTest -- dH at 298.15 K [kJ/mol]\n");
    std::printf("Reaction computed Lim(2022) Shi(2020)\n");
    std::printf("SRM %8.2f 206.3 206.8\n", deltaH(SRM, T_REF)/1000.0);
    std::printf("DRM %8.2f 247.3 247.3\n", deltaH(DRM, T_REF)/1000.0);
    std::printf("POM %8.2f -35.6 -35.6\n", deltaH(POM, T_REF)/1000.0);
}
}

