#pragma once

#include <Arduino.h>
#include "Config.h"

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
  void update(uint32_t nowMs);
  bool nextEvent(ButtonEvent &event);

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
  static constexpr uint8_t QUEUE_SIZE = 16;

  State states_[3] = {
      {PIN_BUTTON_LEFT, ButtonId::LEFT, false, false, false, 0, 0, 0},
      {PIN_BUTTON_MIDDLE, ButtonId::MIDDLE, false, false, false, 0, 0, 0},
      {PIN_BUTTON_RIGHT, ButtonId::RIGHT, false, false, false, 0, 0, 0},
  };

  ButtonEvent queue_[QUEUE_SIZE];
  uint8_t queueHead_ = 0;
  uint8_t queueTail_ = 0;

  void updateOne(State &state, uint32_t nowMs);
  void push(ButtonId id, ButtonAction action);
};
