// Multi-stage intercooled compression

#include "front_end/compressor_train.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== compressor train ===\n\n");

  std::printf("[1] A single stage reproduces the standalone compressor\n");
  front_end::CompressorTrainConfig one;
  one.n_stages = 1;
  const auto t1 = front_end::run_compressor_train(Species::H2, 1900.0 / 3600.0, 30.0, 60.0, one);
  const auto s1 = front_end::run_compressor(
      {1900.0 / 3600.0, 30.0, 60.0}, front_end::CompressorConfig{});
  checkTrue("both ok", t1.ok && s1.ok);
  check("same shaft power", t1.total_P_comp_MW, s1.P_comp_MW, 1e-9);
  check("one stage recorded", static_cast<double>(t1.stages.size()), 1.0, 0.0);
  check("overall ratio", t1.overall_beta, 2.0, 1e-9);

  std::printf("\n[2] Intercooling makes multi-stage cheaper than one big stage\n");
  front_end::CompressorTrainConfig four;
  four.n_stages = 4;
  const auto t4 = front_end::run_compressor_train(Species::CO2, 24.44, 1.0, 78.0, four);
  const auto t1big = front_end::run_compressor_train(Species::CO2, 24.44, 1.0, 78.0, one);
  checkTrue("both ok", t4.ok && t1big.ok);
  check("four stages recorded", static_cast<double>(t4.stages.size()), 4.0, 0.0);
  check("overall ratio is 78", t4.overall_beta, 78.0, 1e-6);
  checkTrue("four intercooled stages use less power than one",
            t4.total_P_comp_MW < t1big.total_P_comp_MW);

  std::printf("\n[3] Stages are equal ratio, so each does the same work\n");
  const double per_stage_beta = std::pow(78.0, 0.25);
  for (const auto& st : t4.stages) check("stage ratio", st.beta, per_stage_beta, 1e-6);
  double sum = 0.0;
  for (const auto& st : t4.stages) sum += st.P_comp_MW;
  check("total is the sum of the stages", t4.total_P_comp_MW, sum, 1e-9);

  std::printf("\n[4] Cooling duty is tracked\n");
  checkTrue("cooling is positive", t4.total_P_cooling_MW > 0.0);

  std::printf("\n[5] Mixture stage uses a mole-weighted cp and k\n");
  Stream mix;
  mix.temperature = 298.15; mix.pressure = 30e5;
  mix.molar_flow[Species::H2]  = 90.0;
  mix.molar_flow[Species::CO2] = 10.0;
  const auto m = front_end::run_compressor_stage_mixture(mix, 30.0, 78.0);
  checkTrue("ok", m.ok);
  checkTrue("power is positive", m.P_comp_MW > 0.0);
  // Adding heavy CO2 to hydrogen must lower the mass-basis cp
  Stream pure;
  pure.temperature = 298.15; pure.pressure = 30e5;
  pure.molar_flow[Species::H2] = 100.0;
  const auto p = front_end::run_compressor_stage_mixture(pure, 30.0, 78.0);
  checkTrue("the CO2-laden mixture has a lower mass-basis cp",
            m.cp_J_per_kgK < p.cp_J_per_kgK);

  std::printf("\n[6] Guards\n");
  checkTrue("zero stages is rejected", [&]{
    front_end::CompressorTrainConfig bad; bad.n_stages = 0;
    return !front_end::run_compressor_train(Species::H2, 1.0, 30.0, 78.0, bad).ok; }());

  return report("compressor train");
}
