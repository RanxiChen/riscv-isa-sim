#include <cstdint> // DRAMsim3 public header requires uint64_t from its caller.
#include "dramsim3.h"
#include "configuration.h"
#include <algorithm>
#include <deque>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <memory>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

struct Request {
    uint64_t id, address, accepted = 0, callback = 0;
    bool write;
};
struct Response { Request request; uint64_t due; };

// Transaction adapter: DRAMsim3 callbacks carry an address, not an ID.
// FIFO per (direction,address) assigns indistinguishable same-address callbacks
// to requests. This is a bookkeeping convention, not an AXI ordering claim.
class Adapter {
public:
    uint64_t now = 0, rejected = 0, callbacks = 0;
    std::map<std::pair<bool,uint64_t>, std::deque<Request>> pending;
    std::vector<Response> responses;
    std::unique_ptr<dramsim3::MemorySystem> memory;
    const uint64_t response_delay;
    Adapter(const std::string& config, const std::string& out, uint64_t delay)
        : response_delay(delay) {
        memory.reset(dramsim3::GetMemorySystem(config, out,
            [this](uint64_t a) { complete(false, a); },
            [this](uint64_t a) { complete(true, a); }));
    }
    bool submit(Request r) {
        if (!memory->WillAcceptTransaction(r.address, r.write)) {
            ++rejected;
            return false;
        }
        r.accepted = now;
        auto key = std::make_pair(r.write, r.address);
        pending[key].push_back(r);
        if (!memory->AddTransaction(r.address, r.write))
            throw std::runtime_error("acceptance check disagrees with submit");
        return true;
    }
    void tick() { memory->ClockTick(); }
private:
    void complete(bool write, uint64_t address) {
        auto it = pending.find({write, address});
        if (it == pending.end() || it->second.empty())
            throw std::runtime_error("unexpected or duplicate callback");
        Request r = it->second.front();
        it->second.pop_front();
        if (it->second.empty()) pending.erase(it);
        r.callback = now;
        responses.push_back({r, now + response_delay});
        ++callbacks;
    }
};

int main(int argc, char** argv) try {
    if (argc != 8) {
        std::cerr << "usage: ddr_master CONFIG OUT PATTERN COUNT MAX_INFLIGHT ISSUE_WIDTH RESPONSE_DELAY\n";
        return 2;
    }
    const std::string config = argv[1], out = argv[2], pattern = argv[3];
    const uint64_t count = std::stoull(argv[4]), cap = std::stoull(argv[5]);
    const uint64_t width = std::stoull(argv[6]), delay = std::stoull(argv[7]);
    if (!count || !cap || !width || count > 1000000 || delay > 1000000)
        throw std::runtime_error("invalid bounds");
    std::filesystem::create_directories(out);
    dramsim3::Config cfg(config, out);
    Adapter adapter(config, out, delay);
    const uint64_t bytes = adapter.memory->GetBusBits() / 8 * adapter.memory->GetBurstLength();
    auto field = [&](uint64_t value, int pos) { return value << (pos + cfg.shift_bits); };
    auto request = [&](uint64_t id) {
        uint64_t address = 0;
        bool write = false;
        if (pattern == "row-hit") address = field(id % 8, cfg.co_pos);
        else if (pattern == "row-conflict") address = field(id % 2, cfg.ro_pos);
        else if (pattern == "stream") address = id * bytes;
        else if (pattern == "duplicate") address = 0;
        else if (pattern == "mixed") { address = id * bytes; write = id % 2 == 0; }
        else throw std::runtime_error("unknown pattern");
        if (address % bytes) throw std::runtime_error("unaligned transaction");
        return Request{id, address, 0, 0, write};
    };
    std::ofstream trace(out + "/requests.csv");
    trace << "id,write,address,channel,rank,bankgroup,bank,row,column,accepted_tick,callback_tick,response_tick,event\n";
    uint64_t next = 0, done = 0, inflight = 0, peak = 0, reads = 0, writes = 0;
    uint64_t cap_stalls = 0, final_tick = 0;
    std::vector<bool> seen(count, false);
    constexpr uint64_t timeout = 2000000;
    for (; adapter.now < timeout && done < count; ++adapter.now) {
        // Master consumes responses before issuing more work at this boundary.
        auto& responses = adapter.responses;
        for (auto it = responses.begin(); it != responses.end();) {
            if (it->due > adapter.now) { ++it; continue; }
            const auto& r = it->request;
            if (seen[r.id] || !inflight || r.callback < r.accepted)
                throw std::runtime_error("response conservation/timing violation");
            seen[r.id] = true;
            --inflight; ++done;
            r.write ? ++writes : ++reads;
            const auto a = cfg.AddressMapping(r.address);
            trace << r.id << ',' << r.write << ',' << r.address << ',' << a.channel << ','
                  << a.rank << ',' << a.bankgroup << ',' << a.bank << ',' << a.row << ','
                  << a.column << ',' << r.accepted << ',' << r.callback << ',' << adapter.now
                  << ',' << (r.write ? "write_buffer_ack" : "read_ready") << '\n';
            it = responses.erase(it);
            final_tick = adapter.now;
        }
        if (next < count && inflight == cap) ++cap_stalls;
        for (uint64_t n = 0; n < width && next < count && inflight < cap; ++n) {
            if (!adapter.submit(request(next))) break; // retry same request next tick
            ++next; ++inflight;
            peak = std::max(peak, inflight);
        }
        adapter.tick();
    }
    if (done != count || next != count || !adapter.pending.empty() || !adapter.responses.empty())
        throw std::runtime_error("timeout or undelivered requests");
    // Write callbacks acknowledge buffering, not physical drain. Tail time is
    // explicitly separate from measured completion time; tests inspect commands.
    for (uint64_t n = 0; n < 20000; ++n, ++adapter.now) adapter.tick();
    if (adapter.callbacks != count) throw std::runtime_error("callback count mismatch");
    adapter.memory->PrintStats();
    std::ofstream summary(out + "/summary.json");
    summary << "{\"requests\":" << count << ",\"read_ready\":" << reads
            << ",\"write_buffer_ack\":" << writes << ",\"completion_ticks\":" << final_tick
            << ",\"dram_tick_ns\":" << adapter.memory->GetTCK()
            << ",\"peak_inflight\":" << peak << ",\"master_cap_stall_ticks\":" << cap_stalls
            << ",\"backend_rejections\":" << adapter.rejected << ",\"tail_ticks\":20000}\n";
    if (!trace || !summary) throw std::runtime_error("output write failure");
    std::cout << "PASS " << pattern << " requests=" << count << " completion_ticks=" << final_tick
              << " rejected=" << adapter.rejected << '\n';
    return 0;
} catch (const std::exception& e) {
    std::cerr << "FAIL: " << e.what() << '\n';
    return 1;
}
