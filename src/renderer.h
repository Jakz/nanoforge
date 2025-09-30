#include "raylib.hpp"
#include "Matrix.hpp"
#include "Window.hpp"
#include "Mesh.hpp"
#include "Model.hpp"
#include "Shader.hpp"
#include "Camera3D.hpp"

#include "model/model.h"
#include "defines.h"

namespace gfx
{
  struct InstanceData
  {
    Matrix matrix;
    const nb::PieceColor* color;
  };

  class Batch
  {
    unsigned int _vaoID;
    unsigned int _vboIDs[3];

    unsigned int _vboVertices, _vboTransforms, _vboColorShades;

    /* per instance data */
    std::vector<float16> _colorShadesData;
    std::vector<float16> _transformsData;

    void update(const Mesh& mesh, const std::vector<gfx::InstanceData>& instancesData);

  public:
    void setup(int vaoID);
    void release();
    void MyDrawMeshInstanced(const Mesh& mesh, const Material& material, const std::vector<InstanceData>& data);
  };

  class Renderer
  {
  public:
    


  protected:
    Context* _context;

    raylib::Camera3D _camera;
    std::vector<InstanceData> _studData;

    Batch _cubeBatch;
    Batch _studBatch;

  public:
    static constexpr int EDGE_COMPLEXITY = 6;
    static constexpr int MOCK_LAYER_SIZE = 16;

    void render(const nb::Model* model);

    auto& camera() { return _camera; }

  protected:

    void renderLayerGrid3d(layer_index_t index, size2d_t size);
    void renderLayer(const nb::Layer* layer);
    void renderModel(const nb::Model* model);
    void renderStuds();

  public:
    Renderer(Context* context);

    void init();

    void renderLayerGrid2d(vec2 base, const nb::Layer* layer, size2d_t layerSize, size2d_t cellSize);
  };
}
