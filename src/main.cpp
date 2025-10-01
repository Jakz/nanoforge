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

#include <vector>
#include <array>
#include <memory>

#include "model/common.h"
#include "model/piece.h"
#include "model/model.h"

#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"

#ifdef __APPLE__
#define BASE_PATH "/Users/jack/Documents/Dev/nanoforge/models"
#else
#define BASE_PATH "../../models"
#endif

// https://nanoblocks.fandom.com/wiki/Nanoblocks_Wiki
// https://blockguide.ch/

#define LOG(msg, ...)   printf("[nanoforge] " msg "\n", ##__VA_ARGS__)


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


void Data::init()
{
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
  fkyaml::node root = { { "pieces", fkyaml::node::sequence() }, { "info", fkyaml::node::mapping() } };
  root["info"]["name"] = model->info().name;
  
  auto& pieces = root["pieces"].as_seq();

  for (const auto& layer : model->layers())
  {
    for (const auto& piece : layer->pieces())
    {
      fkyaml::node node = {
        { "position", fkyaml::node::sequence({ layer->index(), piece.coord().x, piece.coord().y}) },
        { "size", fkyaml::node::sequence({ piece.width(), piece.height() }) },
        { "color", piece.color()->ident }
      };

      if (piece.type() == nb::PieceType::Round)
        node["type"] = "round";
      

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
  if (!std::filesystem::exists(file))
    return nb::Model("Model");
  
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
    model.info().name = node["info"]["name"].as_str();

    /* load pieces */
    for (const auto& p : node["pieces"].as_seq())
    {
      int z = p["position"][0].as_int();
      int x = p["position"][1].as_int();
      int y = p["position"][2].as_int();
      const nb::PieceColor* color = data.colors.white;
      nb::PieceType type = nb::PieceType::Square;

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

      if (p["type"].is_string())
      {
        if (p["type"] == "round")
          type = nb::PieceType::Round;
      }

      model.addPiece(z, nb::Piece(coord2d_t(x, y), color, nb::PieceOrientation::North, type, size));
    }

    return model;
  }

  return std::optional<nb::Model>();
}


static inline ImVec4 ToImVec4(Color c) {
  return ImVec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
}

const nb::PieceColor* ImGuiPaletteWindow(const char* title,
  const std::vector<const nb::PieceColor*>& colors,
  int columns,
  const nb::PieceColor* selected = nullptr,
  float cellSize = 28.0f,
  float cellRounding = 4.0f,
  float cellSpacing = 6.0f)
{
  if (columns < 1) columns = 1;

  int clickedIndex = -1;

  if (ImGui::Begin(title)) {
    // Layout: griglia compatta
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(cellSpacing, cellSpacing));

    for (int i = 0; i < (int)colors.size(); ++i) {
      ImGui::PushID(i);

      ImVec4 col = ToImVec4(colors[i]->top());

      ImGuiColorEditFlags flags =
        ImGuiColorEditFlags_NoTooltip |
        ImGuiColorEditFlags_NoDragDrop |
        ImGuiColorEditFlags_NoAlpha;

      // Forziamo la dimensione del bottone colore
      ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, cellRounding);
      ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(0, 0));

      // Hitbox invisibile con draw manuale del bordo selezione
      ImGui::InvisibleButton("cell", ImVec2(cellSize, cellSize));
      bool hovered = ImGui::IsItemHovered();
      bool pressed = ImGui::IsItemClicked();

      // Disegno del quad colorato
      ImDrawList* dl = ImGui::GetWindowDrawList();
      ImVec2 p0 = ImGui::GetItemRectMin();
      ImVec2 p1 = ImGui::GetItemRectMax();
      ImU32 ucol = ImGui::GetColorU32(col);

      dl->AddRectFilled(p0, p1, ucol, cellRounding);

      bool isSelected = (selected && selected == colors[i]);

      if (hovered || isSelected)
      {
        dl->AddRect(p0, p1, ImGui::GetColorU32(ToImVec4(colors[i]->edge())), cellRounding, 0, hovered ? 8.0f : 6.0f);
      }
      else
      {
        // sottile bordo scuro per separare le celle
        dl->AddRect(p0, p1, ImGui::GetColorU32(ImVec4(0, 0, 0, 0.35f)), cellRounding, 0, 1.0f);
      }

      // Tooltip
      if (hovered)
      {
        const Color& c = colors[i]->top();
        ImGui::BeginTooltip();
        ImGui::Text("%s - RGB(%d, %d, %d)", colors[i]->ident.c_str(), c.r, c.g, c.b);
        ImGui::EndTooltip();
      }

      if (pressed) {
        clickedIndex = i;
      }

      ImGui::PopStyleVar(2);

      // Gestione colonne (same-line tranne a fine riga)
      int colIdx = (i % columns);
      if (colIdx != columns - 1)
        ImGui::SameLine();

      ImGui::PopID();
    }

    ImGui::PopStyleVar(); // ItemSpacing
  }
  ImGui::End();

  return clickedIndex >= 0 ? colors[clickedIndex] : nullptr;
}

static bool IconButton(const char* id, ImTextureID tex, const ImVec2& uv0, const ImVec2& uv1,
  const ImVec2& size, bool enabled = true, const char* tooltip = nullptr)
{
  ImGui::PushStyleVar(ImGuiStyleVar_FrameBorderSize, 0.0f);    // no bordo
  ImGui::PushStyleVar(ImGuiStyleVar_FrameRounding, 4.0f);      // angoli arrotondati
  ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, ImVec2(2, 2));// padding interno ridotto
  
  if (!enabled) 
    ImGui::PushStyleVar(ImGuiStyleVar_Alpha, ImGui::GetStyle().Alpha * 0.5f);

  bool pressed = ImGui::ImageButton(id, tex, size, uv0, uv1, ImVec4(0, 0, 0, 0), ImVec4(1, 1, 1, 1)) && enabled;

  if (tooltip && ImGui::IsItemHovered(ImGuiHoveredFlags_DelayNormal))
    ImGui::SetTooltip("%s", tooltip);

  if (!enabled)
    ImGui::PopStyleVar();

  ImGui::PopStyleVar(3);

  return pressed;
}

void DrawToolbar(Context* _context, Texture2D atlas, Rectangle uvNew, Rectangle uvOpen, Rectangle uvSave,
  Rectangle uvPlay, Rectangle uvPause, Rectangle uvStop,
  bool canSave, bool isPlaying)
{
  ImGuiIO& io = ImGui::GetIO();
  // finestra a tutta larghezza, senza bordi
  ImGui::SetNextWindowPos(ImVec2(0, 0), ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(io.DisplaySize.x, _context->prefs.ui.toolbar.height));
  ImGuiWindowFlags flags = ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoResize |
    ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoScrollbar |
    ImGuiWindowFlags_NoSavedSettings;
  ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(8, 4));
  ImGui::Begin("Toolbar", nullptr, flags);

  auto toUV = [&](Rectangle r) {
    ImVec2 uv0(r.x / atlas.width, r.y / atlas.height);
    ImVec2 uv1((r.x + r.width) / atlas.width, (r.y + r.height) / atlas.height);
    return std::pair{ uv0, uv1 };
    };
  auto [uv0New, uv1New] = toUV(uvNew);
  auto [uv0Open, uv1Open] = toUV(uvOpen);
  auto [uv0Save, uv1Save] = toUV(uvSave);
  auto [uv0Play, uv1Play] = toUV(uvPlay);
  auto [uv0Pause, uv1Pause] = toUV(uvPause);
  auto [uv0Stop, uv1Stop] = toUV(uvStop);

  ImTextureID tex = (ImTextureID)(intptr_t)atlas.id;
  ImVec2 iconSize(_context->prefs.ui.toolbar.buttonSize, _context->prefs.ui.toolbar.buttonSize);

  if (IconButton("##new", tex, uv0New, uv1New, iconSize, true, "New (Ctrl+N)")) {/*...*/ }
  ImGui::SameLine();
  if (IconButton("##open", tex, uv0Open, uv1Open, iconSize, true, "Open (Ctrl+O)")) {/*...*/ }
  ImGui::SameLine();
  if (IconButton("##save", tex, uv0Save, uv1Save, iconSize, canSave, "Save (Ctrl+S)")) {/*...*/ }

  // separatore
  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // se toggle play è attivo, mostra pause/stop, altrimenti play
  if (!isPlaying) {
    if (IconButton("##play", tex, uv0Play, uv1Play, iconSize, true, "Play (Space)")) {/* start */ }
  }
  else {
    if (IconButton("##pause", tex, uv0Pause, uv1Pause, iconSize, true, "Pause (Space)")) {/* pause */ }
    ImGui::SameLine();
    if (IconButton("##stop", tex, uv0Stop, uv1Stop, iconSize, true, "Stop (Shift+Space)")) {/* stop */ }
  }

  // scorciatoie da tastiera
  bool ctrl = io.KeyCtrl;
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) {/* new */ }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {/* open */ }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S) && canSave) {/* save */ }
  if (ImGui::IsKeyPressed(ImGuiKey_Space)) {/* play/pause toggle */ }
  if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Space)) {/* stop */ }

  ImGui::End();
  ImGui::PopStyleVar();
}

int main(int arg, char* argv[])
{  
  Context context;

  auto& model = context.model;
  gfx::Renderer* renderer = context.renderer.get();
  InputHandler* input = context.input.get();

  SetConfigFlags(FLAG_MSAA_4X_HINT);

  InitWindow(1280, 800, "Nanoforge v0.0.1a");

  auto icons = LoadTexture(BASE_PATH "/icons.png");

  data.init();
  renderer->init();

  Loader loader;
  auto result = loader.load(BASE_PATH "/model.yml");
  if (result)
    *context.model = std::move(*result);

  context.brush.reset(new nb::Piece(coord2d_t(0, 0), data.colors.lime, nb::PieceOrientation::North, nb::PieceType::Square, size2d_t(1, 1)));

  renderer->camera().target = { gfx::Renderer::MOCK_LAYER_SIZE * side * 0.5f,  0.0f,  gfx::Renderer::MOCK_LAYER_SIZE * side * 0.5f };
  renderer->camera().position = { renderer->camera().target.x * 4.0f, renderer->camera().target.x * 2.0f, renderer->camera().target.y * 4.0f };
  renderer->camera().up = { 0.0f,  1.0f,  0.0f };
  renderer->camera().fovy = 45.0f;
  renderer->camera().projection = CAMERA_PERSPECTIVE;

  rlImGuiSetup(true);
  ImGui::StyleColorsLight();

  SetTargetFPS(60);

  while (!WindowShouldClose())
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
        float y = (gfx::Renderer::MOCK_LAYER_SIZE * Data::Constants::LAYER2D_CELL_SIZE.height) * it.relative() + (Data::Constants::LAYER2D_SPACING * it.relative());
        renderer->renderLayerGrid2d(context.prefs.gridTopPosition() + vec2(0, y), model->layer(idx), size2d_t(gfx::Renderer::MOCK_LAYER_SIZE, gfx::Renderer::MOCK_LAYER_SIZE), Data::Constants::LAYER2D_CELL_SIZE);
      }
    }

    if (input->hover())
    {
      /* draw string with coordinate in bottom left corner */
      std::string coordStr = TextFormat("Hover: %d - (%d, %d)", input->hover()->z, input->hover()->x, input->hover()->y);
      DrawText(coordStr.c_str(), 10, GetScreenHeight() - 30, 14, DARKGRAY);
    }

    rlImGuiBegin();
    bool open = true;
    
    std::vector<const nb::PieceColor*> colors;
    for (const auto& c : data.colors)
      colors.push_back(&c.second);
    auto newSelection = ImGuiPaletteWindow("Palette", colors, 5, context.brush->color());
    if (newSelection)
      context.brush->dye(newSelection);

    DrawToolbar(&context, icons, Rectangle{0, 0, 64, 64}, Rectangle{64, 0, 64, 64}, Rectangle{128, 0, 64, 64},
                Rectangle{196, 0, 64, 64}, Rectangle{64 * 4, 0, 64, 64}, Rectangle{64 * 5, 0, 64, 64}, true, false);

    ImGuiIO& io = ImGui::GetIO();
    bool blockMouse = io.WantCaptureMouse;
    bool blockKeyboard = io.WantCaptureKeyboard;

    rlImGuiEnd();

    if (!blockMouse && !blockKeyboard)
      input->handle(model.get());

    UpdateCamera(&renderer->camera(), CAMERA_ORBITAL);

    EndDrawing();
  }

  renderer->deinit();

  loader.save(model.get(), BASE_PATH "/model.yml");

  CloseWindow();
  return 0;
}
