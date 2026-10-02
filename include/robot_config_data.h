#pragma once

/**
 * @file robot_config_data.h
 * @brief Shared configuration types for VEX VRC robot
 *
 * This header is shared between the Configure UI application and the
 * Competition application to ensure type compatibility for configuration
 * data stored on the SD card.
 */

/**
 * Autonomous mode selection enum
 */
enum class AutonomousMode {
  ALLIANCE_LEFT = 0,   // Left side
  ALLIANCE_RIGHT = 1,  // Right side
  ALLIANCE_SOLO = 2,   // Solo AWP
  SKILLS = 3           // Skills challenge
};

/**
 * Team color enum
 */
enum class TeamColor { RED = 0, BLUE = 1 };

/**
 * Robot configuration structure
 * Groups all configurable parameters together
 */
struct RobotConfig {
  AutonomousMode autonMode = AutonomousMode::ALLIANCE_LEFT;
  TeamColor teamColor = TeamColor::RED;
  bool useColorSensor = false; // Disabled by default for safety
};

/**
 * Autonomous configuration passed to auton functions
 * Contains runtime parameters needed during autonomous routines
 */
struct AutonConfig {
  TeamColor teamColor = TeamColor::RED;
  bool useColorSensor = false;
  bool aggressive = false;
  bool pushAlliance = false;
  bool tank_mode = false;
  bool arcade_mode = false;
};

// Configuration file path on SD card
constexpr const char *CONFIG_FILE_PATH = "/usd/config.txt";
constexpr const char *CONFIG_PARAMETER_FILE_PATH = "/usd/config_parameters.txt";

// Default configuration values (fallback)
namespace ConfigDefaults {
constexpr AutonomousMode AUTON_MODE = AutonomousMode::ALLIANCE_LEFT;
constexpr TeamColor TEAM_COLOR = TeamColor::RED;
constexpr bool USE_COLOR_SENSOR = false; // Disabled for safety
constexpr bool AGGRESSIVE = false;
constexpr bool PUSH_ALLIANCE = false;
constexpr bool ARCADE_MODE = false;
constexpr bool TANK_MODE = false;


} // namespace ConfigDefaults

/**
 * Helper function to get auton mode display name
 * @param mode The autonomous mode
 * @return Human-readable name for the mode
 */
inline const char *getAutonName(AutonomousMode mode) {
  switch (mode) {
  case AutonomousMode::ALLIANCE_LEFT:
    return "LEFT";
  case AutonomousMode::ALLIANCE_RIGHT:
    return "RIGHT";
  case AutonomousMode::ALLIANCE_SOLO:
    return "SOLO AWP";
  case AutonomousMode::SKILLS:
    return "SKILLS";
  default:
    return "UNKNOWN";
  }
}

/**
 * Helper function to get team color display name
 * @param color The team color
 * @return Human-readable name for the color
 */
inline const char *getColorName(TeamColor color) {
  switch (color) {
  case TeamColor::RED:
    return "RED";
  case TeamColor::BLUE:
    return "BLUE";
  default:
    return "UNKNOWN";
  }
}

/**
 * Check if an auton mode value is valid
 * @param value Integer value to check
 * @return true if valid, false otherwise
 */
inline bool isValidAutonMode(int value) { return value >= 0 && value <= 3; }

/**
 * Check if a team color value is valid
 * @param value Integer value to check
 * @return true if valid, false otherwise
 */
inline bool isValidTeamColor(int value) { return value == 0 || value == 1; }


