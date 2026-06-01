#pragma once

#include "model/model.h"

#include <filesystem>
#include <string>
#include <vector>

class Context;

namespace instructions
{
  struct PieceEntry
  {
    layer_index_t layer;
    nb::Piece piece;
  };

  enum class GroupingMode
  {
    PiecesPerStep,
    ByLayer
  };

  enum class ImageMode
  {
    SeparateImages,
    SingleSheet
  };

  struct ExportOptions
  {
    GroupingMode groupingMode = GroupingMode::PiecesPerStep;
    ImageMode imageMode = ImageMode::SeparateImages;
    int piecesPerStep = 8;
    std::string imagePrefix = "step";
  };

  struct PieceSummary
  {
    const nb::PieceColor* color;
    nb::PieceType type;
    nb::StudMode studs;
    size2d_t size;
    int count;
  };

  std::vector<PieceEntry> diff(const nb::Model& before, const nb::Model& after);
  std::vector<PieceSummary> summarize(const std::vector<PieceEntry>& pieces);
  std::vector<PieceSummary> summarizeCurrentModel(Context* context);
  bool exportBetweenModels(Context* context, const nb::Model& before, const nb::Model& after, const std::filesystem::path& directory, const ExportOptions& options);
  bool exportCurrentModel(Context* context, const std::filesystem::path& directory, const ExportOptions& options);
}
