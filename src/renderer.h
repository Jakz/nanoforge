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

  struct MeshVertex
  {
    std::array<float, 3> position;
  };

  struct MeshTriangle
  {
    std::array<uint16_t, 3> index;
  };

  struct MyMesh
  {
    std::vector<MeshVertex> vertices;
    std::vector<MeshTriangle> triangles;
  };

  class Batch
  {
    raylib::MeshUnmanaged _oldMesh;
    MyMesh _mesh;
    
    unsigned int _vaoID;
    unsigned int _vboIDs[4];

    unsigned int _vboVertices, _vboIndices, _vboTransforms, _vboColorShades;
  
    /* per instance data */
    std::vector<float16> _colorShadesData;
    std::vector<float16> _transformsData;

    std::vector<InstanceData> _instanceData;

    void update();

  public:
    ~Batch();

    void setup(raylib::MeshUnmanaged&& mesh, FlatShader* shader);
    void release();
    void draw(const Material& material);

    auto& mesh() { return _oldMesh; }
    auto& instanceData() { return _instanceData; }

    void setup(MyMesh&& mesh, FlatShader* shader);
  };

  class TopDownGrid
  {
  protected:
    Context* _context;

  public:
    layer_index_t _offset;
    layer_index_t _shown;

    TopDownGrid(Context* context) : _context(context), _offset(0), _shown(4) { }

    nb::layer_iterator_t begin() const;
    nb::layer_iterator_t end() const;
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

    MyMesh generateCube();
    MyMesh generateCylinder();

  public:
    static constexpr int EDGE_COMPLEXITY = 6;
    static constexpr int MOCK_LAYER_SIZE = 16;

    void render(const nb::Model* model);

    auto& camera() { return _camera; }

  protected:

    void prepareStudsForPiece(const nb::Piece* piece, const raylib::Matrix& layerTransform);
    
    void renderLayerGrid3d(layer_index_t index, size2d_t size);
    void renderLayer(const nb::Layer* layer);
    void renderModel(const nb::Model* model);
    void renderStuds();

  public:
    Renderer(Context* context);
    ~Renderer() { deinit(); }

    void init();
    void deinit();

    TopDownGrid _topDown;
    void renderLayerGrid2d(vec2 base, const nb::Layer* layer, size2d_t layerSize, size2d_t cellSize);
  };
}
