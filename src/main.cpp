//
#include "raylib.hpp"

#include "Matrix.hpp"
#include "Window.hpp"
#include "Mesh.hpp"
#include "Model.hpp"
#include "Shader.hpp"
#include "Camera3D.hpp"

#include "io/node.hpp"

#include "defines.h"
#include "context.h"
#include "renderer.h"
#include "input.h"
#include "ui.h"

#include <vector>
#include <array>
#include <memory>

#include "model/common.h"
#include "model/piece.h"
#include "model/model.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"

// https://nanoblocks.fandom.com/wiki/Nanoblocks_Wiki
// https://blockguide.ch/

size2d_t Data::Constants::LAYER2D_CELL_SIZE = size2d_t(12.0f, 12.0f);
float Data::Constants::LAYER2D_SPACING = 10.0f;


void DrawCylinderSilhouette(const Vector3& center, float r, float h, const Camera3D& cam, Color col) {
  // Direzione di vista proiettata sul piano XZ
  Vector3 v = { cam.position.x - center.x, 0.0f, cam.position.z - center.z };
  float len = std::sqrt(v.x * v.x + v.z * v.z);
  if (len < 1e-5f) {
    // camera sopra il cilindro: fallback a due direzioni fisse
    v = { 1.0f, 0.0f, 0.0f };
    len = 1.0f;
  }
  else {
    v.x /= len; v.z /= len;
  }

  // Versore tangente (perpendicolare in XZ): ruota v di +90� nel piano XZ
  Vector3 t = { -v.z, 0.0f, v.x };

  // Punti alle basi delle due generatrici
  Vector3 a0 = { center.x + r * t.x, center.y,         center.z + r * t.z };
  Vector3 a1 = { center.x + r * t.x, center.y + h,     center.z + r * t.z };
  Vector3 b0 = { center.x - r * t.x, center.y,         center.z - r * t.z };
  Vector3 b1 = { center.x - r * t.x, center.y + h,     center.z - r * t.z };

  //DrawLine3D(a0, a1, col);
  //DrawLine3D(b0, b1, col);

  DrawLineEx(GetWorldToScreen(a0, cam), GetWorldToScreen(b0, cam), 5.0f, col);
}

constexpr float side = 3.8f;   // lato
constexpr float height = 3.1f;
constexpr float studHeight = 1.4f;
constexpr float studDiameter = 2.5f;

#include <filesystem>
#include <fstream>

struct files
{
  static std::string read_as_string(const std::filesystem::path& path)
  {
    auto length = std::filesystem::file_size(path);

    std::ifstream in(path, std::ios::binary);

    std::string yaml;
    yaml.resize(length);
    in.read(yaml.data(), yaml.length());
    in.close();

    return yaml;
  }
};


Data::Data(Context* context) : _context(context)
{
  /* load colors from ../../models/colors.yml */
  auto node = fkyaml::node::deserialize(files::read_as_string(_context->prefs.basePath + "/colors.yml"));
  for (const auto& cc : node["colors"].as_seq())
  {
    ident_t id = cc["ident"].as_str();
    std::array<raylib::Color, 4> cols;

    for (size_t i = 0; i < 4; ++i)
    {
      const auto& col = cc["data"][i];
      cols[i] = raylib::Color(col[0].as_int(), col[1].as_int(), col[2].as_int(), col[3].as_int());
    }

    colors[id] = nb::PieceColor(id, cols);
  }

  colors.lime = &colors["lime"];
  colors.white = &colors["white"];
}

#include <optional>
#include <unordered_set>

#include "model/undo.h"
#include "io/loader.h"
#include "ui.h"

Context::Context() :
  model(std::make_unique<nb::Model>()),
  renderer(std::make_unique<gfx::Renderer>(this)),
  input(std::make_unique<InputHandler>(this)),
  brush(std::make_unique<nb::Piece>(nb::Piece())),
  ui(std::make_unique<UI>(this)),
  loader(std::make_unique<Loader>(this)),
  data(std::make_unique<Data>(this)),
  history(std::make_unique<undo::History>(model.get()))
{

}

void Context::loadModel(const std::string& path)
{
  auto result = loader->load(path);
  if (result)
    *model = std::move(*result);
}

int main(int arg, char* argv[])
{
  SetConfigFlags(FLAG_MSAA_4X_HINT);
  InitWindow(1280, 800, "Nanoforge v0.0.1a");

  Context context;

  auto& model = context.model;
  gfx::Renderer* renderer = context.renderer.get();
  InputHandler* input = context.input.get();


  renderer->init();

  context.loadModel(context.prefs.basePath + "/model.yml");
  context.brush.reset(new nb::Piece(coord2d_t(0, 0), context.data->colors.lime, nb::PieceOrientation::North, nb::PieceType::Square, size2d_t(1, 1)));

  renderer->camera().target = { model->size().width * side * 0.5f,  model->layerCount() * height * 0.5f,  model->size().height * side * 0.5f};
  renderer->camera().position = { renderer->camera().target.x * 4.0f, renderer->camera().target.y * 2.0f, renderer->camera().target.y *4.0f };
  renderer->camera().up = { 0.0f,  1.0f,  0.0f };
  renderer->camera().fovy = 60.0f;
  renderer->camera().projection = CAMERA_PERSPECTIVE;

  rlImGuiSetup(true);
  ImGui::StyleColorsLight();

  SetTargetFPS(60);

  while (!WindowShouldClose() && !context.shouldExit)
  {
    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(renderer->camera());
    //DrawGrid(20, 10.0f);

    /*
    data.models.cube.Draw(pos, raylib::Vector3(), 0.0f, raylib::Vector3(1.0f, 1.0f, 1.0f), WHITE);

    // Pass 2: wireframe bordi (sovrapposto)
    Matrix world = MakeDrawTransform(pos, scale, rot, data.models.cube);
    // centro locale del box � (0,0,0) per GenMeshCube
    DrawCubeEdgesFast({ 0, 0, 0 }, side, height, side, world, lime.edge());

    {
      data.models.stud.Draw({ 0, height, 0 }, raylib::Vector3(), 0.0f, raylib::Vector3(1.0f, 1.0f, 1.0f), WHITE);
      DrawCylinderWireframe({ 0, 0, 0 }, studDiameter / 2.0f, studHeight, 32, lime.edge(), MatrixIdentity(), cam);
    }
    */

    renderer->render(model.get());

    EndMode3D();

    for (auto it = renderer->_topDown.begin(); it != renderer->_topDown.end(); ++it)
    {
      auto idx = it.index();
      if (idx >= 0)
      {
        float y = (model->size().height * Data::Constants::LAYER2D_CELL_SIZE.height) * it.relative() + (Data::Constants::LAYER2D_SPACING * it.relative());
        renderer->renderLayerGrid2d(context.prefs.gridTopPosition() + vec2(0, y), model->layer(idx), model->size(), Data::Constants::LAYER2D_CELL_SIZE);
      }
    }

    if (input->hover())
    {
      /* draw string with coordinate in bottom left corner */
      std::string coordStr = TextFormat("Hover: %d - (%2.2f, %2.2f)", input->hover()->z, input->hover()->x(), input->hover()->y());
      DrawText(coordStr.c_str(), 10, GetScreenHeight() - 30, 14, DARKGRAY);
    }

    if (context.prefs.ui.drawUI)
    {
      rlImGuiBegin();

      context.ui->draw();

      ImGuiIO& io = ImGui::GetIO();
      bool blockMouse = io.WantCaptureMouse;
      bool blockKeyboard = io.WantCaptureKeyboard;

      rlImGuiEnd();

      if (!blockMouse && !blockKeyboard)
        input->handle(model.get());
    }
    else
      input->handle(model.get());


    //UpdateCamera(&renderer->camera(), CAMERA_ORBITAL);

    EndDrawing();
  }

  renderer->deinit();

  context.loader->save(model.get(), context.prefs.basePath + "/model.yml");

  CloseWindow();
  return 0;
}
