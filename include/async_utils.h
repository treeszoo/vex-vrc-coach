#pragma once

#include "pros/rtos.hpp"
#include <functional>

/**
 * @file async_utils.h
 * @brief Async task utilities (header-only).
 */

/**
 * Run a function asynchronously with optional delay
 *
 * @param func Function to execute
 * @param delayInMs Delay before execution (ms)
 * @return Task handle
 */
inline pros::Task runAsync(std::function<void()> func, int delayInMs = 0) {
  return pros::Task([func, delayInMs]() {
    if (delayInMs > 0) {
      pros::delay(delayInMs);
    }
    func();
  });
}
