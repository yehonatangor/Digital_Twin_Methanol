#include "sampling/fingerprint.hpp"

#include <cstdio>
#include <cstring>

#include "eos.hpp"
#include "lhhw.hpp"
#include "nrtl.hpp"
#include "reactor_core.hpp"
#include "species.hpp"
#include "thermo.hpp"
#include "trm_reactor.hpp"

namespace fingerprint {

namespace {

// FNV-1a 64-bit hash: A fast, dependency-free algorithm sufficient for detecting if physical constants have changed
constexpr std::uint64_t kFnvOffset = 1469598103934665603ULL;
constexpr std::uint64_t kFnvPrime  = 1099511628211ULL;

struct Hasher {
  std::uint64_t h = kFnvOffset;
  int n = 0;

  void byte(unsigned char b) {
    h ^= static_cast<std::uint64_t>(b);
    h *= kFnvPrime;
  }

  // Hash the EXACT bit pattern bit patterns are identical everywhere IEEE-754 doubles are
  void value(double d) {
    // Normalise the two zeros so -0.0 and +0.0 do not produce different fingerprints for numerically identical parameter sets
    if (d == 0.0) d = 0.0;
    unsigned char buf[sizeof(double)];
    std::memcpy(buf, &d, sizeof(double));
    for (unsigned char b : buf) byte(b);
    ++n;
  }

  void value(int i)  { value(static_cast<double>(i)); }
  void value(bool b) { value(b ? 1.0 : 0.0); }

  void text(const char* s) {
    if (!s) return;
    for (const char* p = s; *p; ++p) byte(static_cast<unsigned char>(*p));
  }

  // Domain separator between sections, so that moving a value from one table
  // to another changes the hash even if the multiset of numbers is unchanged
  void section(const char* name) { byte(0xFF); text(name); byte(0xFF); }
};

std::uint64_t hashThermo(Hasher& g) {
  const std::uint64_t before = g.h;
  g.section("thermo");
  for (int i = 0; i < static_cast<int>(Species::Count); ++i) {
    const Species sp = static_cast<Species>(i);
    const Properties& p = properties(sp);
    g.value(p.molar_mass); g.value(p.Tc); g.value(p.Pc); g.value(p.omega);

    const CpParams& c = cpParams(sp);
    g.value(c.eq);
    g.value(c.a); g.value(c.b); g.value(c.c);
    g.value(c.d); g.value(c.e); g.value(c.f); g.value(c.g);

    // Formation data is file-static in thermo.cpp, so it is captured through its observable consequence instead:
    // enthalpy and entropy at a fixed reference temperature 
    // Any change to dHf298 or S298 moves these
    g.value(thermo::enthalpy(sp, 298.15));
    g.value(thermo::entropy(sp, 298.15));

    // Vapour pressure, likewise captured via evaluation
    // 350 K is inside the useful range for the condensables and harmless for the rest
    const VaporPressureParams* vp = vaporPressureParams(sp);
    g.value(vp != nullptr);
    if (vp) { g.value(vp->A); g.value(vp->B); g.value(vp->C); g.value(vp->D); g.value(vp->E); }
  }
  return g.h ^ before;
}

std::uint64_t hashKinetics(Hasher& g) {
  const std::uint64_t before = g.h;
  g.section("kinetics");

  // LHHW parameters are file-static constants in lhhw.cpp
  // Capture them through rate and equilibrium evaluations at fixed probe conditions
  // Any change to A_K*/B_K* moves at least one of these
  for (double T : {473.15, 493.15, 523.15}) {
    g.value(lhhw::Keq1(T));
    g.value(lhhw::Keq2(T));
    g.value(lhhw::r_CH3OH(1.5, 41.0, 0.05, 0.05, T));
    g.value(lhhw::r_RWGS (1.5, 41.0, 0.05, 2.00, T));
  }

  // Xu & Froment / Trimm & Lam parameters
  const front_end::TrmKineticsParams kp = front_end::aboosadi_kinetics_params();
  auto rate = [&](const front_end::ArrheniusRate& r) { g.value(r.A); g.value(r.E_J_mol); };
  auto ads  = [&](const front_end::VantHoffAdsorption& v) { g.value(v.A_bar_inv); g.value(v.dH_J_mol); };
  rate(kp.k_srm_kmol_h); rate(kp.k_wgs_kmol_h); rate(kp.k_overall_kmol_h);
  ads(kp.K_CO); ads(kp.K_H2); ads(kp.K_CH4); ads(kp.K_H2O);
  rate(kp.k4a_mol_s); rate(kp.k4b_mol_s);
  ads(kp.K_CH4_combustion); ads(kp.K_O2_combustion);
  g.value(kp.KI_a); g.value(kp.KI_b); g.value(kp.KIII_a); g.value(kp.KIII_b);

  // Effectiveness factors change every reaction rate so they are here
  const front_end::EffectivenessFactors eta{};
  g.value(eta.eta_srm); g.value(eta.eta_overall);
  g.value(eta.eta_wgs); g.value(eta.eta_combustion);

  return g.h ^ before;
}

std::uint64_t hashTransport(Hasher& g) {
  const std::uint64_t before = g.h;
  g.section("transport");
  for (int i = 0; i < reactor::NS; ++i) {
    const reactor::Dippr102& c = reactor::kVaporViscosity[static_cast<std::size_t>(i)];
    g.value(c.A); g.value(c.B); g.value(c.C); g.value(c.D);
    g.value(c.T_min_K); g.value(c.T_max_K);
    g.value(c.sourced);
  }
  return g.h ^ before;
}

std::uint64_t hashPhase(Hasher& g) {
  const std::uint64_t before = g.h;
  g.section("phase");

  // Peng-Robinson binary interaction parameters, every unordered pair
  for (int i = 0; i < static_cast<int>(Species::Count); ++i)
    for (int j = i + 1; j < static_cast<int>(Species::Count); ++j)
      g.value(eos::kij(static_cast<Species>(i), static_cast<Species>(j)));

  // The PR alpha function itself this is what changed when the 1978 Robinson-Peng kappa branch was added for methanol
  for (int i = 0; i < static_cast<int>(Species::Count); ++i) {
    const eos::PureParams p = eos::pureComponentParams(static_cast<Species>(i), 400.0);
    g.value(p.a); g.value(p.b);
  }

  // NRTL binaries, captured through tau/G at a fixed temp so that both the parameters and the orientation convention are covered
  for (int i = 0; i < static_cast<int>(Species::Count); ++i)
    for (int j = 0; j < static_cast<int>(Species::Count); ++j) {
      const Species a = static_cast<Species>(i), b = static_cast<Species>(j);
      g.value(nrtl::has_binary_params(a, b));
      g.value(nrtl::tau(a, b, 350.0));
      g.value(nrtl::G(a, b, 350.0));
    }

  return g.h ^ before;
}

std::uint64_t hashNumerics(Hasher& g) {
  const std::uint64_t before = g.h;
  g.section("numerics");

  // Integrator accuracy settings CHANGE RESULTS, so they are part of the model identity
  // Altering the effective step size can move plant-scale methanol production significantly,
  // which requires the step size to be hashed to ensure dataset provenance
  const reactor::ReactorConfig rc{};
  g.value(rc.n_steps);
  g.value(rc.max_step_kg_cat);
  g.value(rc.max_clamp_rel);
  g.value(rc.use_real_gas_Z);
  g.value(rc.T_min_K); g.value(rc.T_max_K); g.value(rc.P_min_Pa);

  const front_end::TrmReactorConfig tc{};
  g.value(tc.n_steps);
  g.value(tc.max_step_kg_cat);
  g.value(tc.max_clamp_rel);
  g.value(tc.use_real_gas_Z);

  const reactor::ViscosityConfig vc{};
  g.value(vc.use_constant);
  g.value(vc.constant_mu_Pa_s);

  return g.h ^ before;
}

std::string toHex(std::uint64_t v) {
  char buf[17];
  std::snprintf(buf, sizeof(buf), "%016llx", static_cast<unsigned long long>(v));
  return std::string(buf);
}

}  // namespace

ModelFingerprint compute() {
  Hasher g;
  ModelFingerprint fp;
  fp.thermo_hash    = hashThermo(g);
  fp.kinetics_hash  = hashKinetics(g);
  fp.transport_hash = hashTransport(g);
  fp.phase_hash     = hashPhase(g);
  fp.numerics_hash  = hashNumerics(g);
  fp.hash     = g.h;
  fp.n_values = g.n;
  fp.hex      = toHex(g.h);
  return fp;
}

std::string hex_hash() { return compute().hex; }

std::string report() {
  const ModelFingerprint fp = compute();
  std::string s;
  char buf[256];
  std::snprintf(buf, sizeof(buf), "methanol_twin model fingerprint\n  combined  : %s  (%d constants)\n",
                fp.hex.c_str(), fp.n_values);
  s += buf;
  std::snprintf(buf, sizeof(buf), "  thermo    : %s\n", toHex(fp.thermo_hash).c_str());    s += buf;
  std::snprintf(buf, sizeof(buf), "  kinetics  : %s\n", toHex(fp.kinetics_hash).c_str());  s += buf;
  std::snprintf(buf, sizeof(buf), "  transport : %s\n", toHex(fp.transport_hash).c_str()); s += buf;
  std::snprintf(buf, sizeof(buf), "  phase     : %s\n", toHex(fp.phase_hash).c_str());     s += buf;
  std::snprintf(buf, sizeof(buf), "  numerics  : %s\n", toHex(fp.numerics_hash).c_str());  s += buf;
  return s;
}

}  // namespace fingerprint
