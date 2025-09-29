#pragma once

#include "Vector2.hpp"
#include "Vector3.hpp"
#include "Rectangle.hpp"
#include "Color.hpp"

using vec2 = raylib::Vector2;
using vec3 = raylib::Vector3;
using rect = raylib::Rectangle;
using color = raylib::Color;

using ident_t = std::string;

namespace nb
{
  class PieceColor;
  class Layer;
  class Model;
  class Piece;
}

#include <unordered_map>
#include "Shader.hpp"
#include "Material.hpp"
#include "Mesh.hpp"

struct FlatShader
{
  raylib::ShaderUnmanaged shader;
  unsigned int locationInstanceTransform;
  unsigned int locationColorShade;
  
  raylib::ShaderUnmanaged* operator->() { return &shader; }
};

struct Data
{
  struct Constants
  {
    static constexpr float side = 3.8f;
    static constexpr float height = 3.1f;
    static constexpr float studHeight = 1.4f;
    static constexpr float studDiameter = 2.5f;
  };

  struct Shaders
  {
    FlatShader flatShading;
  } shaders;

  struct Materials
  {
    raylib::Material flatMaterial;
  } materials;

  struct Meshes
  {
    raylib::Mesh cube;
    raylib::Mesh stud;
  } meshes;

  struct Colors : public std::unordered_map<ident_t, nb::PieceColor>
  {
    const nb::PieceColor* lime;
    const nb::PieceColor* white;
  } colors;

  void init();
  void deinit();
};
