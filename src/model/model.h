#pragma once

#include "piece.h"

#include <vector>
#include <memory>

namespace nb
{
  class Model;

  class Layer
  {
  protected:
    layer_index_t _index;
    std::vector<Piece> _pieces;
    Layer* _prev;
    Layer* _next;

  public:
    Layer(layer_index_t index) : _index(index), _prev(nullptr), _next(nullptr) { }
    Layer() : _index(0), _prev(nullptr), _next(nullptr) { }

    void add(const Piece& piece) { _pieces.push_back(piece); }
    Piece* piece(const coord2d_t& coord) const;

    layer_index_t index() const { return _index; }
    const auto& pieces() const { return _pieces; }
    auto& pieces() { return _pieces; }

    const Layer* prev() const { return _prev; }

    friend class nb::Model;
  };

  struct ModelInfo
  {
    std::string name;
  };

  class Model
  {
  protected:
    ModelInfo _info;
    std::vector<std::unique_ptr<Layer>> _layers;

    void linkLayers(Layer* prev, Layer* next);

  public:
    Model(const std::string& name = "") { _info.name = name; }

    void addLayer(layer_index_t index);
    void prepareLayers(layer_index_t count);
    void addPiece(layer_index_t layerIndex, const Piece& piece);

    void addLayerOnTop() { addLayer(static_cast<layer_index_t>(_layers.size())); }
    void addLayerAtBottom() { addLayer(0); }
    
    Layer* layer(layer_index_t index) { return (index < _layers.size()) ? _layers[index].get() : nullptr; }
    const Layer* layer(layer_index_t index) const { return (index < _layers.size()) ? _layers[index].get() : nullptr; }

    void shift(Direction direction);
    
    const auto& layers() const { return _layers; }
    
    auto& info() { return _info; }
    const auto& info() const { return _info; }

    layer_index_t lastLayerIndex() const { return static_cast<layer_index_t>(_layers.size()) - 1; }
    layer_index_t layerCount() const { return static_cast<layer_index_t>(_layers.size()); }

    Piece* piece(const coord3d_t& coord) const;
    void remove(const Piece* piece);
  };

  struct layer_iterator_t
  {
  protected:
    layer_index_t _topMost;
    layer_index_t _i;

  public:
    using value_type = std::pair<layer_index_t, layer_index_t>;

    layer_iterator_t(layer_index_t topMost, layer_index_t i = 0) : _topMost(topMost), _i(i) { }

    value_type operator*() const
    {
      return { static_cast<layer_index_t>(_i), static_cast<layer_index_t>(_topMost - _i) };
    }

    layer_iterator_t& operator++() { ++_i; return *this; }
    bool operator!=(const layer_iterator_t& other) const { return _i != other._i; }

    auto index() const { return _topMost - _i; }
    auto relative() const { return _i; }
  };
}
