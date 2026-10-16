#include "op2/sim/replay.h"

#include <algorithm>
#include <map>

namespace op2::sim {

Replay::Replay(std::vector<CommandPacket> packets) : packets_(std::move(packets)) {
    // Stable sort by tick so commands issued on the same tick keep their order.
    std::stable_sort(packets_.begin(), packets_.end(),
                     [](const CommandPacket& a, const CommandPacket& b) {
                         return a.tick < b.tick;
                     });
}

ReplayResult Replay::run(std::uint32_t maxTicks, std::uint32_t checksumEvery) {
    ReplayResult result;

    std::size_t cursor = 0; // next packet to dispatch
    for (std::uint32_t tick = 0; tick < maxTicks; ++tick) {
        // Dispatch every command scheduled for this tick (already sorted).
        while (cursor < packets_.size() && packets_[cursor].tick == tick) {
            if (onCommand) onCommand(packets_[cursor]);
            ++cursor;
        }

        if (onTick) onTick(tick);

        if (checksumFn && checksumEvery != 0 && (tick % checksumEvery) == 0) {
            result.checksums.emplace_back(tick, checksumFn());
        }
    }
    return result;
}

std::vector<Divergence> Replay::diff(
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& a,
    const std::vector<std::pair<std::uint32_t, std::uint32_t>>& b) {
    std::map<std::uint32_t, std::uint32_t> ma(a.begin(), a.end());
    std::map<std::uint32_t, std::uint32_t> mb(b.begin(), b.end());

    std::vector<Divergence> divs;
    // Walk the union of ticks in order.
    auto ia = ma.begin();
    auto ib = mb.begin();
    while (ia != ma.end() || ib != mb.end()) {
        std::uint32_t tick;
        bool hasA = false, hasB = false;
        std::uint32_t va = 0, vb = 0;

        if (ia != ma.end() && (ib == mb.end() || ia->first <= ib->first)) {
            tick = ia->first;
            hasA = true;
            va = ia->second;
            if (ib != mb.end() && ib->first == tick) { hasB = true; vb = ib->second; ++ib; }
            ++ia;
        } else {
            tick = ib->first;
            hasB = true;
            vb = ib->second;
            ++ib;
        }

        if (!hasA || !hasB || va != vb) {
            divs.push_back(Divergence{tick, va, vb});
        }
    }
    return divs;
}

} // namespace op2::sim
