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
#include "context.h"
#include "renderer.h"
#include "input.h"

#include <vector>
#include <array>
#include <memory>

#include "model/common.h"
#include "model/piece.h"
#include "model/model.h"

//#define BASE_PATH "/Users/jack/Documents/Dev/nanoforge/models"
#define BASE_PATH "../../models"

// https://nanoblocks.fandom.com/wiki/Nanoblocks_Wiki
// https://blockguide.ch/

#define LOG(msg, ...)   printf("[nanoforge] " msg "\n", ##__VA_ARGS__)


size2d_t Data::Constants::LAYER2D_CELL_SIZE = size2d_t(16.0f, 16.0f);
vec2 Data::Constants::LAYER2D_BASE = vec2(10.0f, 10.0f);
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

    colors[id] = nb::PieceColor(id, cols);
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


Context::Context() : 
  model(std::make_unique<nb::Model>()), 
  renderer(std::make_unique<gfx::Renderer>(this)), 
  input(std::make_unique<InputHandler>(this)), 
  brush(std::make_unique<nb::Piece>(nb::Piece()))
{

}

class Loader
{
  public:
    std::optional<nb::Model> load(const std::filesystem::path& filename);
    void save(const nb::Model* model, const std::filesystem::path& filename);
};

void Loader::save(const nb::Model* model, const std::filesystem::path& filename)
{
  fkyaml::node root = { { "pieces", fkyaml::node::sequence() } };
  auto& pieces = root["pieces"].as_seq();

  for (const auto& layer : model->layers())
  {
    for (const auto& piece : layer->pieces())
    {
      fkyaml::node node = {
        "position", fkyaml::node::sequence({ layer->index(), piece.coord().x, piece.coord().y}) ,
        "size", fkyaml::node::sequence({ piece.width(), piece.height() }) ,
        "color", piece.color()->ident
      };

      pieces.emplace_back(std::move(node));
    }
  }
  
  std::string yaml = fkyaml::node::serialize(root);

  std::ofstream out(filename, std::ios::binary);
  out.write(yaml.data(), yaml.length());
  out.close();
}

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
  gfx::Renderer* renderer = context.renderer.get();
  InputHandler* input = context.input.get();

  SetConfigFlags(FLAG_MSAA_4X_HINT);

  InitWindow(1280, 800, "Nanoforge v0.0.1a");

  data.init();
  renderer->init();

  Loader loader;
  auto result = loader.load(BASE_PATH "/test.yml");
  if (result)
    *context.model = std::move(*result);

  context.brush.reset(new nb::Piece(coord2d_t(0, 0), data.colors.lime, nb::PieceOrientation::North, size2d_t(1, 1)));

  renderer->camera().target = { gfx::Renderer::MOCK_LAYER_SIZE * side * 0.5f,  0.0f,  gfx::Renderer::MOCK_LAYER_SIZE * side * 0.5f };
  renderer->camera().position = { renderer->camera().target.x * 4.0f, renderer->camera().target.x * 2.0f, renderer->camera().target.y * 4.0f };
  renderer->camera().up = { 0.0f,  1.0f,  0.0f };
  renderer->camera().fovy = 45.0f;
  renderer->camera().projection = CAMERA_PERSPECTIVE;

  int locThr = data.shaders.flatShading->GetLocation("yThreshold");

  float thr = 0.3f;
  data.shaders.flatShading->SetValue(locThr, &thr, SHADER_UNIFORM_FLOAT);

  //rlImGuiSetup(true);

  SetTargetFPS(60);

  while (!WindowShouldClose())
  {
    UpdateCamera(&renderer->camera(), CAMERA_ORBITAL);

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

    for (layer_index_t i = 0; i < model->layerCount(); ++i)
    {
      auto idx = model->lastLayerIndex() - i;
      float y = (gfx::Renderer::MOCK_LAYER_SIZE * Data::Constants::LAYER2D_CELL_SIZE.height) * i + (Data::Constants::LAYER2D_SPACING * i);
      renderer->renderLayerGrid2d(Data::Constants::LAYER2D_BASE + vec2(0, y), model->layer(idx), size2d_t(gfx::Renderer::MOCK_LAYER_SIZE, gfx::Renderer::MOCK_LAYER_SIZE), Data::Constants::LAYER2D_CELL_SIZE);
    }
   
    if (input->hover())
    {
      /* draw string with coordinate in bottom left corner */
      std::string coordStr = TextFormat("Hover: %d - (%d, %d)", input->hover()->z, input->hover()->x, input->hover()->y);
      DrawText(coordStr.c_str(), 10, GetScreenHeight() - 30, 14, DARKGRAY);
    }

    /*rlImGuiBegin();
    bool open = true;
    ImGui::ShowDemoWindow(&open);
    rlImGuiEnd();*/

    input->handle(model.get());

    EndDrawing();
  }

  data.deinit();

  loader.save(model.get(), BASE_PATH "/test_out.yml");

  CloseWindow();
  return 0;
}
