#include "op2/sim/cmdlog.h"

#include <cctype>
#include <cstdio>
#include <fstream>
#include <map>
#include <stdexcept>

namespace op2::sim {

namespace {

// CommandPacket opcode names (0x00..0x36), from
// InternalData/DataStructure CommandPacket.txt.
const char* const kOpcodeNames[] = {
    "ctNop",          "ctMoDoze",       "ctMoMove",       "ctMoDock",
    "ctMoDockEG",     "ctMoStop",       "ctMoBuild",      "ctMoBuildWall",
    "ctMoRemoveWall", "ctMoProduce",    "ctMoTransferCargo", "ctMoLoadCargo",
    "ctMoUnloadCargo","ctMoRecycle",    "ctMoDumpCargo",  "ctMoScavenge",
    "ctMoSpecialWait","ctMoSurvey",     "ctMoIdle",       "ctMoUnidle",
    "ctMoSelfDestruct","ctMoScatter",   "ctMoResearch",   "ctMoTrainScientists",
    "ctMoTransfer",   "ctMoLaunch",     "ctMoFlyInSpace", "ctMoRepair",
    "ctMoRepairObj",  "ctMoReprogram",  "ctMoDismantle",  "ctMoSalvage",
    "ctMoCreate",     "ctMoDevelop",    "ctMoUnDevelop",  "ctMoLightToggle",
    "ctMoAttackObj",  "ctMoGuard",      "ctMoStandGround","ctMoCargoRoute",
    "ctMoPatrol",     "ctMapChange",    "ctMoPoof",       "ctGameOpt",
    "ctGodWeapon",    "ctChatText",     "ctChatSFX",      "ctMoDeath",
    "ctChat",         "ctQuit",         "ctAlly",         "ctGoAI",
    "ctMachineSettings","ctInvalidCommand","WeaponFiring",
};
constexpr std::uint32_t kNumOpcodes =
    sizeof(kOpcodeNames) / sizeof(kOpcodeNames[0]); // 0x37

// --- Minimal flat-JSON-object parser (one cmdlog record per line) -----------
// Handles exactly our record shape: {"key":int, "key":"string", ...}. No nested
// objects/arrays, no escape sequences (none appear in our generated files).

void skipWs(const std::string& s, std::size_t& i) {
    while (i < s.size() && std::isspace(static_cast<unsigned char>(s[i]))) ++i;
}

std::map<std::string, std::string> parseFlatObject(const std::string& line) {
    std::map<std::string, std::string> out;
    std::size_t i = 0;
    skipWs(line, i);
    if (i >= line.size() || line[i] != '{') {
        throw std::runtime_error("cmdlog: expected '{' in line: " + line);
    }
    ++i;
    while (true) {
        skipWs(line, i);
        if (i < line.size() && line[i] == '}') break;
        if (i >= line.size() || line[i] != '"') {
            throw std::runtime_error("cmdlog: expected key in line: " + line);
        }
        ++i;
        std::string key;
        while (i < line.size() && line[i] != '"') key += line[i++];
        if (i >= line.size()) throw std::runtime_error("cmdlog: unterminated key");
        ++i; // closing quote
        skipWs(line, i);
        if (i >= line.size() || line[i] != ':') {
            throw std::runtime_error("cmdlog: expected ':' in line: " + line);
        }
        ++i;
        skipWs(line, i);
        std::string val;
        if (i < line.size() && line[i] == '"') {
            ++i;
            while (i < line.size() && line[i] != '"') val += line[i++];
            if (i >= line.size()) throw std::runtime_error("cmdlog: unterminated string");
            ++i; // closing quote
        } else {
            while (i < line.size() && line[i] != ',' && line[i] != '}' &&
                   !std::isspace(static_cast<unsigned char>(line[i]))) {
                val += line[i++];
            }
        }
        out[key] = val;
        skipWs(line, i);
        if (i < line.size() && line[i] == ',') { ++i; continue; }
        skipWs(line, i);
        if (i < line.size() && line[i] == '}') break;
    }
    return out;
}

std::vector<std::uint8_t> hexToBytes(const std::string& hex) {
    auto nib = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return -1;
    };
    std::vector<std::uint8_t> out;
    if (hex.size() % 2 != 0) throw std::runtime_error("cmdlog: odd-length data_hex");
    for (std::size_t i = 0; i < hex.size(); i += 2) {
        const int hi = nib(hex[i]);
        const int lo = nib(hex[i + 1]);
        if (hi < 0 || lo < 0) throw std::runtime_error("cmdlog: bad hex in data_hex");
        out.push_back(static_cast<std::uint8_t>((hi << 4) | lo));
    }
    return out;
}

std::string bytesToHex(const std::vector<std::uint8_t>& bytes) {
    static const char* k = "0123456789abcdef";
    std::string s;
    s.reserve(bytes.size() * 2);
    for (std::uint8_t b : bytes) {
        s += k[b >> 4];
        s += k[b & 0xF];
    }
    return s;
}

} // namespace

std::string opcodeName(std::uint32_t op) {
    if (op == kStartGameMarker) return "<START_GAME>";
    if (op < kNumOpcodes) return kOpcodeNames[op];
    char buf[16];
    std::snprintf(buf, sizeof(buf), "op_0x%x", op);
    return buf;
}

int opcodeFromName(const std::string& name) {
    if (name == "<START_GAME>") return static_cast<int>(kStartGameMarker);
    for (std::uint32_t i = 0; i < kNumOpcodes; ++i) {
        if (name == kOpcodeNames[i]) return static_cast<int>(i);
    }
    return -1;
}

Cmdlog Cmdlog::readJsonl(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cmdlog: cannot open " + path);

    Cmdlog log;
    std::string line;
    while (std::getline(f, line)) {
        // Trim leading whitespace to detect blanks/comments.
        std::size_t start = 0;
        while (start < line.size() &&
               std::isspace(static_cast<unsigned char>(line[start]))) ++start;
        if (start >= line.size() || line[start] == '#') continue;

        auto obj = parseFlatObject(line);
        if (obj.count("type") && obj["type"] == "meta") continue;

        CommandPacket p;
        if (obj.count("tick")) p.tick = static_cast<std::uint32_t>(std::stoul(obj["tick"]));
        if (obj.count("op_id")) {
            p.op = static_cast<std::uint32_t>(std::stoul(obj["op_id"]));
        } else if (obj.count("op")) {
            const int id = opcodeFromName(obj["op"]);
            if (id < 0) throw std::runtime_error("cmdlog: unknown op name " + obj["op"]);
            p.op = static_cast<std::uint32_t>(id);
        }
        if (obj.count("player")) p.player = std::stoi(obj["player"]);
        if (obj.count("data_hex")) p.data = hexToBytes(obj["data_hex"]);
        log.packets.push_back(std::move(p));
    }
    return log;
}

void Cmdlog::writeJsonl(const std::string& path, const std::string& source) const {
    std::ofstream f(path);
    if (!f) throw std::runtime_error("cmdlog: cannot write " + path);

    f << "{\"type\":\"meta\",\"source\":\"" << source
      << "\",\"format\":\"op2_cmdlog_v1\"}\n";
    for (const auto& p : packets) {
        f << "{\"tick\":" << p.tick
          << ",\"op\":\"" << opcodeName(p.op) << "\""
          << ",\"op_id\":" << p.op
          << ",\"len\":" << p.data.size()
          << ",\"data_hex\":\"" << bytesToHex(p.data) << "\"";
        if (p.player >= 0) f << ",\"player\":" << p.player;
        f << "}\n";
    }
}

} // namespace op2::sim
