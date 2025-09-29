#pragma once

#include <cstdint>
#include <string>
#include "defines.h"

using layer_index_t = int32_t;
using coord_t = int32_t;

struct coord2d_t
{
  coord_t x;
  coord_t y;

  coord2d_t(coord_t x = 0, coord_t y = 0) : x(x), y(y) { }
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

  size2d_t(int32_t w, int32_t h) : width(w), height(h) { }
  
  size2d_t operator+(const size2d_t& size) { return size2d_t(width + size.width, height + size.height); }
};
