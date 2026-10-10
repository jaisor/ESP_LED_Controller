#pragma once

#include <stdint.h>
#include <stddef.h>

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
