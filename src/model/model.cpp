#include "model.h"

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
