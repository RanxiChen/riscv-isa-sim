#include "dcache.h"

namespace breeze {

DCache::DCache() : DCache(Config{}) {}

DCache::DCache(Config config) : config_(config) { reset(); }

void DCache::reset() {
  sets_ = {};
  state_ = State::Idle;
  request_ = {};
  pending_ = false;
  hit_ = false;
  selected_way_ = 0;
  remaining_ = 0;
}

bool DCache::request(Request request) {
  if (!idle()) return false;
  request_ = request;
  pending_ = true;
  return true;
}

DCache::Response DCache::response() const {
  return {state_ == State::Respond, hit_};
}

uint8_t DCache::touch(uint8_t plru, unsigned way) {
  switch (way) {
    case 0: return (plru & 1) | 6;
    case 1: return (plru & 1) | 4;
    case 2: return (plru & 2) | 1;
    default: return plru & 2;
  }
}

unsigned DCache::victim(const Set& set) {
  for (unsigned i = 0; i < 4; ++i)
    if (!set.ways[i].valid) return i;
  return (set.plru & 4) ? 2 + ((set.plru & 1) ? 1 : 0)
                        : ((set.plru & 2) ? 1 : 0);
}

void DCache::tick() {
  auto& set = sets_[set_index(request_.address)];
  switch (state_) {
    case State::Idle:
      if (pending_) {
        pending_ = false;
        hit_ = false;
        state_ = State::Lookup;
      }
      break;
    case State::Lookup: {
      unsigned way = 4;
      for (unsigned i = 0; i < 4; ++i) {
        if (set.ways[i].valid && set.ways[i].tag == tag(request_.address)) {
          way = i;
          break;
        }
      }
      if (way != 4) {
        hit_ = true;
        selected_way_ = way;
        auto& line = set.ways[way];
        if (request_.write) {
          if (!line.exclusive && !line.dirty) {
            state_ = State::UpgradeReq;
          } else {
            line.exclusive = false;
            line.dirty = true;
            set.plru = touch(set.plru, way);
            state_ = State::StoreHitWrite;
          }
        } else {
          set.plru = touch(set.plru, way);
          state_ = State::Respond;
        }
      } else {
        selected_way_ = victim(set);
        // A valid victim must be released through Home, whether clean or dirty.
        state_ = set.ways[selected_way_].valid ? State::PutReq : State::RefillReq;
      }
      break;
    }
    case State::StoreHitWrite:
      state_ = State::Respond;
      break;
    case State::PutReq:
      remaining_ = config_.home_latency;
      state_ = State::PutWait;
      break;
    case State::PutWait:
      if (remaining_ != 0) --remaining_;
      else {
        set.ways[selected_way_].valid = false;
        state_ = State::RefillReq;
      }
      break;
    case State::RefillReq:
      remaining_ = config_.home_latency;
      state_ = State::RefillWait;
      break;
    case State::RefillWait:
      if (remaining_ != 0) --remaining_;
      else state_ = State::RefillInstall;
      break;
    case State::RefillInstall: {
      auto& line = set.ways[selected_way_];
      line = {true, !request_.write && config_.load_grants_exclusive,
              request_.write, tag(request_.address)};
      set.plru = touch(set.plru, selected_way_);
      // The retained RTL merges an ordinary store during RefillInstall and
      // goes directly to Respond; StoreHitWrite is only the hit/upgrade path.
      state_ = State::Respond;
      break;
    }
    case State::UpgradeReq:
      remaining_ = config_.home_latency;
      state_ = State::UpgradeWait;
      break;
    case State::UpgradeWait:
      if (remaining_ != 0) --remaining_;
      else {
        auto& line = set.ways[selected_way_];
        line.exclusive = false;
        line.dirty = true;
        set.plru = touch(set.plru, selected_way_);
        state_ = State::StoreHitWrite;
      }
      break;
    case State::Respond:
      state_ = State::Idle;
      break;
  }
}

}  // namespace breeze
