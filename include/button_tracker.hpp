#pragma once

// Simple per-button edge tracker. Call update() each loop with the
// current raw pressed state. Query pressedEdge() for rising-edge
// detection (new press), releasedEdge() for falling-edge.
class ButtonTracker {
public:
  ButtonTracker() : _isPressed(false) {}

  void press() {
    _isPressed = true;
  }

  void release() {
    _isPressed = false;
  }

  bool isPressed() const { return _isPressed; }

private:
  bool _isPressed;
};