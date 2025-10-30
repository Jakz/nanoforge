#pragma once

#include "defines.h"
#include <filesystem>

class Context;

class Loader
{
protected:
  Context* _context;
public:
  Loader(Context* context) : _context(context) {}

  std::optional<nb::Model> load(const std::filesystem::path& filename);
  void save(const nb::Model* model, const std::filesystem::path& filename);
};