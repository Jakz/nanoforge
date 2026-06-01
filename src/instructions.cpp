#include "instructions.h"

#include "context.h"
#include "renderer.h"

#include <algorithm>
#include <fstream>
#include <iomanip>
#include <map>
#include <sstream>

namespace
{
  using instructions::PieceEntry;

  const char* typeName(nb::PieceType type)
  {
    return type == nb::PieceType::Round ? "round" : "square";
  }

  const char* studsName(nb::StudMode studs)
  {
    switch (studs)
    {
      case nb::StudMode::None: return "none";
      case nb::StudMode::Centered: return "centered";
      case nb::StudMode::Full: return "full";
    }

    return "full";
  }

  bool samePiece(const nb::Piece& a, const nb::Piece& b)
  {
    const bool sameColor = a.color() && b.color() && a.color()->ident == b.color()->ident;
    return sameColor &&
      a.coord().tx() == b.coord().tx() &&
      a.coord().ty() == b.coord().ty() &&
      a.width() == b.width() &&
      a.height() == b.height() &&
      a.type() == b.type() &&
      a.studs() == b.studs();
  }

  bool containsPiece(const nb::Model& model, layer_index_t layerIndex, const nb::Piece& piece)
  {
    const nb::Layer* layer = model.layer(layerIndex);
    if (!layer)
      return false;

    return std::any_of(layer->pieces().begin(), layer->pieces().end(), [&](const nb::Piece& existing) {
      return samePiece(existing, piece);
    });
  }

  std::vector<PieceEntry> collectPieces(const nb::Model& model)
  {
    std::vector<PieceEntry> pieces;

    for (const auto& layer : model.layers())
      for (const auto& piece : layer->pieces())
        pieces.push_back({ layer->index(), piece });

    std::stable_sort(pieces.begin(), pieces.end(), [](const PieceEntry& a, const PieceEntry& b) {
      if (a.layer != b.layer) return a.layer < b.layer;
      if (a.piece.coord().ty() != b.piece.coord().ty()) return a.piece.coord().ty() < b.piece.coord().ty();
      return a.piece.coord().tx() < b.piece.coord().tx();
    });

    return pieces;
  }

  nb::Piece clonePiece(const nb::Piece& piece, const nb::PieceColor* color)
  {
    return nb::Piece(piece.coord(), color, nb::PieceOrientation::North, piece.type(), piece.size(), piece.studs());
  }

  std::string cleanPrefix(const std::string& prefix)
  {
    std::string result = prefix.empty() ? "step" : prefix;
    for (char& c : result)
      if (c == '/' || c == '\\' || c == ':' || c == '*' || c == '?' || c == '"' || c == '<' || c == '>' || c == '|')
        c = '_';
    return result;
  }

  std::string stepImageName(const std::string& prefix, int step)
  {
    std::ostringstream out;
    out << cleanPrefix(prefix) << "_" << std::setw(3) << std::setfill('0') << step << ".png";
    return out.str();
  }

  std::string sheetImageName(const std::string& prefix)
  {
    return cleanPrefix(prefix) + "_all.png";
  }

  std::string pieceKey(const nb::Piece& piece)
  {
    std::ostringstream out;
    out << piece.color()->ident << " "
      << piece.width() << "x" << piece.height() << " "
      << typeName(piece.type()) << " studs:" << studsName(piece.studs());
    return out.str();
  }

  void writePieceYaml(std::ostream& out, const PieceEntry& entry, const char* indent)
  {
    out << indent << "- layer: " << entry.layer << "\n";
    out << indent << "  color: " << entry.piece.color()->ident << "\n";
    out << indent << "  type: " << typeName(entry.piece.type()) << "\n";
    out << indent << "  studs: " << studsName(entry.piece.studs()) << "\n";
    out << indent << "  position: [" << entry.piece.coord().tx() << ", " << entry.piece.coord().ty() << "]\n";
    out << indent << "  size: [" << entry.piece.width() << ", " << entry.piece.height() << "]\n";
  }

  std::vector<std::pair<size_t, size_t>> makeGroups(const std::vector<PieceEntry>& pieces, const instructions::ExportOptions& options)
  {
    std::vector<std::pair<size_t, size_t>> groups;
    if (pieces.empty())
      return groups;

    if (options.groupingMode == instructions::GroupingMode::ByLayer)
    {
      size_t begin = 0;
      while (begin < pieces.size())
      {
        size_t end = begin + 1;
        while (end < pieces.size() && pieces[end].layer == pieces[begin].layer)
          ++end;
        groups.push_back({ begin, end });
        begin = end;
      }
      return groups;
    }

    const size_t step = static_cast<size_t>(std::max(1, options.piecesPerStep));
    for (size_t begin = 0; begin < pieces.size(); begin += step)
      groups.push_back({ begin, std::min(begin + step, pieces.size()) });

    return groups;
  }

  bool exportSheet(const std::filesystem::path& directory, const std::vector<std::string>& imageNames, const std::string& outputName)
  {
    if (imageNames.empty())
      return false;

    constexpr int cellWidth = 640;
    constexpr int cellHeight = 480;
    constexpr int columns = 2;
    const int rows = static_cast<int>((imageNames.size() + columns - 1) / columns);

    Image sheet = GenImageColor(cellWidth * columns, cellHeight * rows, RAYWHITE);

    for (size_t i = 0; i < imageNames.size(); ++i)
    {
      Image step = LoadImage((directory / imageNames[i]).string().c_str());
      ImageResize(&step, cellWidth, cellHeight);

      Rectangle src = { 0, 0, static_cast<float>(step.width), static_cast<float>(step.height) };
      Rectangle dst = {
        static_cast<float>((i % columns) * cellWidth),
        static_cast<float>((i / columns) * cellHeight),
        static_cast<float>(cellWidth),
        static_cast<float>(cellHeight)
      };

      ImageDraw(&sheet, step, src, dst, WHITE);
      UnloadImage(step);
    }

    bool ok = ExportImage(sheet, (directory / outputName).string().c_str());
    UnloadImage(sheet);

    for (const std::string& imageName : imageNames)
      std::filesystem::remove(directory / imageName);

    return ok;
  }
}

std::vector<PieceEntry> instructions::diff(const nb::Model& before, const nb::Model& after)
{
  std::vector<PieceEntry> result;

  for (const PieceEntry& entry : collectPieces(after))
    if (!containsPiece(before, entry.layer, entry.piece))
      result.push_back(entry);

  return result;
}

std::vector<instructions::PieceSummary> instructions::summarize(const std::vector<PieceEntry>& pieces)
{
  std::vector<PieceSummary> result;

  for (const PieceEntry& entry : pieces)
  {
    auto it = std::find_if(result.begin(), result.end(), [&](const PieceSummary& summary) {
      return summary.color == entry.piece.color() &&
        summary.type == entry.piece.type() &&
        summary.studs == entry.piece.studs() &&
        summary.size.width == entry.piece.width() &&
        summary.size.height == entry.piece.height();
    });

    if (it != result.end())
      ++it->count;
    else
      result.push_back({ entry.piece.color(), entry.piece.type(), entry.piece.studs(), entry.piece.size(), 1 });
  }

  std::stable_sort(result.begin(), result.end(), [](const PieceSummary& a, const PieceSummary& b) {
    if (a.color->ident != b.color->ident) return a.color->ident < b.color->ident;
    if (a.size.width != b.size.width) return a.size.width < b.size.width;
    if (a.size.height != b.size.height) return a.size.height < b.size.height;
    return static_cast<int>(a.type) < static_cast<int>(b.type);
  });

  return result;
}

std::vector<instructions::PieceSummary> instructions::summarizeCurrentModel(Context* context)
{
  if (!context || !context->model)
    return {};

  nb::Model empty(context->model->info().name);
  empty.info().size = context->model->info().size;
  empty.prepareLayers(context->model->layerCount());

  return summarize(diff(empty, *context->model));
}

bool instructions::exportCurrentModel(Context* context, const std::filesystem::path& directory, const ExportOptions& options)
{
  if (!context || !context->model)
    return false;

  nb::Model empty(context->model->info().name);
  empty.info().size = context->model->info().size;
  empty.prepareLayers(context->model->layerCount());

  return exportBetweenModels(context, empty, *context->model, directory, options);
}

bool instructions::exportBetweenModels(Context* context, const nb::Model& before, const nb::Model& after, const std::filesystem::path& directory, const ExportOptions& options)
{
  if (!context || !context->model || !context->renderer)
    return false;

  std::vector<PieceEntry> basePieces = collectPieces(before);
  std::vector<PieceEntry> pieces = diff(before, after);
  if (pieces.empty())
    return false;

  const nb::PieceColor* previousColor = nullptr;
  auto clearBlue = context->data->colors.find("clear_blue");
  if (clearBlue != context->data->colors.end())
    previousColor = &clearBlue->second;
  else
    previousColor = context->data->colors.white;

  std::filesystem::create_directories(directory);
  std::vector<std::pair<size_t, size_t>> groups = makeGroups(pieces, options);
  std::vector<std::string> imageNames;

  std::ofstream manifest(directory / "instructions.yml", std::ios::binary);
  manifest << "model: " << after.info().name << "\n";
  manifest << "base_piece_count: " << basePieces.size() << "\n";
  manifest << "grouping: " << (options.groupingMode == GroupingMode::ByLayer ? "by_layer" : "pieces_per_step") << "\n";
  if (options.groupingMode == GroupingMode::PiecesPerStep)
    manifest << "pieces_per_step: " << std::max(1, options.piecesPerStep) << "\n";
  manifest << "image_mode: " << (options.imageMode == ImageMode::SingleSheet ? "single_sheet" : "separate_images") << "\n";
  manifest << "steps:\n";

  int step = 0;
  for (const auto& group : groups)
  {
    ++step;
    const size_t begin = group.first;
    const size_t end = group.second;

    nb::Model stepModel(after.info().name);
    stepModel.info().size = after.info().size;
    stepModel.prepareLayers(std::max(before.layerCount(), after.layerCount()));

    for (const PieceEntry& entry : basePieces)
      stepModel.addPiece(entry.layer, clonePiece(entry.piece, previousColor));

    for (size_t i = 0; i < end; ++i)
    {
      const PieceEntry& entry = pieces[i];
      const bool isNew = i >= begin;
      stepModel.addPiece(entry.layer, clonePiece(entry.piece, isNew ? entry.piece.color() : previousColor));
    }

    std::string imageName = stepImageName(options.imagePrefix, step);
    bool rendered = context->renderer->exportPng(&stepModel, directory / imageName);
    if (!rendered)
      return false;
    imageNames.push_back(imageName);

    std::map<std::string, int> summary;
    for (size_t i = begin; i < end; ++i)
      ++summary[pieceKey(pieces[i].piece)];

    manifest << "  - index: " << step << "\n";
    manifest << "    image: " << imageName << "\n";
    manifest << "    piece_count: " << (end - begin) << "\n";
    manifest << "    summary:\n";
    for (const auto& [name, count] : summary)
      manifest << "      - count: " << count << "\n        piece: \"" << name << "\"\n";
    manifest << "    pieces:\n";
    for (size_t i = begin; i < end; ++i)
      writePieceYaml(manifest, pieces[i], "      ");
  }

  if (options.imageMode == ImageMode::SingleSheet)
  {
    std::string sheetName = sheetImageName(options.imagePrefix);
    manifest << "single_sheet: " << sheetName << "\n";
    return exportSheet(directory, imageNames, sheetName);
  }

  return true;
}
