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
    raylib::MeshUnmanaged _mesh;
    
    unsigned int _vaoID;
    unsigned int _vboIDs[3];

    unsigned int _vboVertices, _vboTransforms, _vboColorShades;

    /* per instance data */
    std::vector<float16> _colorShadesData;
    std::vector<float16> _transformsData;

    std::vector<InstanceData> _instanceData;

    void update(const Mesh& mesh);

  public:
    ~Batch();

    void setup(raylib::MeshUnmanaged&& mesh, FlatShader* shader);
    void release();
    void draw(const Material& material);

    auto& mesh() { return _mesh; }
    auto& instanceData() { return _instanceData; }
  };

  class Renderer
  {
  public:
    


  protected:
    Context* _context;

    raylib::Camera3D _camera;

    Batch _cubeBatch;
    Batch _cylinderBatch;
    Batch _studBatch;

    std::vector<Batch*> _shapeBatches;

    struct Shaders
    {
      FlatShader flatShading;
    } shaders;

    struct Materials
    {
      raylib::Material flatMaterial;
    } materials;

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
    ~Renderer() { deinit(); }

    void init();
    void deinit();

    void renderLayerGrid2d(vec2 base, const nb::Layer* layer, size2d_t layerSize, size2d_t cellSize);
  };
}
