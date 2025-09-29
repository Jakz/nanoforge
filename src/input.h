#pragma once

#include "defines.h"

#include "model/common.h"

#include <unordered_set>
#include <optional>
#include <array>

class InputHandler
{
  enum class MouseButton { Left = 0, Middle, Right };

  Context* _context;

  std::unordered_set<int> _keyState;
  std::array<bool, 3> _mouseState;
  std::optional<coord3d_t> _hover;

  nb::Model* model;

  void handleKeystate();

public:
  InputHandler(Context* context) : _context(context), _mouseState({ false, false, false }) { }

  void mouseDown(MouseButton button);
  void mouseUp(MouseButton button);

  void keyDown(int key);
  void keyUp(int key);

  void handle(nb::Model* model);

  const auto& hover() const { return _hover; }
};
