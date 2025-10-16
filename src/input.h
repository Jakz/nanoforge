#pragma once

#include "defines.h"

#include "model/common.h"
#include "model/piece.h"

#include <unordered_set>
#include <optional>
#include <array>

class InputHandler
{
  enum class MouseButton { Left = 0, Middle, Right };
  enum class DragStatus { Start, Change, End, None };

  Context* _context;

  std::unordered_set<int> _keyState;
  
  std::array<bool, 3> _mouseState;
  vec2 _mousePosition;
  DragStatus _dragStatus;

  std::optional<nb::PieceCoord3d> _hover;

  nb::Model* model;

  void handleKeystate();

public:
  InputHandler(Context* context) : _context(context), _mouseState({ false, false, false }), _dragStatus(DragStatus::None) { }

  void mouseDown(MouseButton button);
  void mouseUp(MouseButton button);
  void mouseDrag(const vec2& position, DragStatus status, MouseButton button);

  void keyDown(int key);
  void keyUp(int key);

  void handle(nb::Model* model);

  const auto& hover() const { return _hover; }
};
