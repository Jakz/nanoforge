#include "ui.h"

#include "raylib.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"

#include "model/piece.h"

#include <optional>

static inline ImVec4 ToImVec4(Color c)
{
  return ImVec4(c.r / 255.f, c.g / 255.f, c.b / 255.f, c.a / 255.f);
}

const nb::PieceColor* ImGuiPaletteWindow(const char* title,
  const std::vector<const nb::PieceColor*>& colors,
  int columns,
  const nb::PieceColor* selected = nullptr,
  bool* visible = nullptr,
  float cellSize = 28.0f,
  float cellRounding = 4.0f,
  float cellSpacing = 6.0f
)
{
  if (columns < 1) columns = 1;

  int clickedIndex = -1;

  if (ImGui::Begin(title, visible, ImGuiWindowFlags_AlwaysAutoResize)) {
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

  //ImGui::NewLine();
  //ImGui::Separator();

  ImGui::End();

  return clickedIndex >= 0 ? colors[clickedIndex] : nullptr;
}



void UI::drawPaletteWindow()
{
  if (!_paletteWindowVisible)
    return;
  
  std::vector<const nb::PieceColor*> colors;
  for (const auto& c : _context->data->colors)
    colors.push_back(&c.second);
  auto newSelection = ImGuiPaletteWindow("Palette", colors, 5, _context->brush->color(), &_paletteWindowVisible);
  if (newSelection)
    _context->brush->dye(newSelection);
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

void UI::drawStudModeWindow()
{
  std::optional<nb::StudMode> result;
  int modeInt = static_cast<int>(_context->brush->studs());

  if (ImGui::Begin("Studs", &_studWindowVisible, ImGuiWindowFlags_AlwaysAutoResize))
  {
    if (ImGui::RadioButton("Full", modeInt == static_cast<int>(nb::StudMode::Full)))
      modeInt = static_cast<int>(nb::StudMode::Full);

    if (ImGui::RadioButton("Centered", modeInt == static_cast<int>(nb::StudMode::Centered)))
      modeInt = static_cast<int>(nb::StudMode::Centered);

    if (ImGui::RadioButton("None", modeInt == static_cast<int>(nb::StudMode::None)))
      modeInt = static_cast<int>(nb::StudMode::None);

    // Se è cambiato rispetto a current → restituisci nuovo valore
    if (modeInt != static_cast<int>(_context->brush->studs()))
      _context->brush->setStuds(static_cast<nb::StudMode>(modeInt));
  }

  ImGui::End();
}

bool UI::drawToolbarIcon(const char* ident, coord2d_t icon, const char* caption) const
{
  constexpr float iconTextureSize = 64.0f;
  ImVec2 iconSize(_context->prefs.ui.toolbar.buttonSize, _context->prefs.ui.toolbar.buttonSize);

  ImTextureID texture = (ImTextureID)(intptr_t)_icons.id;
  
  ImVec2 uv0((icon.x * iconTextureSize) / _icons.width, (icon.y * iconTextureSize) / _icons.height);
  ImVec2 uv1(((icon.x + 1) * iconTextureSize) / _icons.width, ((icon.y + 1) * iconTextureSize) / _icons.height);
  
  return IconButton(ident, texture, uv0, uv1, iconSize, true, caption);
}


UI::UI(Context* context) : _context(context), _paletteWindowVisible(true), _studWindowVisible(true)
{
  _icons = LoadTexture((_context->prefs.basePath + "/icons.png").c_str());
}

void UI::drawToolbar()
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

  if (drawToolbarIcon("##new", coord2d_t(0, 0), "New (Ctrl+N)")) {/*...*/ }
  ImGui::SameLine();
  if (drawToolbarIcon("##open", coord2d_t(1, 0), "Open (Ctrl+O)")) {/*...*/ }
  ImGui::SameLine();

  // separatore
  ImGui::SameLine();
  ImGui::SeparatorEx(ImGuiSeparatorFlags_Vertical);
  ImGui::SameLine();

  // se toggle play è attivo, mostra pause/stop, altrimenti play
  if (drawToolbarIcon("##show-palette", coord2d_t(3, 0), "Show Palette"))
    _paletteWindowVisible = !_paletteWindowVisible;
  ImGui::SameLine();
  if (drawToolbarIcon("##show-stud-mode", coord2d_t(4, 0), "Show Stud Mode"))
    _studWindowVisible = !_studWindowVisible;

  // scorciatoie da tastiera
  bool ctrl = io.KeyCtrl;
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_N)) {/* new */ }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_O)) {/* open */ }
  if (ctrl && ImGui::IsKeyPressed(ImGuiKey_S)) {/* save */ }
  if (ImGui::IsKeyPressed(ImGuiKey_Space)) {/* play/pause toggle */ }
  if (io.KeyShift && ImGui::IsKeyPressed(ImGuiKey_Space)) {/* stop */ }

  ImGui::End();
  ImGui::PopStyleVar();
}

void UI::draw()
{
  drawPaletteWindow();
  
  if (_studWindowVisible)
    drawStudModeWindow();

  drawToolbar();
}