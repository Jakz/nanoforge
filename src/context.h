#pragma once

#include "defines.h"

#include <memory>

namespace gfx
{
  class Renderer;
}

class InputHandler;

struct Context
{
  std::unique_ptr<nb::Model> model;
  std::unique_ptr<gfx::Renderer> renderer;
  std::unique_ptr<InputHandler> input;
  std::unique_ptr<nb::Piece> brush;

  Context();
};