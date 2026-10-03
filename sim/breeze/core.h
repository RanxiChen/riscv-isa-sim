#pragma once

namespace breeze {

// Flow RTL baseline: df791952b56e6c87d8d593dea610b2c8224befef.
// The active core still uses the retained BreezeDCache.
class BreezeCore {
 public:
  void step();
};

}  // namespace breeze
