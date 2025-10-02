#pragma once

#include "defines.h"

#include <memory>

namespace gfx
{
  class Renderer;
}

class UI;
class InputHandler;
class Loader;

struct Preferences
{
  struct
  {
    bool drawHoverOnAllLayers = true;
    struct
    {
      float height = 36.0f;
      float buttonSize = 24.0f;

    } toolbar;

    struct
    {
      vec2 marginFromTop = vec2(10.0f, 10.0f);


    } grid;
  } ui;
  
  std::string basePath;

  Preferences()
  {
#ifdef __APPLE__
    basePath = "/Users/jack/Documents/Dev/nanoforge/models";
#else
    basePath = "../../models";
#endif
    
  }
  vec2 gridTopPosition() const { return ui.grid.marginFromTop + vec2(0, ui.toolbar.height); }
};

struct Context
{
  Preferences prefs;

  std::unique_ptr<nb::Model> model;
  std::unique_ptr<gfx::Renderer> renderer;
  std::unique_ptr<InputHandler> input;
  std::unique_ptr<nb::Piece> brush;
  std::unique_ptr<UI> ui;
  std::unique_ptr<Data> data;
  std::unique_ptr<Loader> loader;

  Context();
};
