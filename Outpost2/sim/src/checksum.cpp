#include "op2/sim/checksum.h"

#include "op2/sim/clock.h"
#include "op2/sim/prng.h"

namespace op2::sim {

namespace {
// Read 4 bytes little-endian (endian-stable on any host).
inline std::uint32_t readU32LE(const std::uint8_t* p) noexcept {
    return static_cast<std::uint32_t>(p[0]) |
           (static_cast<std::uint32_t>(p[1]) << 8) |
           (static_cast<std::uint32_t>(p[2]) << 16) |
           (static_cast<std::uint32_t>(p[3]) << 24);
}
} // namespace

std::uint32_t hashKernel(std::uint32_t byteCount, const void* data) noexcept {
    // Engine checks (short)byteCount — only the low 16 bits.
    if ((byteCount & 0xFFFFu) == 0) {
        return 1;
    }

    const auto* d = static_cast<const std::uint8_t*>(data);
    const std::uint32_t nDwords = byteCount >> 2;
    const std::uint32_t nTrailing = byteCount & 3u;

    std::uint32_t h = 0;
    std::size_t offset = 0;

    // Prologue: fold 0..3 trailing bytes first (high-byte accumulation).
    if (nTrailing != 0) {
        std::uint32_t tmp = 0;
        for (std::uint32_t i = 0; i < nTrailing; ++i) {
            tmp = (h & 0xFFFFFF00u) | (d[offset] & 0xFFu);
            h = tmp << 8;
            ++offset;
        }
        h = (tmp & 0xFFFFFFu) << 7;
        if ((nDwords & 0xFFFFu) == 0) {
            return h ? h : 1u;
        }
    }

    // Main loop: XOR each dword into h, then right-rotate by 1.
    for (std::uint32_t i = 0; i < nDwords; ++i) {
        const std::uint32_t dw = readU32LE(d + offset);
        offset += 4;
        const std::uint32_t x = h ^ dw;
        h = (x >> 1) | ((x & 1u) << 31);
    }

    return h ? h : 1u;
}

void Checksummer::addBytes(const void* data, std::size_t n) noexcept {
    const auto* d = static_cast<const std::uint8_t*>(data);
    std::size_t i = 0;
    for (; i + 4 <= n; i += 4) {
        addU32(readU32LE(d + i));
    }
    if (i < n) {
        std::uint32_t last = 0;
        for (std::size_t shift = 0; i < n; ++i, shift += 8) {
            last |= static_cast<std::uint32_t>(d[i]) << shift;
        }
        addU32(last);
    }
}

std::uint32_t checksumCoreState(const Clock& clock, const Prng& prng) noexcept {
    Checksummer cs;
    cs.addU32(clock.tick());
    const Prng::State st = prng.state();
    cs.addU32(st.s0);
    cs.addU32(st.s1);
    return cs.value();
}

} // namespace op2::sim
