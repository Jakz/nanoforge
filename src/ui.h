#pragma once

#include "raylib.h"
#include "context.h"

struct ImVec2;

class UI
{
protected:
  Context* _context;
  
  Texture2D _icons;
  
  std::pair<ImVec2, ImVec2> toUV(const Rectangle& r, const Texture2D& atlas) const;
  
  bool drawToolbarIcon(const char* ident, coord2d_t icon, const char* caption);

public:
  bool _paletteWindowVisible;
  bool _studWindowVisible;
  
public:
  UI(Context* context);
  
  void drawPaletteWindow();
  void drawToolbar();
};
