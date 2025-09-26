#pragma once

#include <cstdint>
#include <array>

#include "common.h"

#include "Color.hpp"

namespace nb
{
  enum class PieceOrientation
  {
    North = 0x01, East = 0x02, South = 0x04, West = 0x08
  };

  struct PieceColor
  {
    std::array<raylib::Color, 4> colors;

    PieceColor(const std::array<raylib::Color, 4>& cols) : colors(cols) { }

    const raylib::Color& top() const { return colors[0]; }
    const raylib::Color& left() const { return colors[1]; }
    const raylib::Color& right() const { return colors[2]; }
    const raylib::Color& edge() const { return colors[3]; }

    Vector3 topV() const { return Vector3{ top().r / 255.0f, top().g / 255.0f, top().b / 255.0f }; }
    Vector3 leftV() const { return Vector3{ left().r / 255.0f, left().g / 255.0f, left().b / 255.0f }; }
    Vector3 rightV() const { return Vector3{ right().r / 255.0f, right().g / 255.0f, right().b / 255.0f }; }
  };

  class Piece
  {
    PieceColor _color;
    PieceOrientation _orientation;
    coord2d_t _coord;
    size2d_t _size;

  public:
    Piece(coord2d_t coord, const PieceColor* color, PieceOrientation orientation, size2d_t size = size2d_t(1, 1)) :
      _coord(coord), _color(*color), _orientation(orientation), _size(size) { }

    coord2d_t coord() const { return _coord; }
    coord_t x() const { return _coord.x; }
    coord_t y() const { return _coord.y; }
    const PieceColor& color() const { return _color; }

    int32_t width() const { return _size.width; }
    int32_t height() const { return _size.height; }
  };
}