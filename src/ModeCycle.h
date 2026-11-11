#pragma once

#include <stdint.h>
#include <stddef.h>
#include <time.h>

// The mode to show after `current` when cycling. With a selection (`count` > 0) that is the next entry of
// `list` that is available, in list order and wrapping around; an empty selection means every mode in
// index order. It starts from the showing mode rather than a remembered list position, so the order still
// holds after the mode was changed from the web page, /api or Home Assistant - a showing mode that isn't in
// the selection continues with the selection's first entry. Returns `current` when nothing else qualifies.
template <typename Available>
uint8_t nextCycleMode(uint8_t current, const uint8_t *list, uint8_t count, size_t modeCount, Available available) {
  if (count > 0) {
    int position = -1;
    for (uint8_t i = 0; i < count; i++) {
      if (list[i] == current) {
        position = i;
        break;
      }
    }
    for (uint8_t step = 1; step <= count; step++) {
      uint8_t candidate = list[(position + step) % count];
      if (candidate != current && available(candidate)) {
        return candidate;
      }
    }
    return current;
  }
  for (size_t step = 1; step < modeCount; step++) {
    uint8_t candidate = (current + step) % modeCount;
    if (available(candidate)) {
      return candidate;
    }
  }
  return current;
}

// Synchronized mode changes: the next wall-clock time (epoch seconds) for a change. The cycle duration is
// rounded up to whole minutes and changes land on multiples of it counted from the epoch (UTC), so every
// controller with the same duration changes at the same moment, whenever and wherever it was set up.
// The change is the first such boundary more than `minLead` seconds after `now`.
inline time_t nextSyncedChange(time_t now, unsigned long cycleMs, uint32_t minLead) {
  time_t period = (time_t)((cycleMs / 1000 + 59) / 60) * 60;
  if (period < 60) {
    period = 60;
  }
  time_t next = (now / period + 1) * period;
  while (next - now <= (time_t)minLead) {
    next += period;
  }
  return next;
}

// When the next synchronized change is due. It reschedules - to the next boundary more than `minLead`
// seconds out - when synchronizing starts, when the duration changes, and when the mode was changed from
// elsewhere (web page, /api, Home Assistant), so a change never follows within `minLead` seconds.
struct ModeSyncSchedule {
  bool active = false;
  unsigned long durationMs = 0;
  uint8_t mode = 0;
  time_t next = 0;

  bool due(time_t now, unsigned long cycleMs, uint8_t currentMode, uint32_t minLead) {
    if (!active || durationMs != cycleMs || mode != currentMode) {
      next = nextSyncedChange(now, cycleMs, minLead);
      active = true;
      durationMs = cycleMs;
      mode = currentMode;
    }
    if (now < next) {
      return false;
    }
    next = nextSyncedChange(now, cycleMs, minLead);
    return true;
  }
  // The scheduled change switched to this mode (so it isn't taken for a change from elsewhere)
  void changedTo(uint8_t newMode) { mode = newMode; }
  void stop() { active = false; }
};
