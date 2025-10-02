#pragma once

#include "raylib.h"
#include "context.h"

class UI
{
protected:
  Context* _context;
  
  Texture2D _icons;

public:
  bool _paletteWindowVisible;
  bool _studWindowVisible;
  
public:
  UI(Context* context);
  
  void drawPaletteWindow();
  void drawToolbar();
};
