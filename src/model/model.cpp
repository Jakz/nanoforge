#include "model.h"

using namespace nb;

Piece* nb::Layer::piece(const coord2d_t& coord) const
{
  for (const auto& p : _pieces)
  {
    if (p.x() >= coord.x && p.x() < coord.x + p.width() &&
      p.y() >= coord.y && p.y() < coord.y + p.height())
      return const_cast<Piece*>(&p);
  }
  return nullptr;
}

void nb::Model::linkLayers(Layer* prev, Layer* next)
{
  if (prev) prev->_next = next;
  if (next) next->_prev = prev;
}

void nb::Model::addLayer(layer_index_t index)
{
  auto newLayer = std::make_unique<Layer>();
  newLayer->_index = index;

  if (index > 0)
  {
    Layer* prev = _layers[index - 1].get();
    linkLayers(prev, newLayer.get());
  }

  if (index < _layers.size())
  {
    Layer* nextLayer = _layers[index].get();
    linkLayers(newLayer.get(), nextLayer);
  }

  for (layer_index_t i = index; i < _layers.size(); ++i)
    _layers[i]->_index += 1;

  index = std::min(index, static_cast<layer_index_t>(_layers.size()));
  _layers.insert(_layers.begin() + index, std::move(newLayer));
}

void nb::Model::prepareLayers(layer_index_t count)
{
  _layers.resize(count);
  for (layer_index_t i = 0; i < count; ++i)
    _layers[i] = std::make_unique<Layer>(i);

  for (size_t i = 0; i < _layers.size(); ++i)
  {
    auto current = _layers[i].get();
    auto next = (i + 1 < _layers.size()) ? _layers[i + 1].get() : nullptr;
    linkLayers(current, next);
  }
}

void nb::Model::addPiece(layer_index_t layerIndex, const Piece& piece)
{
  auto* layer = this->layer(layerIndex);
  if (layer)
    layer->add(piece);
}

Piece* nb::Model::piece(const coord3d_t& coord) const
{
  const Layer* layer = this->layer(coord.z);

  if (layer)
    return layer->piece(coord.xy());

  return nullptr;
}

void nb::Model::remove(const Piece* p)
{
  for (auto& layer : _layers)
  {
    for (auto it = layer->_pieces.begin(); it != layer->_pieces.end(); ++it)
    {
      if (&(*it) == p)
      {
        layer->_pieces.erase(it);
        return;
      }
    }
  }
}