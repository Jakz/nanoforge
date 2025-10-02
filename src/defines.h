#pragma once

#include "Vector2.hpp"
#include "Vector3.hpp"
#include "Rectangle.hpp"
#include "Color.hpp"

using vec2 = raylib::Vector2;
using vec3 = raylib::Vector3;
using rect = raylib::Rectangle;
using color = raylib::Color;

using ident_t = std::string;

#include "model/common.h"

namespace nb
{
  class PieceColor;
  class Layer;
  class Model;
  class Piece;
}

#include <map>
#include "Shader.hpp"
#include "Material.hpp"
#include "Mesh.hpp"

struct FlatShader
{
  raylib::ShaderUnmanaged shader;
  unsigned int locationInstanceTransform;
  unsigned int locationColorShade;
  
  raylib::ShaderUnmanaged* operator->() { return &shader; }
};

class Context;

struct Data
{
  Context* _context;
  
  struct Constants
  {
    static constexpr float side = 3.8f;
    static constexpr float height = 3.1f;
    static constexpr float studHeight = 1.4f;
    static constexpr float studDiameter = 2.5f;

    static size2d_t LAYER2D_CELL_SIZE;
    static float LAYER2D_SPACING;
  };

  struct Colors : public std::map<ident_t, nb::PieceColor>
  {
    const nb::PieceColor* lime;
    const nb::PieceColor* white;
  } colors;

  Data(Context* context);
};

