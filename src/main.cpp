//#include "imgui.h"
#include "raylib.hpp"
//#include "rlImGui.h"
#include "Matrix.hpp"
#include "Window.hpp"
#include "Mesh.hpp"
#include "Model.hpp"
#include "Shader.hpp"
#include "Camera3D.hpp"

#include "io/node.hpp"

#include "defines.h"
#include "renderer.h"

#include <vector>
#include <array>
#include <memory>

#include "model/common.h"
#include "model/piece.h"
#include "model/model.h"

#define BASE_PATH "/Users/jack/Documents/Dev/nanoforge/models"
//#define BASE_PATH "../../models"

// https://nanoblocks.fandom.com/wiki/Nanoblocks_Wiki
// https://blockguide.ch/

#define LOG(msg, ...)   printf("[nanoforge] " msg "\n", ##__VA_ARGS__)



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

auto vertShader = R"(
#version 330

layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec3 vertexNormal;
layout(location=2) in mat4 instanceTransform;
layout(location=6) in mat4 colorShades;

uniform mat4 mvp;

out vec3 vNormalWorld;
flat out mat4 vColorShades;

void main()
{
  mat3 normalMatrix = transpose(inverse(mat3(instanceTransform)));
  vNormalWorld = normalize(normalMatrix * vertexNormal);
  vColorShades = colorShades;

  gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);
}
)";

auto fragShader = R"(
#version 330

in vec3 vNormalWorld;
flat in mat4 vColorShades;

uniform float yThreshold; // soglia (0..1). Tipico 0.2-0.4

out vec4 fragColor;

void main()
{
  vec3 normal = normalize(vNormalWorld);
  float isUp = step(yThreshold, normal.y);   // 1 se Y>=soglia, altrimenti 0
  if (isUp > 0.5)
  {
    fragColor = vColorShades[0];
  }
  else
  {      
    fragColor = (normal.x > 0.0) ? vColorShades[2] : vColorShades[1];
  }
}
)";

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


void Data::init()
{
  meshes.cube = raylib::Mesh::Cube(side, height, side);
  meshes.stud = raylib::Mesh::Cylinder(studDiameter / 2.0f, studHeight, 32);

  shaders.flatShading.shader = raylib::Shader::LoadFromMemory(vertShader, fragShader);
  shaders.flatShading.shader.locs[SHADER_LOC_MATRIX_MVP] = shaders.flatShading->GetLocation("mvp");
  shaders.flatShading.locationInstanceTransform = shaders.flatShading->GetLocationAttrib("instanceTransform");
  shaders.flatShading.locationColorShade = shaders.flatShading->GetLocationAttrib("colorShades");

  materials.flatMaterial.shader = shaders.flatShading.shader;

  /* load colors from ../../models/colors.yml */
  auto node = fkyaml::node::deserialize(files::read_as_string(BASE_PATH "/colors.yml"));
  for (const auto& cc : node["colors"].as_seq())
  {
    ident_t id = cc["ident"].as_str();
    std::array<raylib::Color, 4> cols;

    for (size_t i = 0; i < 4; ++i)
    {
      const auto& col = cc["data"][i];
      cols[i] = raylib::Color(col[0].as_int(), col[1].as_int(), col[2].as_int(), col[3].as_int());
    }

    colors[id] = nb::PieceColor(cols);
  }

  colors.lime = &colors["lime"];
  colors.white = &colors["white"];
}

void Data::deinit()
{
  materials.flatMaterial.Unload();
  meshes.cube.Unload();
  meshes.stud.Unload();
}

Data data;

#include <optional>
#include <unordered_set>

class Context;

class InputHandler
{
  enum class MouseButton { Left = 0, Middle, Right };
  
  Context* _context;
  
  std::unordered_set<int> _keyState;
  std::array<bool, 3> _mouseState;
  std::optional<coord3d_t> _hover;

  nb::Model* model;

  void handleKeystate();

public:
  InputHandler(Context* context) : _context(context), _mouseState({ false, false, false }) { }

  void mouseDown(MouseButton button);
  void mouseUp(MouseButton button);

  void keyDown(int key);
  void keyUp(int key);

  void handle(nb::Model* model);

  const auto& hover() const { return _hover; }
};

void InputHandler::handleKeystate()
{
  /* use GetKeyState to insert pressed keys into _keyState or remove them if they're not pressed anymore */
  std::unordered_set<int> newState;
  int c;
  while ((c = GetKeyPressed()))
    newState.insert(c);

  /* trigger events */
  for (int key : _keyState)
  {
    if (newState.find(key) == newState.end())
      keyUp(key);
  }

  for (int key : newState)
  {
    if (_keyState.find(key) == _keyState.end())
      keyDown(key);
  }

  _keyState = newState;
}


static size2d_t LAYER2D_CELL_SIZE = size2d_t(16.0f, 16.0f);
static vec2 LAYER2D_BASE = vec2(10.0f, 10.0f);
static float LAYER2D_SPACING = 10.0f;

void InputHandler::handle(nb::Model* model)
{
  this->model = model;

  handleKeystate();

  vec2 position = GetMousePosition();

  bool any = false;
  for (layer_index_t i = 0; i < model->layerCount(); ++i)
  {
    float y = (gfx::Renderer::MOCK_LAYER_SIZE * LAYER2D_CELL_SIZE.height) * i + (LAYER2D_SPACING * i);
    rect bounds = rect(LAYER2D_BASE.x, LAYER2D_BASE.y + y, gfx::Renderer::MOCK_LAYER_SIZE * LAYER2D_CELL_SIZE.width, gfx::Renderer::MOCK_LAYER_SIZE * LAYER2D_CELL_SIZE.height);

    /* if mouse is inside 2d layer grid */
    if (bounds.CheckCollision(position))
    {
      auto relative = position - bounds.Origin();
      coord2d_t cell = coord2d_t(relative.x / LAYER2D_CELL_SIZE.width, relative.y / LAYER2D_CELL_SIZE.height);
      _hover = coord3d_t(cell, model->lastLayerIndex() - i);
      any = true;
      break;
    }
  }
  
  if (!any)
    _hover.reset();

  /* fetch button state into a new std::array and call relevant methods if state changed */
  std::array<bool, 3> newState = { IsMouseButtonDown(MOUSE_LEFT_BUTTON), IsMouseButtonDown(MOUSE_MIDDLE_BUTTON), IsMouseButtonDown(MOUSE_RIGHT_BUTTON) };
  for (size_t i = 0; i < _mouseState.size(); ++i)
  {
    if (newState[i] != _mouseState[i])
    {
      if (newState[i])
        mouseDown(static_cast<MouseButton>(i));
      else
        mouseUp(static_cast<MouseButton>(i));
    }
  }
  _mouseState = newState;
}

void InputHandler::mouseDown(MouseButton button)
{
  if (button == MouseButton::Left && _hover)
  {
    // add a piece at hover position
    nb::Piece* p = model->piece(*_hover);
    if (p)
      model->remove(p);
  }
}

void InputHandler::mouseUp(MouseButton button)
{

}

void InputHandler::keyUp(int key)
{

}


struct Context
{
  nb::Model model;
  gfx::Renderer renderer;
  InputHandler input;
  nb::Piece brush;
  
  Context() : input(this)
  {
    
  }
};


void InputHandler::keyDown(int key)
{
  if (key == KEY_W)
  {
    _context->brush.derive(_context->brush.size() + size2d_t(1, 0));
  }
  else if (key == KEY_Q)
  {
    _context->brush.derive(_context->brush.size() + size2d_t(-1, 0));
  }
}



class Loader
{
  public:
    std::optional<nb::Model> load(const std::filesystem::path& filename);
};

std::optional<nb::Model> Loader::load(const std::filesystem::path& file)
{
  /* get file length through std::filesystem api */
  auto length = std::filesystem::file_size(file);

  std::ifstream in(file, std::ios::binary);

  std::string yaml;
  yaml.resize(length);
  in.read(yaml.data(), yaml.length());
  in.close();

  auto node = fkyaml::node::deserialize(yaml);

  if (node.is_mapping())
  {
    nb::Model model;

    /* compute layers count */
    int maxZ = 0;
    for (const auto& p : node["pieces"].as_seq())
    {
      int z = p["position"][0].as_int();
      maxZ = std::max(maxZ, z);
    }
    
    LOG("Loading model %s... (%d pieces, %d layers)", node["info"]["name"].as_str().c_str(), node["pieces"].as_seq().size(), maxZ);

    model.prepareLayers(maxZ + 1);

    /* load pieces */
    for (const auto& p : node["pieces"].as_seq())
    {
      int z = p["position"][0].as_int();
      int x = p["position"][1].as_int();
      int y = p["position"][2].as_int();
      const nb::PieceColor* color = data.colors.white;

      size2d_t size = size2d_t(1, 1);

      if (p["size"].is_sequence())
      {
        size.width = p["size"][0].as_int();
        size.height = p["size"][1].as_int();
      }

      if (p["color"].is_string())
      {
        auto it = data.colors.find(p["color"].as_str());
        if (it != data.colors.end())
          color = &it->second;
      }

      model.addPiece(z, nb::Piece(coord2d_t(x, y), color, nb::PieceOrientation::North, size));
    }

    return model;
  }

  return std::optional<nb::Model>();
}

int main(int arg, char* argv[])
{  
  Context context;

  auto& model = context.model;
  gfx::Renderer& renderer = context.renderer;
  InputHandler& input = context.input;

  SetConfigFlags(FLAG_MSAA_4X_HINT);

  InitWindow(1280, 800, "Nanoforge v0.0.1a");

  data.init();
  renderer.init();

  Loader loader;
  auto result = loader.load(BASE_PATH "/test.yml");
  if (result)
    context.model = std::move(*result);

  context.brush = nb::Piece(coord2d_t(0, 0), data.colors.lime, nb::PieceOrientation::North, size2d_t(1, 1));

  renderer.camera().target = { gfx::Renderer::MOCK_LAYER_SIZE * side * 0.5f,  0.0f,  gfx::Renderer::MOCK_LAYER_SIZE * side * 0.5f };
  renderer.camera().position = { renderer.camera().target.x * 4.0f, renderer.camera().target.x * 2.0f, renderer.camera().target.y * 4.0f };
  renderer.camera().up = { 0.0f,  1.0f,  0.0f };
  renderer.camera().fovy = 45.0f;
  renderer.camera().projection = CAMERA_PERSPECTIVE;

  int locThr = data.shaders.flatShading->GetLocation("yThreshold");

  float thr = 0.3f;
  data.shaders.flatShading->SetValue(locThr, &thr, SHADER_UNIFORM_FLOAT);

  //rlImGuiSetup(true);

  SetTargetFPS(60);

  while (!WindowShouldClose())
  {
    UpdateCamera(&renderer.camera(), CAMERA_ORBITAL);

    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(renderer.camera());
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

    renderer.render(&model);

    EndMode3D();

    for (layer_index_t i = 0; i < model.layerCount(); ++i)
    {
      auto idx = model.lastLayerIndex() - i;
      float y = (gfx::Renderer::MOCK_LAYER_SIZE * LAYER2D_CELL_SIZE.height) * i + (LAYER2D_SPACING * i);
      renderer.renderLayerGrid2d(LAYER2D_BASE + vec2(0, y), model.layer(idx), size2d_t(gfx::Renderer::MOCK_LAYER_SIZE, gfx::Renderer::MOCK_LAYER_SIZE), LAYER2D_CELL_SIZE);
    }
   
    if (input.hover())
    {
      /* draw string with coordinate in bottom left corner */
      std::string coordStr = TextFormat("Hover: %d - (%d, %d)", input.hover()->z, input.hover()->x, input.hover()->y);
      DrawText(coordStr.c_str(), 10, GetScreenHeight() - 30, 14, DARKGRAY);
    }

    /*rlImGuiBegin();
    bool open = true;
    ImGui::ShowDemoWindow(&open);
    rlImGuiEnd();*/

    input.handle(&model);

    EndDrawing();
  }

  data.deinit();

  CloseWindow();
  return 0;
}
