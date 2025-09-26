#pragma once

#include <cstdint>
#include "defines.h"

using layer_index_t = int32_t;
using coord_t = int32_t;

struct coord2d_t
{
  coord_t x;
  coord_t y;
};

struct size2d_t
{
  int32_t width;
  int32_t height;

  size2d_t(int32_t w, int32_t h) : width(w), height(h) { }
};
