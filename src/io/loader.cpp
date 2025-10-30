#include "loader.h"

#include "node.hpp"
#include "model/model.h"
#include "context.h"

#include <fstream>


void Loader::save(const nb::Model* model, const std::filesystem::path& filename)
{
  fkyaml::node root = { { "pieces", fkyaml::node::sequence() }, { "info", fkyaml::node::mapping() } };
  root["info"]["name"] = model->info().name;
  root["info"]["size"] = fkyaml::node::sequence({ model->size().width, model->size().height });

  auto& pieces = root["pieces"].as_seq();

  for (const auto& layer : model->layers())
  {
    for (const auto& piece : layer->pieces())
    {
      fkyaml::node node = {
        {
          "position",
          fkyaml::node::sequence({
            layer->index(),
            piece.coord().tx(),
            piece.coord().ty()
          })
        },
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

    model.prepareLayers(maxZ + 1);
    model.info().name = node["info"]["name"].as_str();

    /* load pieces */
    for (const auto& p : node["pieces"].as_seq())
    {
      int z = p["position"][0].as_int();
      int x = p["position"][1].as_int();
      int y = p["position"][2].as_int();

      const nb::PieceColor* color = _context->data->colors.white;
      nb::PieceType type = nb::PieceType::Square;
      nb::StudMode studs = nb::StudMode::Full;

      size2d_t size = size2d_t(1, 1);

      if (p["size"].is_sequence())
      {
        size.width = p["size"][0].as_int();
        size.height = p["size"][1].as_int();
      }

      if (p["color"].is_string())
      {
        auto it = _context->data->colors.find(p["color"].as_str());
        if (it != _context->data->colors.end())
          color = &it->second;
      }

      if (p["type"].is_string())
      {
        if (p["type"] == "round")
          type = nb::PieceType::Round;
      }

      if (p["studs"].is_string())
      {
        if (p["studs"] == "none")
          studs = nb::StudMode::None;
        else if (p["studs"] == "centered")
          studs = nb::StudMode::Centered;
        else if (p["studs"] == "full")
          studs = nb::StudMode::Full;
      }

      model.addPiece(z, nb::Piece(nb::PieceCoord(x, y, true), color, nb::PieceOrientation::North, type, size, studs));
    }

    auto bounds = model.bounds();
    LOG("Loading model %s... (%d pieces, %d layers, %dx%d size)", node["info"]["name"].as_str().c_str(), node["pieces"].as_seq().size(), maxZ, model.bounds().size().width, model.bounds().size().height);

    if (node["info"]["size"].is_sequence())
    {
      model.info().size.width = node["info"]["size"][0].as_int();
      model.info().size.height = node["info"]["size"][1].as_int();
    }
    else
    {
      model.setSizeAccordingToBounds();
    }


    return model;
  }

  return std::optional<nb::Model>();
}

