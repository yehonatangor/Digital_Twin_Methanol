// Catalyst activity coupled into the reactor
//
// The contract that matters: activity 1.0 must reproduce integrate_reactor()
// exactly, because integrate_aged_reactor() is a near-copy of it and any
// divergence between the two would be silent

#include "integration/aged_reactor.hpp"
#include "_harness.inc"

int main() {
  std::printf("=== aged reactor ===\n\n");

  reactor::ReactorConfig cfg;
  cfg.bed = reactor::presets::van_dal_lab_mass_primary();
  cfg.record_profile = false;
  const reactor::ReactorState inlet = reactor::van_dal_lab_feed();

  const auto fresh = reactor::integrate_reactor(inlet, cfg);
  const auto aged1 = integration::integrate_aged_reactor(inlet, cfg, 1.0);

  std::printf("[1] Activity 1.0 reproduces the fresh reactor bit for bit\n");
  checkTrue("both ok", fresh.ok && aged1.ok);
  check("outlet temperature difference", aged1.outlet.T_K - fresh.outlet.T_K, 0.0, 0.0);
  check("outlet pressure difference",    aged1.outlet.P_Pa - fresh.outlet.P_Pa, 0.0, 0.0);
  check("conversion difference",
        aged1.co2_conversion(inlet) - fresh.co2_conversion(inlet), 0.0, 0.0);
  check("methanol difference", aged1.meoh_yield_mol_s() - fresh.meoh_yield_mol_s(), 0.0, 0.0);
  check("same grid", static_cast<double>(aged1.n_steps_used),
        static_cast<double>(fresh.n_steps_used), 0.0);

  // Van-Dal's laboratory bed is EQUILIBRIUM limited, not kinetically limited:
  // it holds enough catalyst to reach the exothermic equilibrium ceiling even
  // with much of the activity gone. So conversion is deliberately NOT expected
  // to fall proportionally with activity. What must fall is methanol
  // production once the bed finally becomes rate limited
  std::printf("\n[2] The bed is equilibrium limited at moderate deactivation\n");
  const auto a80 = integration::integrate_aged_reactor(inlet, cfg, 0.8);
  const auto a50 = integration::integrate_aged_reactor(inlet, cfg, 0.5);
  const auto a20 = integration::integrate_aged_reactor(inlet, cfg, 0.2);
  const auto a05 = integration::integrate_aged_reactor(inlet, cfg, 0.05);
  checkTrue("all ok", a80.ok && a50.ok && a20.ok && a05.ok);
  checkRel("80 % activity still reaches the equilibrium conversion",
           a80.co2_conversion(inlet), fresh.co2_conversion(inlet), 1e-4);
  checkRel("50 % activity still reaches it too",
           a50.co2_conversion(inlet), fresh.co2_conversion(inlet), 1e-4);

  std::printf("\n[3] Severe deactivation makes the bed rate limited\n");
  checkTrue("5 % activity converts clearly less than fresh",
            a05.co2_conversion(inlet) < 0.95 * fresh.co2_conversion(inlet));
  checkTrue("and produces far less methanol",
            a05.meoh_yield_mol_s() < 0.6 * fresh.meoh_yield_mol_s());
  checkTrue("methanol production never rises as activity falls",
            a80.meoh_yield_mol_s() <= fresh.meoh_yield_mol_s() * (1.0 + 1e-9) &&
            a50.meoh_yield_mol_s() <= a80.meoh_yield_mol_s() * (1.0 + 1e-9) &&
            a20.meoh_yield_mol_s() <= a50.meoh_yield_mol_s() * (1.0 + 1e-9) &&
            a05.meoh_yield_mol_s() <= a20.meoh_yield_mol_s() * (1.0 + 1e-9));

  std::printf("\n[3b] A slower bed runs cooler, which slightly favours equilibrium\n");
  checkTrue("5 % activity heats up much less", a05.outlet.T_K < fresh.outlet.T_K - 20.0);
  // Because synthesis is exothermic, the cooler bed at 20 % activity can reach
  // a marginally HIGHER equilibrium conversion than the fresh one. This is real
  // and is asserted rather than tolerated silently
  checkTrue("cooler bed at 20 % activity does not convert less",
            a20.co2_conversion(inlet) >= fresh.co2_conversion(inlet) - 1e-6);

  std::printf("\n[4] Guards\n");
  checkTrue("activity 0 is refused",   !integration::integrate_aged_reactor(inlet, cfg, 0.0).ok);
  checkTrue("activity > 1 is refused", !integration::integrate_aged_reactor(inlet, cfg, 1.5).ok);

  std::printf("\n[5] Aging campaign over a checkpoint schedule\n");
  const auto decay = activity_decay::presets::fichtl_cza1_523_553K();
  const auto camp = integration::run_aging_campaign(inlet, cfg, decay, 523.0, 1000.0, 200,
                                                     {0.0, 250.0, 500.0, 1000.0});
  checkTrue("campaign ok", camp.ok);
  check("one result per checkpoint", static_cast<double>(camp.checkpoints.size()), 4.0, 0.0);
  checkTrue("activity falls along the schedule",
            camp.checkpoints.front().activity >= camp.checkpoints.back().activity);
  // Again the equilibrium ceiling, so production is the quantity that must
  // not rise, rather than conversion
  checkTrue("methanol production does not rise along the schedule",
            camp.checkpoints.back().reactor.meoh_yield_mol_s() <=
            camp.checkpoints.front().reactor.meoh_yield_mol_s() * (1.0 + 1e-9));
  checkTrue("every checkpoint integrated successfully", [&]{
    for (const auto& c : camp.checkpoints) if (!c.reactor.ok) return false;
    return true; }());

  std::printf("\n[6] Campaign guards\n");
  checkTrue("an empty schedule is refused",
            !integration::run_aging_campaign(inlet, cfg, decay, 523.0, 1000.0, 200, {}).ok);
  checkTrue("an unsorted schedule is refused",
            !integration::run_aging_campaign(inlet, cfg, decay, 523.0, 1000.0, 200,
                                              {500.0, 100.0}).ok);
  checkTrue("a checkpoint past t_end is refused",
            !integration::run_aging_campaign(inlet, cfg, decay, 523.0, 100.0, 200,
                                              {0.0, 500.0}).ok);

  return report("aged reactor");
}
