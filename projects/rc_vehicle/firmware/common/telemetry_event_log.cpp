#include "telemetry_event_log.hpp"

namespace rc_vehicle {

TelemetryEventLog::TelemetryEventLog() = default;
TelemetryEventLog::~TelemetryEventLog() = default;

void TelemetryEventLog::Push(const TelemetryEvent& evt) {
  std::lock_guard<std::mutex> lock(mutex_);
  buf_[write_pos_] = evt;
  write_pos_ = (write_pos_ + 1) % kCapacity;
  if (count_ < kCapacity) {
    ++count_;
  }
}

size_t TelemetryEventLog::Count() const {
  std::lock_guard<std::mutex> lock(mutex_);
  return count_;
}

bool TelemetryEventLog::GetEvent(size_t idx, TelemetryEvent& out) const {
  std::lock_guard<std::mutex> lock(mutex_);
  if (idx >= count_) {
    return false;
  }
  // oldest element sits at (write_pos_ - count_ + kCapacity) % kCapacity
  size_t real_idx = (write_pos_ - count_ + idx + kCapacity) % kCapacity;
  out = buf_[real_idx];
  return true;
}

void TelemetryEventLog::Clear() {
  std::lock_guard<std::mutex> lock(mutex_);
  write_pos_ = 0;
  count_ = 0;
}

}  // namespace rc_vehicle
