cmake_minimum_required(VERSION 3.16)
project(methanol_twin CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

if(NOT CMAKE_BUILD_TYPE)
  set(CMAKE_BUILD_TYPE Release)
endif()

add_compile_options(-Wall -Wextra)

# Core library

# Sources are grouped by the layer they belong to, and each group compiles against the groups above it and nothing below
# Later layers append their own group
# The dependency order stays readable in the build file itself

# Layer 1. Units, species table, stream type, ideal-gas thermodynamics
set(METHANOL_TWIN_SRC_FOUNDATION
  src/species.cpp
  src/thermo.cpp
)

# Layer 2. Peng-Robinson, NRTL, Rachford-Rice flash
set(METHANOL_TWIN_SRC_PHASE_EQUILIBRIUM
  src/eos.cpp
  src/nrtl.cpp
  src/flash.cpp
)

# Layer 3. LHHW methanol synthesis, reforming equilibrium
set(METHANOL_TWIN_SRC_KINETICS
  src/lhhw.cpp
  src/trm_kinetics.cpp
)

# Layer 4. Packed-bed reactor: transport, pressure drop, energy balance, the coupled integrator and the two-stage configuration
set(METHANOL_TWIN_SRC_REACTOR
  src/reactor/transport.cpp
  src/reactor/ergun.cpp
  src/reactor/energy_balance.cpp
  src/reactor/reactor_core.cpp
  src/reactor/two_stage_reactor.cpp
)

# Layer 5. Tri-reforming reactor
set(METHANOL_TWIN_SRC_TRI_REFORMING
  src/front_end/trm_reactor.cpp
)

# Layer 6. Process units
set(METHANOL_TWIN_SRC_PROCESS_UNITS
  src/front_end/electrolyzer.cpp
  src/front_end/compressor.cpp
  src/front_end/compressor_train.cpp
  src/front_end/heat_exchanger.cpp
  src/front_end/knockout_drum.cpp
  src/front_end/recycle.cpp
  src/front_end/membrane.cpp
  src/front_end/distillation.cpp
  src/front_end/feed_blend.cpp
)

# Layer 7. Catalyst deactivation and its coupling to the reactor
set(METHANOL_TWIN_SRC_DEGRADATION
  src/degradation/activity_decay.cpp
  src/integration/aged_reactor.cpp
)

# Layer 8. Costing and economics
set(METHANOL_TWIN_SRC_ECONOMICS
  src/economics/capex_opex.cpp
  src/economics/plant_economics.cpp
  src/economics/catalyst_replacement.cpp
  src/economics/vessel_sizing.cpp
  src/economics/column_sizing.cpp
  src/economics/separator_capex.cpp
)

# Layer 9. Hydrogen storage and price-threshold dispatch
set(METHANOL_TWIN_SRC_DISPATCH
  src/dispatch/storage.cpp
  src/dispatch/dispatch_loop.cpp
)

# Layer 10. Flowsheets, from the single-pass chain up to the design point
set(METHANOL_TWIN_SRC_FLOWSHEET
  src/flowsheet/co2_h2_plant.cpp
  src/flowsheet/co2_h2_plant_recycle.cpp
  src/flowsheet/co2_h2_plant_recycle_purified.cpp
  src/flowsheet/co2_h2_plant_recycle_aged.cpp
  src/flowsheet/co2_h2_plant_recycle_purified_aged.cpp
  src/flowsheet/compression_train.cpp
  src/flowsheet/hybrid_plant.cpp
  src/flowsheet/hybrid_plant_recycle.cpp
  src/flowsheet/hybrid_plant_recycle_purified.cpp
  src/flowsheet/hybrid_plant_recycle_purified_compressed.cpp
  src/flowsheet/hybrid_plant_recycle_purified_compressed_aged.cpp
  src/flowsheet/hybrid_plant_design_point.cpp
  src/flowsheet/hybrid_plant_design_point_full.cpp
)

# Layer 11. Validity ranges, feasibility classification, model fingerprint
set(METHANOL_TWIN_SRC_SAMPLING
  src/sampling/validity.cpp
  src/sampling/feasibility.cpp
  src/sampling/fingerprint.cpp
)

add_library(methanol_twin_core STATIC
  ${METHANOL_TWIN_SRC_FOUNDATION}
  ${METHANOL_TWIN_SRC_PHASE_EQUILIBRIUM}
  ${METHANOL_TWIN_SRC_KINETICS}
  ${METHANOL_TWIN_SRC_REACTOR}
  ${METHANOL_TWIN_SRC_TRI_REFORMING}
  ${METHANOL_TWIN_SRC_PROCESS_UNITS}
  ${METHANOL_TWIN_SRC_DEGRADATION}
  ${METHANOL_TWIN_SRC_ECONOMICS}
  ${METHANOL_TWIN_SRC_DISPATCH}
  ${METHANOL_TWIN_SRC_FLOWSHEET}
  ${METHANOL_TWIN_SRC_SAMPLING}
)

# Needed so the static library can be linked into a shared module, which is what a pybind11 binding is
set_property(TARGET methanol_twin_core PROPERTY POSITION_INDEPENDENT_CODE ON)

# Headers are included by bare filename (reactor_core.hpp does #include "energy_balance.hpp")
# Each subdirectory holding sources is on the include path alongside include/methanol_twin itself
target_include_directories(methanol_twin_core PUBLIC
  include/methanol_twin
  include/methanol_twin/reactor
  include/methanol_twin/front_end
  include/methanol_twin/degradation
  include/methanol_twin/economics
  include/methanol_twin/integration
  include/methanol_twin/dispatch
  include/methanol_twin/flowsheet
  include/methanol_twin/sampling
)

# Tests
# One self-contained printf-based PASS/FAIL binary per module, matching the project's convention 
# Each is registered with CTest so `ctest` runs all of them and reports per binary
enable_testing()

function(methanol_twin_add_test name source)
  if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/${source})
    add_executable(${name} ${source})
    target_link_libraries(${name} PRIVATE methanol_twin_core)
    target_include_directories(${name} PRIVATE tests)
    add_test(NAME ${name} COMMAND ${name})
  endif()
endfunction()

methanol_twin_add_test(test_thermo           tests/test_thermo.cpp)
methanol_twin_add_test(test_vle              tests/test_vle.cpp)
methanol_twin_add_test(test_kinetics         tests/test_kinetics.cpp)
methanol_twin_add_test(test_reactor          tests/test_reactor.cpp)
methanol_twin_add_test(test_two_stage_reactor tests/test_two_stage_reactor.cpp)
methanol_twin_add_test(test_trm_reactor      tests/test_trm_reactor.cpp)
methanol_twin_add_test(test_electrolyzer     tests/test_electrolyzer.cpp)
methanol_twin_add_test(test_compressor       tests/test_compressor.cpp)
methanol_twin_add_test(test_compressor_train tests/test_compressor_train.cpp)
methanol_twin_add_test(test_heat_exchanger   tests/test_heat_exchanger.cpp)
methanol_twin_add_test(test_knockout_drum    tests/test_knockout_drum.cpp)
methanol_twin_add_test(test_recycle          tests/test_recycle.cpp)
methanol_twin_add_test(test_membrane         tests/test_membrane.cpp)
methanol_twin_add_test(test_distillation     tests/test_distillation.cpp)
methanol_twin_add_test(test_feed_blend       tests/test_feed_blend.cpp)
methanol_twin_add_test(test_activity_decay   tests/test_activity_decay.cpp)
methanol_twin_add_test(test_aged_reactor     tests/test_aged_reactor.cpp)
methanol_twin_add_test(test_capex_opex       tests/test_capex_opex.cpp)
methanol_twin_add_test(test_plant_economics  tests/test_plant_economics.cpp)
methanol_twin_add_test(test_vessel_sizing    tests/test_vessel_sizing.cpp)
methanol_twin_add_test(test_column_sizing    tests/test_column_sizing.cpp)
methanol_twin_add_test(test_catalyst_replacement tests/test_catalyst_replacement.cpp)
methanol_twin_add_test(test_storage          tests/test_storage.cpp)
methanol_twin_add_test(test_dispatch_loop    tests/test_dispatch_loop.cpp)
methanol_twin_add_test(test_co2_h2_plant     tests/test_co2_h2_plant.cpp)
methanol_twin_add_test(test_co2_h2_plant_recycle tests/test_co2_h2_plant_recycle.cpp)
methanol_twin_add_test(test_co2_h2_plant_recycle_purified tests/test_co2_h2_plant_recycle_purified.cpp)
methanol_twin_add_test(test_co2_h2_plant_recycle_aged tests/test_co2_h2_plant_recycle_aged.cpp)
methanol_twin_add_test(test_co2_h2_plant_recycle_purified_aged tests/test_co2_h2_plant_recycle_purified_aged.cpp)
methanol_twin_add_test(test_compression_train tests/test_compression_train.cpp)
methanol_twin_add_test(test_hybrid_plant     tests/test_hybrid_plant.cpp)
methanol_twin_add_test(test_hybrid_plant_recycle tests/test_hybrid_plant_recycle.cpp)
methanol_twin_add_test(test_hybrid_plant_recycle_purified tests/test_hybrid_plant_recycle_purified.cpp)
methanol_twin_add_test(test_hybrid_plant_recycle_purified_compressed tests/test_hybrid_plant_recycle_purified_compressed.cpp)
methanol_twin_add_test(test_hybrid_plant_recycle_purified_compressed_aged tests/test_hybrid_plant_recycle_purified_compressed_aged.cpp)
methanol_twin_add_test(test_hybrid_plant_design_point tests/test_hybrid_plant_design_point.cpp)
methanol_twin_add_test(test_hybrid_plant_design_point_full tests/test_hybrid_plant_design_point_full.cpp)
methanol_twin_add_test(test_sampling         tests/test_sampling.cpp)

# Top-level demo executable
# Runs the dispatch, reactor and economics pipeline end to end on an illustrative price series and prints a summary
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp)
  add_executable(methanol_twin src/main.cpp)
  target_link_libraries(methanol_twin PRIVATE methanol_twin_core)
endif()

# Figure data
# Not a test and not part of the library
# Regenerates the measured sweep behind docs/figures/03-operating-envelope.svg, so that figure is reproducible

# ./operating_envelope > ../docs/figures/03-operating-envelope.csv

# The sweep solves the recycle loop 25 times plus up to 4 aged solves per converged node
if(EXISTS ${CMAKE_CURRENT_SOURCE_DIR}/tools/operating_envelope.cpp)
  add_executable(operating_envelope tools/operating_envelope.cpp)
  target_link_libraries(operating_envelope PRIVATE methanol_twin_core)
endif()

# Usage:
#   cmake -S . -B build
#   cmake --build build
#   ctest --test-dir build --output-on-failure
