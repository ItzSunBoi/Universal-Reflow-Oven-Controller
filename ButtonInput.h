#pragma once

#include <Arduino.h>
#include "Config.h"

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"

enum class ButtonId : uint8_t {
  LEFT,
  MIDDLE,
  RIGHT,
};

enum class ButtonAction : uint8_t {
  SHORT_PRESS,
  LONG_PRESS,
  REPEAT,
};

struct ButtonEvent {
  ButtonId button;
  ButtonAction action;
};

class ButtonInput {
 public:
  void begin();

  // Cooperative fallback used only if the dedicated FreeRTOS scanner task
  // could not be created. Normally this returns immediately.
  void service(uint32_t nowMs);

  bool nextEvent(ButtonEvent &event);
  bool asynchronous() const { return taskRunning_; }

 private:
  struct State {
    uint8_t pin;
    ButtonId id;
    bool rawPressed;
    bool stablePressed;
    bool longSent;
    uint32_t rawChangedMs;
    uint32_t pressedMs;
    uint32_t lastRepeatMs;
  };

  static constexpr uint32_t DEBOUNCE_MS = 30;
  static constexpr uint32_t LONG_PRESS_MS = 650;
  static constexpr uint32_t REPEAT_START_MS = 850;
  static constexpr uint32_t REPEAT_INTERVAL_MS = 120;
  static constexpr uint8_t FALLBACK_QUEUE_SIZE = 16;

  State states_[3] = {
      {PIN_BUTTON_LEFT, ButtonId::LEFT, false, false, false, 0, 0, 0},
      {PIN_BUTTON_MIDDLE, ButtonId::MIDDLE, false, false, false, 0, 0, 0},
      {PIN_BUTTON_RIGHT, ButtonId::RIGHT, false, false, false, 0, 0, 0},
  };

  QueueHandle_t eventQueue_ = nullptr;
  TaskHandle_t taskHandle_ = nullptr;
  volatile bool taskRunning_ = false;

  // Used only if queue allocation fails. In that case no background task is
  // started, so this small ring remains single-threaded.
  ButtonEvent fallbackQueue_[FALLBACK_QUEUE_SIZE];
  uint8_t fallbackHead_ = 0;
  uint8_t fallbackTail_ = 0;

  static void taskEntry(void *argument);
  void scan(uint32_t nowMs);
  void updateOne(State &state, uint32_t nowMs);
  void push(ButtonId id, ButtonAction action);
};
