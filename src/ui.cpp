#include "ui.h"

#include "raylib.hpp"

#include "imgui.h"
#include "imgui_internal.h"
#include "rlImGui.h"

#include "model/piece.h"
#include "model/model.h"
#include "model/undo.h"
#include "renderer.h"
#include "instructions.h"

#include <algorithm>
#include <cstring>
#include <optional>
#include <vector>

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


UI::UI(Context* context) :
  _context(context),
  _paletteWindowVisible(true),
  _studWindowVisible(true),
  _instructionExportWindowVisible(false),
  _instructionPiecesPerStep(8),
  _instructionGroupingMode(0),
  _instructionImageMode(0)
{
  _icons = LoadTexture((_context->prefs.basePath + "/icons.png").c_str());
  std::strncpy(_instructionImagePrefix, "step", sizeof(_instructionImagePrefix));
  _instructionImagePrefix[sizeof(_instructionImagePrefix) - 1] = '\0';
}

void UI::drawToolbar()
{
  ImGuiViewport* vp = ImGui::GetMainViewport();
  
  ImGuiIO& io = ImGui::GetIO();
  // finestra a tutta larghezza, senza bordi
  ImGui::SetNextWindowPos(vp->WorkPos, ImGuiCond_Always);
  ImGui::SetNextWindowSize(ImVec2(vp->WorkSize.x, _context->prefs.ui.toolbar.height));
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

void UI::drawViewOptionsWindow()
{
  ImGui::Begin("View Options", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  bool changed = false;

  // Draw Studs checkbox
  if (ImGui::Checkbox("Draw Studs", &_context->prefs.renderer.drawStuds))
    changed = true;

  // Draw Edges checkbox
  if (ImGui::Checkbox("Draw Edges", &_context->prefs.renderer.drawEdges))
    changed = true;

  // (Optional) handle changes, e.g. mark scene dirty or save prefs
  if (changed)
  {
    // Example: _context->renderer->markDirty();
    // or _context->prefs.save();
  }

  ImGui::End();
}


void UI::drawPieceTypeWindow()
{
  using namespace nb;
  ImGui::Begin("Piece Type", nullptr, ImGuiWindowFlags_AlwaysAutoResize);

  const PieceType type = _context->brush->type();

  if (ImGui::RadioButton("Square", type == PieceType::Square))
    _context->brush->setType(PieceType::Square);
  else if (ImGui::RadioButton("Round", type == PieceType::Round))
    _context->brush->setType(PieceType::Round);

  ImGui::End();
}

#include "io/tinyfiledialogs/tinyfiledialogs.h"

namespace
{
  const char* pieceTypeLabel(nb::PieceType type)
  {
    return type == nb::PieceType::Round ? "round" : "square";
  }

  const char* studsLabel(nb::StudMode studs)
  {
    switch (studs)
    {
      case nb::StudMode::None: return "no studs";
      case nb::StudMode::Centered: return "center stud";
      case nb::StudMode::Full: return "full studs";
    }

    return "full studs";
  }

  void drawPiecePreview(const instructions::PieceSummary& summary, const ImVec2& size)
  {
    ImGui::InvisibleButton("piece-preview", size);

    ImDrawList* dl = ImGui::GetWindowDrawList();
    ImVec2 p0 = ImGui::GetItemRectMin();
    ImVec2 p1 = ImGui::GetItemRectMax();
    ImVec2 center((p0.x + p1.x) * 0.5f, (p0.y + p1.y) * 0.5f);

    float maxDim = static_cast<float>(std::max(summary.size.width, summary.size.height));
    float scale = maxDim > 0.0f ? (std::min(size.x, size.y) - 8.0f) / maxDim : 1.0f;
    ImVec2 blockSize(summary.size.width * scale, summary.size.height * scale);
    ImVec2 b0(center.x - blockSize.x * 0.5f, center.y - blockSize.y * 0.5f);
    ImVec2 b1(center.x + blockSize.x * 0.5f, center.y + blockSize.y * 0.5f);

    ImU32 top = ImGui::GetColorU32(ToImVec4(summary.color->top()));
    ImU32 edge = ImGui::GetColorU32(ToImVec4(summary.color->edge()));

    if (summary.type == nb::PieceType::Round && summary.size.width == 1 && summary.size.height == 1)
    {
      dl->AddCircleFilled(center, blockSize.x * 0.5f, top, 24);
      dl->AddCircle(center, blockSize.x * 0.5f, edge, 24, 2.0f);
    }
    else
    {
      dl->AddRectFilled(b0, b1, top, summary.type == nb::PieceType::Round ? blockSize.y * 0.5f : 3.0f);
      dl->AddRect(b0, b1, edge, summary.type == nb::PieceType::Round ? blockSize.y * 0.5f : 3.0f, 0, 2.0f);
    }

    if (summary.studs != nb::StudMode::None)
    {
      const int studsX = summary.studs == nb::StudMode::Centered ? 1 : summary.size.width;
      const int studsY = summary.studs == nb::StudMode::Centered ? 1 : summary.size.height;
      const float radius = std::max(2.0f, scale * 0.14f);

      for (int y = 0; y < studsY; ++y)
      {
        for (int x = 0; x < studsX; ++x)
        {
          float sx = summary.studs == nb::StudMode::Centered ? center.x : b0.x + (x + 0.5f) * scale;
          float sy = summary.studs == nb::StudMode::Centered ? center.y : b0.y + (y + 0.5f) * scale;
          dl->AddCircleFilled(ImVec2(sx, sy), radius, top, 12);
          dl->AddCircle(ImVec2(sx, sy), radius, edge, 12, 1.0f);
        }
      }
    }
  }
}

void UI::drawInstructionExportWindow()
{
  if (!_instructionExportWindowVisible)
    return;

  ImGui::SetNextWindowSize(ImVec2(520, 620), ImGuiCond_FirstUseEver);
  if (ImGui::Begin("Export Instructions", &_instructionExportWindowVisible))
  {
    static const int pieceStepOptions[] = { 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 15, 20, 25, 30, 40, 50 };

    ImGui::Text("Grouping");
    ImGui::RadioButton("Pieces per step", &_instructionGroupingMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("By layer", &_instructionGroupingMode, 1);

    if (_instructionGroupingMode == 0)
    {
      if (ImGui::BeginCombo("Pieces", TextFormat("%d", _instructionPiecesPerStep)))
      {
        for (int value : pieceStepOptions)
        {
          bool selected = _instructionPiecesPerStep == value;
          if (ImGui::Selectable(TextFormat("%d", value), selected))
            _instructionPiecesPerStep = value;
          if (selected)
            ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
    }

    ImGui::Separator();
    ImGui::Text("Output");
    ImGui::RadioButton("Separate images", &_instructionImageMode, 0);
    ImGui::SameLine();
    ImGui::RadioButton("Single image", &_instructionImageMode, 1);
    ImGui::InputText("Image prefix", _instructionImagePrefix, sizeof(_instructionImagePrefix));

    ImGui::Separator();
    std::vector<instructions::PieceSummary> summary = instructions::summarizeCurrentModel(_context);
    int totalPieces = 0;
    for (const auto& piece : summary)
      totalPieces += piece.count;

    ImGui::Text("Required pieces: %d", totalPieces);
    ImGui::BeginChild("RequiredPieces", ImVec2(0, 330), true);
    for (const auto& piece : summary)
    {
      ImGui::PushID(&piece);
      drawPiecePreview(piece, ImVec2(56, 40));
      ImGui::SameLine();
      ImGui::Text("x%d  %dx%d  %s  %s  %s",
        piece.count,
        piece.size.width,
        piece.size.height,
        piece.color->ident.c_str(),
        pieceTypeLabel(piece.type),
        studsLabel(piece.studs));
      ImGui::PopID();
    }
    ImGui::EndChild();

    ImGui::Separator();
    if (ImGui::Button("Choose Folder and Export"))
    {
      auto* path = tinyfd_selectFolderDialog("Export Instructions", _context->prefs.basePath.c_str());
      if (path)
      {
        instructions::ExportOptions options;
        options.groupingMode = _instructionGroupingMode == 1 ? instructions::GroupingMode::ByLayer : instructions::GroupingMode::PiecesPerStep;
        options.imageMode = _instructionImageMode == 1 ? instructions::ImageMode::SingleSheet : instructions::ImageMode::SeparateImages;
        options.piecesPerStep = _instructionPiecesPerStep;
        options.imagePrefix = _instructionImagePrefix;

        LOG("Exporting instructions to: %s", path);
        if (instructions::exportCurrentModel(_context, path, options))
          _instructionExportWindowVisible = false;
      }
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel"))
      _instructionExportWindowVisible = false;
  }

  ImGui::End();
}

void UI::drawMainMenu()
{
  if (ImGui::BeginMainMenuBar())
  {
    if (ImGui::BeginMenu("File"))
    {
      if (ImGui::MenuItem("New", "Ctrl+N")) { /* ... */ }
      if (ImGui::MenuItem("Open...", "Ctrl+O")) {
        char const* extensions[] = { "*.yml" };
        auto* path =  tinyfd_openFileDialog("Load Model", _context->prefs.basePath.c_str(), 1, extensions, "model files (*.yml)", 0);
        if (path)
        {
          LOG("Loading model from: %s", path);
          _context->loadModel(path);
        }
      }
      ImGui::Separator();
      static bool autosave = true;
      if (ImGui::MenuItem("Autosave", nullptr, &autosave)) { /* toggle */ }
      ImGui::Separator();

      if (ImGui::MenuItem("Export Instructions..."))
        _instructionExportWindowVisible = true;
      ImGui::Separator();

      if (ImGui::MenuItem("Exit"))
        _context->shouldExit = true;

      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Edit"))
    {
      bool enabled = _context->history->canUndo();
      if (!enabled)
        ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.5f, 0.5f, 0.5f, 1.0f));
      if (ImGui::MenuItem("Undo", "Ctrl+Z", false, enabled))
        _context->history->undoLast();

      if (!enabled)
        ImGui::PopStyleColor();

      if (ImGui::BeginMenu("Grid Snap Mode"))
      {
        auto current = _context->prefs.ui.grid.halfSteps;
        if (ImGui::MenuItem("Free", nullptr, current == GridSnapMode::Free))
          _context->prefs.ui.grid.halfSteps = GridSnapMode::Free;
        if (ImGui::MenuItem("Half", nullptr, current == GridSnapMode::Half))
          _context->prefs.ui.grid.halfSteps = GridSnapMode::Half;
        if (ImGui::MenuItem("Whole", nullptr, current == GridSnapMode::Whole))
          _context->prefs.ui.grid.halfSteps = GridSnapMode::Whole;

        ImGui::EndMenu();
      }

      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("View"))
    {
      if (ImGui::MenuItem("Reset Camera", "F"))
        _context->renderer->resetCamera(_context->model.get());

      ImGui::EndMenu();
    }
    if (ImGui::BeginMenu("Model"))
    {

      if (ImGui::BeginMenu("Grow"))
      {
        if (ImGui::MenuItem("West <")) { _context->model->grow(Direction::West); }
        if (ImGui::MenuItem("East >")) { _context->model->grow(Direction::East); }
        if (ImGui::MenuItem("North ^")) { _context->model->grow(Direction::North); }
        if (ImGui::MenuItem("South v")) { _context->model->grow(Direction::South); }
        if (ImGui::MenuItem("Shrink to fit")) { _context->model->shrinkToFit(); }
        
        ImGui::EndMenu();
      }

      if (ImGui::BeginMenu("Shift"))
      {
        if (ImGui::MenuItem("West <")) { _context->history->execute(new undo::ShiftAction(coord2d_t(-1, 0))); }
        if (ImGui::MenuItem("East >")) { _context->history->execute(new undo::ShiftAction(coord2d_t(+1, 0))); }
        if (ImGui::MenuItem("North ^")) { _context->history->execute(new undo::ShiftAction(coord2d_t(0, -1))); }
        if (ImGui::MenuItem("South v")) { _context->history->execute(new undo::ShiftAction(coord2d_t(0, +1))); }

        ImGui::EndMenu();
      }
      
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();
  }
}

void UI::draw()
{
  drawMainMenu();
  drawToolbar();

  drawPaletteWindow();
  drawPieceTypeWindow();
  if (_studWindowVisible)
    drawStudModeWindow();

  drawViewOptionsWindow();
  drawInstructionExportWindow();
}
