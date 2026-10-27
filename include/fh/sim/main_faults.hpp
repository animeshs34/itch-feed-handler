#ifndef FH_SIM_MAIN_FAULTS_HPP_
#define FH_SIM_MAIN_FAULTS_HPP_

#include <vector>

#include "fh/sim/delivery.hpp"

namespace fh::sim {

// Faults on top of what delivery already did to the main feed. Each function works
// on the list of arrivals of BOTH lines together, and each only strikes arrivals
// before `faulty_until`.
//
// They are written so that no fault can quietly change what the handler should
// believe: damage either removes a datagram (which the handler must treat as loss)
// or adds a copy that arrives AFTER a genuine copy of the same messages, so a
// first-copy-wins handler can never prefer it.

// Silence: for each of `faults.silence_windows`, one line (or both) hears nothing for
// longer than the core's silent window. Arrivals inside the window are removed.
void apply_silence(std::vector<Arrival>& arrivals, const FaultConfig& faults, Nanos faulty_until,
                   Nanos spacing, fhtest::SplitMix64& rng);

// Malformed: some datagrams are cut short, at a random point, so that their structure
// is broken. They are marked not genuine. A broken datagram is dropped whole by the
// decoder, so to the core this is a loss.
void apply_malformed(std::vector<Arrival>& arrivals, const FaultConfig& faults, Nanos faulty_until,
                     fhtest::SplitMix64& rng);

// Mismatch: for some message datagrams, an extra copy with one byte of one message
// changed arrives a little after the genuine one. The handler must raise a line
// mismatch (or drop the damaged message) and keep the genuine data.
// Only genuine message datagrams are copied.
void add_mismatches(std::vector<Arrival>& arrivals, const FaultConfig& faults, Nanos faulty_until,
                    fhtest::SplitMix64& rng);

// Wrong session: for some datagrams, an extra copy with another session id arrives a
// little after the original. The original names the session first, so the copy must be
// refused.
void add_wrong_session(std::vector<Arrival>& arrivals, const FaultConfig& faults,
                       Nanos faulty_until, fhtest::SplitMix64& rng);

}  // namespace fh::sim

#endif  // FH_SIM_MAIN_FAULTS_HPP_
