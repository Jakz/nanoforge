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

#include <vector>
#include <array>
#include <memory>

#include "model/common.h"
#include "model/piece.h"
#include "model/model.h"

// https://nanoblocks.fandom.com/wiki/Nanoblocks_Wiki
// https://blockguide.ch/

#define LOG(msg, ...)   printf("[nanoforge] " msg "\n", ##__VA_ARGS__)


#define SHADER_LOC_VERTEX_INSTANCE_TX (SHADER_LOC_BONE_MATRICES + 1)
#define SHADER_LOC_COLOR_SHADE (SHADER_LOC_VERTEX_INSTANCE_TX + 4)

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

uniform vec3 colorUp;     // colore per facce "verso l'alto"
uniform vec3 colorRight;   // colore per tutte le altre
uniform vec3 colorLeft;
uniform float yThreshold; // soglia (0..1). Tipico 0.2-0.4

out vec4 fragColor;

void main()
{
  float isUp = step(yThreshold, vNormalWorld.y);   // 1 se Y>=soglia, altrimenti 0
  if (isUp > 0.5)
  {
    fragColor = vColorShades[0];
  }
  else
  {      
    fragColor = (vNormalWorld.x > 0.0) ? vColorShades[2] : vColorShades[1];
  }
}
)";

// Replica la trasform di DrawModel: T(pos) * R(rot) * S(scale) * model.transform
static inline Matrix MakeDrawTransform(Vector3 pos, float scale, Matrix rot, const raylib::Matrix& modelMatrix) {
  Matrix S = MatrixScale(scale, scale, scale);
  Matrix TS = MatrixMultiply(S, modelMatrix);          // S * model.transform
  Matrix T = MatrixTranslate(pos.x, pos.y, pos.z);
  return MatrixMultiply(T, TS);                            // T * (S * model.transform)
}

// Disegna i 12 bordi del cubo dato centro e dimensioni (in local space) + trasform finale
static inline void DrawCubeEdgesFast(float w, float h, float d, const Matrix& world, Color col)
{
  const float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;

  // 8 vertici in local space
  Vector3 v[8] = {
      {- hw, - hh, - hd},
      {+ hw, - hh, - hd},
      {+ hw, + hh, - hd},
      {- hw, + hh, - hd},
      {- hw, - hh, + hd},
      {+ hw, - hh, + hd},
      {+ hw, + hh, + hd},
      {- hw, + hh, + hd}
  };

  // indici dei 12 spigoli
  const int e[12][2] = {
      {0,1},{1,2},{2,3},{3,0}, // back (Z-)
      {4,5},{5,6},{6,7},{7,4}, // front (Z+)
      {0,4},{1,5},{2,6},{3,7}  // verticali
  };

  // trasforma e disegna
  for (int i = 0; i < 12; ++i) {
    Vector3 a = Vector3Transform(v[e[i][0]], world);
    Vector3 b = Vector3Transform(v[e[i][1]], world);
    DrawLine3D(a, b, col);
    DrawCylinderEx(a, b, 0.02f, 0.02f, 8, col);
  }
}

// Disegna wireframe cilindro verticale (asse Y)
void DrawCylinderWireframe(Vector3 center, float radius, float height, int segments, Color col, const Matrix& world, const Camera3D& cam)
{
  float y0 = center.y;
  float y1 = center.y + height;

  // --- cerchio top e bottom ---
  for (int i = 0; i < segments; i++) {
    float a0 = (2 * PI * i) / segments;
    float a1 = (2 * PI * (i + 1)) / segments;
    Vector3 p0 = { center.x + radius * cosf(a0), y0, center.z + radius * sinf(a0) };
    Vector3 p1 = { center.x + radius * cosf(a1), y0, center.z + radius * sinf(a1) };
    Vector3 q0 = { center.x + radius * cosf(a0), y1, center.z + radius * sinf(a0) };
    Vector3 q1 = { center.x + radius * cosf(a1), y1, center.z + radius * sinf(a1) };

    DrawCylinderEx(p0, p1, 0.04f, 0.04f, 8, col); // bottom circle
    DrawCylinderEx(q0, q1, 0.04f, 0.04f, 8, col); // top circle
  }

  // --- due generatrici silhouette (a destra/sinistra rispetto alla camera) ---
  Vector3 viewDir = Vector3Normalize(Vector3Subtract(cam.position, center));
  // proietta in XZ
  Vector2 v = { viewDir.x, viewDir.z };
  float len = sqrtf(v.x * v.x + v.y * v.y);
  if (len < 1e-5f) { v = { 1,0 }; len = 1; }
  v.x /= len; v.y /= len;
  // perpendicolare in XZ
  Vector2 t = { -v.y, v.x };

  Vector3 a0 = { center.x + radius * t.x, y0, center.z + radius * t.y };
  Vector3 a1 = { center.x + radius * t.x, y1, center.z + radius * t.y };
  Vector3 b0 = { center.x - radius * t.x, y0, center.z - radius * t.y };
  Vector3 b1 = { center.x - radius * t.x, y1, center.z - radius * t.y };

  DrawCylinderEx(a0, a1, 0.04f, 0.04f, 8, col);
  DrawCylinderEx(b0, b1, 0.04f, 0.04f, 8, col);
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

struct Data
{
  struct Constants
  {
    static constexpr float side = 3.8f;
    static constexpr float height = 3.1f;
    static constexpr float studHeight = 1.4f;
    static constexpr float studDiameter = 2.5f;
  };

  struct Shaders
  {
    raylib::ShaderUnmanaged flatShading;
  } shaders;

  struct Materials
  {
    raylib::Material flatMaterial;
  } materials;

  struct Meshes
  {
    raylib::Mesh cube;
    raylib::Mesh stud;
  } meshes;

  struct Colors : public std::unordered_map<ident_t, nb::PieceColor>
  {
    const nb::PieceColor* lime;
    const nb::PieceColor* white;
  } colors;

  void init();
  void deinit();
};

void Data::init()
{
  meshes.cube = raylib::Mesh::Cube(side, height, side);
  meshes.stud = raylib::Mesh::Cylinder(studDiameter / 2.0f, studHeight, 32);

  shaders.flatShading = raylib::Shader::LoadFromMemory(vertShader, fragShader);
  shaders.flatShading.locs[SHADER_LOC_MATRIX_MVP] = shaders.flatShading.GetLocation("mvp");
  shaders.flatShading.locs[SHADER_LOC_VERTEX_INSTANCE_TX] = shaders.flatShading.GetLocationAttrib("instanceTransform");
  shaders.flatShading.locs[SHADER_LOC_COLOR_SHADE] = shaders.flatShading.GetLocationAttrib("colorShades");

  materials.flatMaterial.shader = shaders.flatShading;

  /* load colors from ../../models/colors.yml */
  auto node = fkyaml::node::deserialize(files::read_as_string("../../models/colors.yml"));
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

class InputHandler
{
  enum class MouseButton { Left = 0, Middle, Right };
  
  std::unordered_set<int> _keyState;
  std::array<bool, 3> _mouseState;
  std::optional<coord3d_t> _hover;

  nb::Model* model;

  void handleKeystate();

public:
  InputHandler() : _mouseState({ false, false, false }) { }

  void mouseDown(MouseButton button);
  void mouseUp(MouseButton button);

  void keyDown(int key);
  void keyUp(int key);

  void handle(nb::Model* model);

  const auto& hover() const { return _hover; }
};

namespace gfx
{
  class Renderer
  {
  public:
    struct InstanceData
    {
      Matrix matrix;
      const nb::PieceColor* color;
    };

  protected:
    raylib::Camera3D _camera;
    std::vector<InstanceData> _studData;

  public:
    static constexpr int EDGE_COMPLEXITY = 6;
    static constexpr int MOCK_LAYER_SIZE = 10;

    void render(const nb::Model* model);

    auto& camera() { return _camera; }

  protected:

    void renderLayerGrid3d(layer_index_t index, size2d_t size);
    void renderLayer(const nb::Layer* layer);
    void renderModel(const nb::Model* model);
    void renderStuds();

  public:
    void renderLayerGrid2d(vec2 base, const nb::Layer* layer, size2d_t layerSize, size2d_t cellSize)
    {
      /* draw a thin black grid with half opacity over the pieces */
      for (int x = 0; x <= layerSize.width; ++x)
      {
        vec2 p0 = vec2(base.x + x * cellSize.width, base.y);
        vec2 p1 = vec2(base.x + x * cellSize.width, base.y + layerSize.height * cellSize.height);
        DrawLineV(p0, p1, color(0, 0, 0, 100));
      }

      for (int y = 0; y <= layerSize.height; ++y)
      {
        vec2 p0 = vec2(base.x, base.y + y * cellSize.height);
        vec2 p1 = vec2(base.x + layerSize.width * cellSize.width, base.y + y * cellSize.height);
        DrawLineV(p0, p1, color(0, 0, 0, 100));
      }

      /* draw pieces of layer below with half opacity */
      auto prev = layer->prev();
      if (prev)
      {
        for (const nb::Piece& piece : prev->pieces())
        {
          vec2 pos = vec2(base.x + piece.x() * cellSize.width, base.y + piece.y() * cellSize.height);
          vec2 size = vec2(piece.width() * cellSize.width, piece.height() * cellSize.height);
          DrawRectangleV(pos, size, piece.color()->top().Fade(0.5f));
          DrawRectangleLinesEx(rect(pos.x, pos.y, size.x, size.y), 1.0f, piece.color()->edge().Fade(0.8f));
        }
      }
      
      /* draw pieces as rect with outline using piece color */
      for (const nb::Piece& piece : layer->pieces())
      {
        vec2 pos = vec2(base.x + piece.x() * cellSize.width, base.y + piece.y() * cellSize.height);
        vec2 size = vec2(piece.width() * cellSize.width, piece.height() * cellSize.height);

        DrawRectangleV(pos, size, piece.color()->top());
        DrawRectangleLinesEx(rect(pos.x, pos.y, size.x, size.y), 2.0f, piece.color()->edge());
      }


    }
  };
}

void MyDrawMeshInstanced(const Mesh& mesh, const Material& material, const gfx::Renderer::InstanceData* transforms, int instances);


void gfx::Renderer::render(const nb::Model* model)
{
  _studData.clear();
  renderModel(model);
  renderStuds();
}

void gfx::Renderer::renderLayerGrid3d(layer_index_t index, size2d_t size)
{
  for (int x = 0; x < size.width; ++x)
  {
    /* draw vertical lines using DrawCylinderEx */
    Vector3 p0 = { x * side, index * height, 0.0f };
    Vector3 p1 = { x * side, index * height, (size.height - 1) * side };
    DrawCylinderEx(p0, p1, 0.02f, 0.02f, EDGE_COMPLEXITY, raylib::Color(80, 80, 80, 100));
  }

  for (int y = 0; y < size.height; ++y)
  {
    /* draw horizontal lines using DrawCylinderEx */
    Vector3 p0 = { 0.0f, index * height, y * side };
    Vector3 p1 = { (size.width - 1) * side, index * height, y * side };
    DrawCylinderEx(p0, p1, 0.02f, 0.02f, EDGE_COMPLEXITY, raylib::Color(80, 80, 80, 100));
  }
}

void gfx::Renderer::renderLayer(const nb::Layer* layer)
{
  /* compute the matrix for the layer */
  raylib::Matrix layerTransform = raylib::Matrix::Translate(0.0f, layer->index() * height, 0.0f);

  std::vector<InstanceData> transforms(layer->pieces().size());

  size_t i = 0;
  for (const nb::Piece& piece : layer->pieces())
  {
    /* translate inside layer according to position */
    raylib::Matrix pieceTransform = raylib::Matrix::Translate((piece.x() + piece.width() * 0.5f) * side, height * 0.5f, (piece.y() + piece.height() * 0.5f) * side);

    for (int y = 0; y < piece.height(); ++y)
      for (int x = 0; x < piece.width(); ++x)
      {
        _studData.push_back({ layerTransform * raylib::Matrix::Translate((piece.x() + x + 0.5f) * side, height, (piece.y() + y + 0.5f) * side), piece.color() });

        raylib::Vector3 center = raylib::Vector3::Zero();
        center = center.Transform(_studData.back().matrix);

        DrawCylinderWireframe(center, studDiameter / 2.0f, studHeight, 32, piece.color()->edge(), MatrixIdentity(), _camera);
      }


    pieceTransform = raylib::Matrix::Scale(piece.width(), 1.0f, piece.height()) * pieceTransform;
 
    /* combine layer and piece matrix */
    transforms[i] = { layerTransform * pieceTransform, piece.color() };
    ++i;

    DrawCubeEdgesFast(side, height, side, transforms[i - 1].matrix, piece.color()->edge());
  }

  MyDrawMeshInstanced(data.meshes.cube, data.materials.flatMaterial, transforms.data(), transforms.size());
}

void gfx::Renderer::renderStuds()
{
  if (_studData.empty())
    return;

  MyDrawMeshInstanced(data.meshes.stud, data.materials.flatMaterial, _studData.data(), _studData.size());

  _studData.clear();
}

void gfx::Renderer::renderModel(const nb::Model* model)
{
  for (const auto& layer : model->layers())
    renderLayer(layer.get());

  renderLayerGrid3d(0, size2d_t(MOCK_LAYER_SIZE, MOCK_LAYER_SIZE));
}

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

void InputHandler::keyDown(int key)
{
  if (key == KEY_W)
  {
    printf("down!");
  }
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
};




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

  Loader loader;
  auto result = loader.load("../../models/test.yml");
  if (result)
    context.model = std::move(*result);

  context.brush = nb::Piece(coord2d_t(0, 0), data.colors.lime, nb::PieceOrientation::North, size2d_t(1, 1));

  renderer.camera().target = { gfx::Renderer::MOCK_LAYER_SIZE * side * 0.5f,  0.0f,  gfx::Renderer::MOCK_LAYER_SIZE * side * 0.5f };
  renderer.camera().position = { renderer.camera().target.x * 4.0f, renderer.camera().target.x * 2.0f, renderer.camera().target.y * 4.0f };
  renderer.camera().up = { 0.0f,  1.0f,  0.0f };
  renderer.camera().fovy = 45.0f;
  renderer.camera().projection = CAMERA_PERSPECTIVE;

  // Uniform palette
  int locUp = data.shaders.flatShading.GetLocation("colorUp");
  int locLeft = data.shaders.flatShading.GetLocation("colorLeft");
  int locRight = data.shaders.flatShading.GetLocation("colorRight");
  int locThr = data.shaders.flatShading.GetLocation("yThreshold");

  float thr = 0.3f; 

  auto top = data.colors.lime->topV();
  auto left = data.colors.lime->leftV();
  auto right = data.colors.lime->rightV();
  data.shaders.flatShading.SetValue(locUp, &top, SHADER_UNIFORM_VEC3);
  data.shaders.flatShading.SetValue(locLeft, &left, SHADER_UNIFORM_VEC3);
  data.shaders.flatShading.SetValue(locRight, &right, SHADER_UNIFORM_VEC3);
  data.shaders.flatShading.SetValue(locThr, &thr, SHADER_UNIFORM_FLOAT);

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


void MyDrawMeshInstanced(const Mesh& mesh, const Material& material, const gfx::Renderer::InstanceData* data, int instances)
{
  constexpr size_t MAX_MATERIAL_MAPS = 4;

#if defined(GRAPHICS_API_OPENGL_33) || defined(GRAPHICS_API_OPENGL_ES2)
  // Instancing required variables
  float16* instanceTransforms = NULL;
  unsigned int instancesVboId = 0;
  unsigned int colorsVboId = 0;

  // Bind shader program
  rlEnableShader(material.shader.id);

  // Send required data to shader (matrices, values)
  //-----------------------------------------------------
  // Upload to shader material.colDiffuse
  if (material.shader.locs[SHADER_LOC_COLOR_DIFFUSE] != -1)
  {
    float values[4] = {
        (float)material.maps[MATERIAL_MAP_DIFFUSE].color.r / 255.0f,
        (float)material.maps[MATERIAL_MAP_DIFFUSE].color.g / 255.0f,
        (float)material.maps[MATERIAL_MAP_DIFFUSE].color.b / 255.0f,
        (float)material.maps[MATERIAL_MAP_DIFFUSE].color.a / 255.0f
    };

    rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_DIFFUSE], values, SHADER_UNIFORM_VEC4, 1);
  }

  // Upload to shader material.colSpecular (if location available)
  if (material.shader.locs[SHADER_LOC_COLOR_SPECULAR] != -1)
  {
    float values[4] = {
        (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.r / 255.0f,
        (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.g / 255.0f,
        (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.b / 255.0f,
        (float)material.maps[SHADER_LOC_COLOR_SPECULAR].color.a / 255.0f
    };

    rlSetUniform(material.shader.locs[SHADER_LOC_COLOR_SPECULAR], values, SHADER_UNIFORM_VEC4, 1);
  }

  Matrix matModel = MatrixIdentity();
  Matrix matView = rlGetMatrixModelview();
  Matrix matModelView = MatrixIdentity();
  Matrix matProjection = rlGetMatrixProjection();

  // Upload view and projection matrices (if locations available)
  if (material.shader.locs[SHADER_LOC_MATRIX_VIEW] != -1)
    rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_VIEW], matView);

  if (material.shader.locs[SHADER_LOC_MATRIX_PROJECTION] != -1)
    rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_PROJECTION], matProjection);

  // Create instances buffer
  instanceTransforms = (float16*)RL_MALLOC(instances * sizeof(float16));

  // Fill buffer with instances transformations as float16 arrays
  for (int i = 0; i < instances; i++)
    instanceTransforms[i] = MatrixToFloatV(data[i].matrix);

  // Enable mesh VAO to attach new buffer
  rlEnableVertexArray(mesh.vaoId);

  // This could alternatively use a static VBO and either glMapBuffer() or glBufferSubData()
  // It isn't clear which would be reliably faster in all cases and on all platforms,
  // anecdotally glMapBuffer() seems very slow (syncs) while glBufferSubData() seems
  // no faster, since we're transferring all the transform matrices anyway
  instancesVboId = rlLoadVertexBuffer(instanceTransforms, instances * sizeof(float16), false);

  for (unsigned int i = 0; i < 4; i++)
  {
    auto baseLocation = material.shader.locs[SHADER_LOC_VERTEX_INSTANCE_TX];
    rlEnableVertexAttribute(baseLocation + i);
    rlSetVertexAttribute(baseLocation + i, 4, RL_FLOAT, 0, sizeof(float16), i * sizeof(Vector4));
    rlSetVertexAttributeDivisor(baseLocation + i, 1);
  }

  float16* colorShades = (float16*)RL_MALLOC(instances * sizeof(float16));
  for (int i = 0; i < instances; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      colorShades[i].v[j * 4 + 0] = data[i].color->colors[j].r / 255.0f;
      colorShades[i].v[j * 4 + 1] = data[i].color->colors[j].g / 255.0f;
      colorShades[i].v[j * 4 + 2] = data[i].color->colors[j].b / 255.0f;
      colorShades[i].v[j * 4 + 3] = data[i].color->colors[j].a / 255.0f;
    }
  }

  colorsVboId = rlLoadVertexBuffer(colorShades, instances * sizeof(float16), false);

  for (unsigned int i = 0; i < 4; i++)
  {
    auto baseLocation = material.shader.locs[SHADER_LOC_COLOR_SHADE];
    rlEnableVertexAttribute(baseLocation + i);
    rlSetVertexAttribute(baseLocation + i, 4, RL_FLOAT, 0, sizeof(float16), i * sizeof(Vector4));
    rlSetVertexAttributeDivisor(baseLocation + i, 1);
  }

  rlDisableVertexBuffer();
  rlDisableVertexArray();

  // Accumulate internal matrix transform (push/pop) and view matrix
  // NOTE: In this case, model instance transformation must be computed in the shader
  matModelView = MatrixMultiply(rlGetMatrixTransform(), matView);

  // Upload model normal matrix (if locations available)
  if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1)
    rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(matModel)));

  // Bind active texture maps (if available)
  for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
  {
    if (material.maps[i].texture.id > 0)
    {
      // Select current shader texture slot
      rlActiveTextureSlot(i);

      // Enable texture for active slot
      if ((i == MATERIAL_MAP_IRRADIANCE) ||
        (i == MATERIAL_MAP_PREFILTER) ||
        (i == MATERIAL_MAP_CUBEMAP)) rlEnableTextureCubemap(material.maps[i].texture.id);
      else rlEnableTexture(material.maps[i].texture.id);

      rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
    }
  }

  // Try binding vertex array objects (VAO)
  // or use VBOs if not possible
  if (!rlEnableVertexArray(mesh.vaoId))
  {
    rlEnableVertexBuffer(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_POSITION]);
    rlSetVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION], 3, RL_FLOAT, 0, 0, 0);
    rlEnableVertexAttribute(material.shader.locs[SHADER_LOC_VERTEX_POSITION]);

    if (mesh.indices != NULL)
      rlEnableVertexBufferElement(mesh.vboId[RL_DEFAULT_SHADER_ATTRIB_LOCATION_INDICES]);
  }

  // Calculate model-view-projection matrix (MVP)
  Matrix matModelViewProjection = MatrixIdentity();
  matModelViewProjection = MatrixMultiply(matModelView, matProjection);

  // Send combined model-view-projection matrix to shader
  rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

  // Draw mesh instanced
  if (mesh.indices != NULL)
    rlDrawVertexArrayElementsInstanced(0, mesh.triangleCount * 3, 0, instances);
  else
    rlDrawVertexArrayInstanced(0, mesh.vertexCount, instances);

  // Unbind all bound texture maps
  for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
  {
    if (material.maps[i].texture.id > 0)
    {
      // Select current shader texture slot
      rlActiveTextureSlot(i);

      // Disable texture for active slot
      if ((i == MATERIAL_MAP_IRRADIANCE) ||
        (i == MATERIAL_MAP_PREFILTER) ||
        (i == MATERIAL_MAP_CUBEMAP)) rlDisableTextureCubemap();
      else rlDisableTexture();
    }
  }

  // Disable all possible vertex array objects (or VBOs)
  rlDisableVertexArray();
  rlDisableVertexBuffer();
  rlDisableVertexBufferElement();

  // Disable shader program
  rlDisableShader();

  // Remove instance transforms buffer
  rlUnloadVertexBuffer(instancesVboId);
  RL_FREE(instanceTransforms);

  rlUnloadVertexBuffer(colorsVboId);
  RL_FREE(colorShades);
#endif
}