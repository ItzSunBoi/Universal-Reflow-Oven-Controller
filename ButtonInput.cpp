#include "ButtonInput.h"

void ButtonInput::begin() {
  for (auto &state : states_) {
    pinMode(state.pin, INPUT_PULLUP);
    const bool pressed = digitalRead(state.pin) == LOW;
    state.rawPressed = pressed;
    state.stablePressed = pressed;
    state.rawChangedMs = millis();
    state.pressedMs = pressed ? millis() : 0;
  }
}

void ButtonInput::update(uint32_t nowMs) {
  for (auto &state : states_) {
    updateOne(state, nowMs);
  }
}

bool ButtonInput::nextEvent(ButtonEvent &event) {
  if (queueHead_ == queueTail_) {
    return false;
  }
  event = queue_[queueTail_];
  queueTail_ = static_cast<uint8_t>((queueTail_ + 1U) % QUEUE_SIZE);
  return true;
}

void ButtonInput::updateOne(State &state, uint32_t nowMs) {
  const bool raw = digitalRead(state.pin) == LOW;
  if (raw != state.rawPressed) {
    state.rawPressed = raw;
    state.rawChangedMs = nowMs;
  }

  if ((nowMs - state.rawChangedMs) >= DEBOUNCE_MS && raw != state.stablePressed) {
    state.stablePressed = raw;
    if (raw) {
      state.pressedMs = nowMs;
      state.lastRepeatMs = nowMs;
      state.longSent = false;
    } else {
      if (!state.longSent) {
        push(state.id, ButtonAction::SHORT_PRESS);
      }
      state.longSent = false;
    }
  }

  if (!state.stablePressed) {
    return;
  }

  const uint32_t heldMs = nowMs - state.pressedMs;
  if (!state.longSent && heldMs >= LONG_PRESS_MS) {
    state.longSent = true;
    state.lastRepeatMs = nowMs;
    push(state.id, ButtonAction::LONG_PRESS);
  } else if (state.longSent && heldMs >= REPEAT_START_MS &&
             (nowMs - state.lastRepeatMs) >= REPEAT_INTERVAL_MS) {
    state.lastRepeatMs = nowMs;
    push(state.id, ButtonAction::REPEAT);
  }
}

void ButtonInput::push(ButtonId id, ButtonAction action) {
  const uint8_t next = static_cast<uint8_t>((queueHead_ + 1U) % QUEUE_SIZE);
  if (next == queueTail_) {
    return;  // Queue full: discard the newest repeat rather than blocking.
  }
  queue_[queueHead_] = {id, action};
  queueHead_ = next;
}
