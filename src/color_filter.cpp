#include "color_filter.h"
#include "robot_config.h"
#include "roller_control.h"
#include "pros/rtos.hpp"

using namespace pros;

bool is_red() {
  return ((optical.get_hue() > RED_LOWER_BOUND) &&
          (optical.get_hue() < RED_UPPER_BOUND));
}

bool is_blue() {
  return ((optical.get_hue() > BLUE_LOWER_BOUND) &&
          (optical.get_hue() < BLUE_UPPER_BOUND));
}

bool should_reject_block(TeamColor ourTeam) {
  // Check if a block is present (proximity > threshold means object is close)
  if (optical.get_proximity() < 25) {
    return false; // No block detected, don't reject
  }

  // If we're RED team, reject BLUE blocks
  // If we're BLUE team, reject RED blocks
  if (ourTeam == TeamColor::RED) {
    return is_blue();
  } else {
    return is_red();
  }
}

bool colorFilteredDelay(int ms, const AutonConfig *config) {
  // If no config or color sensor disabled, just do normal delay
  if (!config || !config->useColorSensor) {
    delay(ms);
    return false; // Not interrupted
  }

  const int checkInterval = 3; // check frequency
  int elapsed = 0;

  while (elapsed < ms) {
    if (should_reject_block(config->teamColor)) {
      hold_intake(); // Stop intake immediately
      return true;   // EXIT EARLY - opponent block detected!
    }
    delay(checkInterval);
    elapsed += checkInterval;
  }
  return false; // Completed normally
}

bool shakeCustomWithColorFilter(int count, int backupSpeed, int backupTimeout,
                                int forwardSpeed, int forwardTimeout,
                                const AutonConfig *config) {
  for (int i = 0; i < count; i++) {
    chassis.arcade(-backupSpeed, 0);
    if (colorFilteredDelay(backupTimeout, config)) {
      chassis.arcade(0, 0); // Stop chassis
      return true;          // Exit early
    }

    chassis.arcade(forwardSpeed, 0);
    if (colorFilteredDelay(forwardTimeout, config)) {
      chassis.arcade(0, 0); // Stop chassis
      return true;          // Exit early
    }
  }
  return false; // Completed all iterations
}
