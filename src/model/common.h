#pragma once

#include <cstdint>
#include <string>
#include <numeric>

#include "Vector2.hpp"

using layer_index_t = int32_t;
using coord_t = int32_t;

enum class Direction { North, East, South, West };

struct coord2d_t
{
  coord_t x;
  coord_t y;

  coord2d_t(coord_t x = 0, coord_t y = 0) : x(x), y(y) { }
  explicit coord2d_t(const Vector2& v) : x(coord_t(v.x)), y(coord_t(v.y)) { }

  coord2d_t operator-(const coord2d_t& c) const { return coord2d_t(x - c.x, y - c.y); }
  coord2d_t operator+(const coord2d_t& c) const { return coord2d_t(x + c.x, y + c.y); }
  coord2d_t operator+=(const coord2d_t& c) { x += c.x; y += c.y; return *this; }

  coord2d_t operator-() const { return coord2d_t(-x, -y); }
};

struct coord3d_t
{
  coord_t x;
  coord_t y;
  layer_index_t z;

  coord3d_t(coord2d_t c, layer_index_t l) : x(c.x), y(c.y), z(l) { }
 
  coord2d_t xy() const { return coord2d_t(x, y); }
};

struct size2d_t
{
  int32_t width;
  int32_t height;

  size2d_t() : width(0), height(0) { }
  size2d_t(int32_t w, int32_t h) : width(w), height(h) { }

  Vector2 operator*(float v) const { return Vector2(width * v, height * v); }
  
  size2d_t operator+(const size2d_t& size) const { return size2d_t(width + size.width, height + size.height); }
  size2d_t operator-(const size2d_t& size) const { return size2d_t(width - size.width, height - size.height); }
};

struct bounds2d_t
{
protected:
  coord2d_t _min;
  coord2d_t _max;

public:
  bounds2d_t() : _min(std::numeric_limits<coord_t>::max(), std::numeric_limits<coord_t>::max()),
    _max(std::numeric_limits<coord_t>::min(), std::numeric_limits<coord_t>::min())
  { };

  bounds2d_t(const coord2d_t& min, const coord2d_t& max) : _min(min), _max(max) { }

  bounds2d_t& operator+=(const coord2d_t& c)
  {
    if (c.x < _min.x) _min.x = c.x;
    if (c.y < _min.y) _min.y = c.y;
    if (c.x > _max.x) _max.x = c.x;
    if (c.y > _max.y) _max.y = c.y;
    return *this;
  }

  bounds2d_t& operator+=(const bounds2d_t& b)
  {
    operator+=(b._min);
    operator+=(b._max);
    return *this;
  }

  coord2d_t min() const { return _min; }
  coord2d_t max() const { return _max; }

  size2d_t size() const
  { 
    if (_min.x > _max.x || _min.y > _max.y)
      return size2d_t(0, 0);

    return size2d_t(_max.x - _min.x + 1, _max.y - _min.y + 1);
  }
};
