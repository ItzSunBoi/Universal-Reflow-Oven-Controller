#include "ButtonInput.h"

void ButtonInput::begin() {
  const uint32_t nowMs = millis();
  for (auto &state : states_) {
    pinMode(state.pin, INPUT_PULLUP);
    const bool pressed = digitalRead(state.pin) == LOW;
    state.rawPressed = pressed;
    state.stablePressed = pressed;
    state.longSent = false;
    state.rawChangedMs = nowMs;
    state.pressedMs = pressed ? nowMs : 0;
    state.lastRepeatMs = nowMs;
  }

  eventQueue_ = xQueueCreate(BUTTON_EVENT_QUEUE_LENGTH, sizeof(ButtonEvent));
  if (eventQueue_ == nullptr) {
    Serial.println("WARNING: button event queue allocation failed; using loop scanner");
    return;
  }

  const BaseType_t created = xTaskCreatePinnedToCore(
      taskEntry,
      "buttons",
      BUTTON_TASK_STACK_BYTES,
      this,
      BUTTON_TASK_PRIORITY,
      &taskHandle_,
      BUTTON_TASK_CORE);

  if (created != pdPASS) {
    taskHandle_ = nullptr;
    taskRunning_ = false;
    Serial.println("WARNING: button task creation failed; using loop scanner");
    return;
  }

  taskRunning_ = true;
}

void ButtonInput::service(uint32_t nowMs) {
  if (!taskRunning_) {
    scan(nowMs);
  }
}

bool ButtonInput::nextEvent(ButtonEvent &event) {
  if (eventQueue_ != nullptr) {
    return xQueueReceive(eventQueue_, &event, 0) == pdTRUE;
  }

  if (fallbackHead_ == fallbackTail_) {
    return false;
  }
  event = fallbackQueue_[fallbackTail_];
  fallbackTail_ = static_cast<uint8_t>(
      (fallbackTail_ + 1U) % FALLBACK_QUEUE_SIZE);
  return true;
}

void ButtonInput::taskEntry(void *argument) {
  auto *self = static_cast<ButtonInput *>(argument);
  TickType_t lastWake = xTaskGetTickCount();
  TickType_t period = pdMS_TO_TICKS(BUTTON_SCAN_INTERVAL_MS);
  if (period < 1) period = 1;

  for (;;) {
    self->scan(millis());
    vTaskDelayUntil(&lastWake, period);
  }
}

void ButtonInput::scan(uint32_t nowMs) {
  for (auto &state : states_) {
    updateOne(state, nowMs);
  }
}

void ButtonInput::updateOne(State &state, uint32_t nowMs) {
  const bool raw = digitalRead(state.pin) == LOW;
  if (raw != state.rawPressed) {
    state.rawPressed = raw;
    state.rawChangedMs = nowMs;
  }

  if ((nowMs - state.rawChangedMs) >= DEBOUNCE_MS &&
      raw != state.stablePressed) {
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
  const ButtonEvent event{id, action};

  if (eventQueue_ != nullptr) {
    if (xQueueSend(eventQueue_, &event, 0) == pdTRUE) {
      return;
    }

    // Repeats are expendable. A short or long press is more important, so if
    // the queue is saturated, discard one oldest item and retain the new one.
    if (action == ButtonAction::REPEAT) {
      return;
    }
    ButtonEvent discarded{};
    xQueueReceive(eventQueue_, &discarded, 0);
    xQueueSend(eventQueue_, &event, 0);
    return;
  }

  const uint8_t next = static_cast<uint8_t>(
      (fallbackHead_ + 1U) % FALLBACK_QUEUE_SIZE);
  if (next == fallbackTail_) {
    return;
  }
  fallbackQueue_[fallbackHead_] = event;
  fallbackHead_ = next;
}
