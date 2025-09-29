#pragma once

#include <cstdint>
#include <array>

#include "common.h"
#include "defines.h"

#include "Color.hpp"

namespace nb
{
  enum class PieceOrientation
  {
    North = 0x01, East = 0x02, South = 0x04, West = 0x08
  };

  struct PieceColor
  {
    ident_t ident;
    std::array<raylib::Color, 4> colors;

    PieceColor() { }
    PieceColor(const ident_t& ident, const std::array<raylib::Color, 4>& cols) : ident(ident), colors(cols) { }

    const raylib::Color& top() const { return colors[0]; }
    const raylib::Color& left() const { return colors[1]; }
    const raylib::Color& right() const { return colors[2]; }
    const raylib::Color& edge() const { return colors[3]; }

    Vector3 topV() const { return Vector3{ top().r / 255.0f, top().g / 255.0f, top().b / 255.0f }; }
    Vector3 leftV() const { return Vector3{ left().r / 255.0f, left().g / 255.0f, left().b / 255.0f }; }
    Vector3 rightV() const { return Vector3{ right().r / 255.0f, right().g / 255.0f, right().b / 255.0f }; }
  };

  using piece_type_t = std::string;

  class Piece
  {
    const PieceColor* _color;
    PieceOrientation _orientation;
    coord2d_t _coord;
    size2d_t _size;

  public:
    Piece() : _coord(0, 0), _color(nullptr), _orientation(PieceOrientation::North), _size(1, 1) { }
    Piece(coord2d_t coord, const PieceColor* color, PieceOrientation orientation, size2d_t size = size2d_t(1, 1)) :
      _coord(coord), _color(color), _orientation(orientation), _size(size) { }
    
    void resize(size2d_t size) { _size = size; }
    void swapSize() { _size = size2d_t(_size.height, _size.width); }

    void moveAt(coord2d_t coord) { _coord = coord; }

    void dye(const PieceColor* color) { _color = color; }

    Piece derive(size2d_t size) const
    {
      Piece other;
      other._color = _color;
      other._orientation = _orientation;
      other._coord = _coord;
      other._size = size;
      return other;
    }

    coord2d_t coord() const { return _coord; }
    coord_t x() const { return _coord.x; }
    coord_t y() const { return _coord.y; }
    const PieceColor* color() const { return _color; }
    size2d_t size() const { return _size; }
    

    int32_t width() const { return _size.width; }
    int32_t height() const { return _size.height; }
    
    
  };
}
