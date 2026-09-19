#pragma once

#include "pros/adi.hpp"

class PneumaticActuator {
private:
  pros::adi::DigitalOut actuator;
  bool deployed = false;
  bool reverse = false;

public:
  PneumaticActuator(int port) : actuator(pros::adi::DigitalOut(port)) {}
  PneumaticActuator(int port, bool reverse) : actuator(pros::adi::DigitalOut(port)) {
    this->reverse = reverse;
  }
  void deploy() {
    actuator.set_value(reverse ? false : true);
    deployed = true;
  }

  void retract() {
    actuator.set_value(reverse ? true : false);
    deployed = false;
  }

  void toggle() {
    deployed = !deployed;
    actuator.set_value(reverse ? !deployed : deployed);
  }

  bool isDeployed() { return deployed; }
};