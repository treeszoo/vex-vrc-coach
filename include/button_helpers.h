#pragma once

#include "button_tracker.hpp"
#include "pros/misc.h"
#include "pros/misc.hpp"
#include <initializer_list>

/**
 * @file button_helpers.h
 * @brief Controller button handling utilities (header-only).
 */

// Forward declaration for controller (defined in robot_config.h)
extern pros::Controller controller;

/**
 * Button handling helper for toggle actions.
 * Executes action only on new button press (not while held).
 *
 * @param button Controller button to monitor
 * @param tracker ButtonTracker to track press state
 * @param action Function to execute on new press
 */
template <typename ActionFunc>
void handleButtonToggle(pros::controller_digital_e_t button,
                        ButtonTracker &tracker, ActionFunc action) {
  bool controllerPressed = controller.get_digital(button);
  if (controllerPressed) {
    if (!tracker.isPressed()) {
      action(); // Execute the action only on new press
    }
    tracker.press();
  } else {
    tracker.release();
  }
}

/**
 * Button handling helper for multiple buttons with same action.
 * Executes action only when ALL buttons are pressed simultaneously.
 *
 * @param buttons List of controller buttons that must all be pressed
 * @param tracker ButtonTracker to track press state
 * @param action Function to execute on new press
 */
template <typename ActionFunc>
void handleMultiButtonToggle(
    std::initializer_list<pros::controller_digital_e_t> buttons,
    ButtonTracker &tracker, ActionFunc action) {
  bool allButtonsPressed = true;
  for (auto button : buttons) {
    if (!controller.get_digital(button)) {
      allButtonsPressed = false;
      break;
    }
  }

  if (allButtonsPressed) {
    if (!tracker.isPressed()) {
      action(); // Execute the action only on new press
    }
    tracker.press();
  } else {
    tracker.release();
  }
}
