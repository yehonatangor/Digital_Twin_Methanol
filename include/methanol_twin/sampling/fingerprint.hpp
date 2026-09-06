#pragma once

// Hash of the compiled constant set, so a dataset can name the model that produced it

// FNV-1a over a canonical serialisation of formation data, Cp, critical properties, Psat, LHHW, Xu-Froment, 
// viscosity, PR kij, NRTL binaries and the integrator's accuracy settings

// Values are serialised by exact bit pattern, so the hash is identical across platforms and optimisation levels

// NOT hashed: anything a caller varies per design point (geometry, activity, feed rates, prices)
// Those are inputs, recorded per row

// See docs/25-model-provenance.md

#include <cstdint>
#include <string>

namespace fingerprint {

struct ModelFingerprint {
  std::uint64_t hash = 0; // FNV-1a over the canonical serialisation
  std::string   hex; // hash as 16 lowercase hex digits
  int           n_values = 0; // how many constants went into it

  // Per-subsystem hashes. 
  // If the combined hash differs between two datasets, these localise the change without needing a diff of the source tree
  std::uint64_t thermo_hash = 0; // formation data, Cp, critical props, Psat
  std::uint64_t kinetics_hash = 0; // LHHW + Xu-Froment + combustion
  std::uint64_t transport_hash = 0; // DIPPR-102 viscosity
  std::uint64_t phase_hash = 0; // PR kij + NRTL binaries
  std::uint64_t numerics_hash  = 0; // integrator accuracy settings
};

// Computes the fingerprint of the CURRENTLY COMPILED constant set
ModelFingerprint compute();

// Human-readable multi-line report: the combined hash, each subsystem hash, and the value count
// Suitable for writing as a dataset header comment
std::string report();

// Convenience for embedding in a CSV/Parquet sidecar
std::string hex_hash();

}
