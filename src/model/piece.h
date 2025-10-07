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

  enum class PieceType
  {
    Square,
    Round
  };

  enum class StudMode
  {
    None = 0,
    Centered,
    Full
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

  struct PieceCoord
  {
    static constexpr coord_t DENOMINATOR = 100;
    
    coord2d_t whole;
    coord2d_t fraction;
    
    float fx() const { return whole.x + fraction.x / float(DENOMINATOR); }
    float fy() const { return whole.y + fraction.y / float(DENOMINATOR); }
    operator vec2() const { return vec2(fx(), fy()); }
    
    PieceCoord(float fx, float fy)
    {
      whole = coord2d_t{ int(fx), int(fy) };
      fraction = coord2d_t{ int((fx - whole.x) * DENOMINATOR),
                            int((fy - whole.y) * DENOMINATOR) };
    }
    
    PieceCoord(coord2d_t coord) : whole(coord), fraction(0, 0) { }
    PieceCoord(coord_t x, coord_t y) : whole(x, y), fraction(0, 0) { }
    PieceCoord(coord_t x, coord_t fx, coord_t y, coord_t fy) :
      whole(x, y), fraction(fx, fy) { normalize(); }
    
    
    void normalize()
    {
      while (fraction.x >= 100) { ++whole.x; fraction.x -= DENOMINATOR; }
      while (fraction.y >= 100) { ++whole.y; fraction.y -= DENOMINATOR; }
      while (fraction.x < 0) { --whole.x; fraction.x += DENOMINATOR; }
      while (fraction.y < 0) { --whole.y; fraction.y += DENOMINATOR; }
    }
        
    PieceCoord& operator+=(coord2d_t coord)
    {
      whole += coord; return *this;
    }
    
    PieceCoord& operator+=(const PieceCoord& other)
    {
      whole += other.whole;
      fraction += other.fraction;
      normalize();
      return *this;
    }
  };

  using piece_type_t = std::string;

  class Piece
  {
    const PieceColor* _color;
    PieceOrientation _orientation;
    PieceType _type;
    StudMode _studs;
    PieceCoord _coord;
    size2d_t _size;

  public:
    Piece() : _coord(0, 0), _color(nullptr), _orientation(PieceOrientation::North), _size(1, 1) { }
    Piece(PieceCoord coord, const PieceColor* color, PieceOrientation orientation, PieceType type = PieceType::Square, size2d_t size = size2d_t(1, 1), StudMode studs = StudMode::Full) :
      _coord(coord), _color(color), _orientation(orientation), _type(type), _size(size), _studs(studs) { }
    
    void resize(size2d_t size) { _size = size; }
    void swapSize() { _size = size2d_t(_size.height, _size.width); }

    void moveAt(PieceCoord coord) { _coord = coord; }
    void moveBy(PieceCoord delta) { _coord += delta; }

    void moveBy(coord_t x, coord_t y) { _coord += coord2d_t(x, y); }


    void dye(const PieceColor* color) { _color = color; }
    void setStuds(StudMode studs) { _studs = studs; }
    void setType(PieceType type) { _type = type; }

    Piece derive(size2d_t size) const
    {
      Piece other;
      other._color = _color;
      other._orientation = _orientation;
      other._coord = _coord;
      other._size = size;
      return other;
    }

    const PieceCoord& coord() const { return _coord; }
    coord_t fx() const { return _coord.fx(); }
    coord_t fy() const { return _coord.fy(); }
    const PieceColor* color() const { return _color; }
    size2d_t size() const { return _size; }
    PieceType type() const { return _type; }
    StudMode studs() const { return _studs; }

    int32_t width() const { return _size.width; }
    int32_t height() const { return _size.height; }
    
    
  };
}
