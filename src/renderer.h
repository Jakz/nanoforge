#include "raylib.hpp"
#include "Matrix.hpp"
#include "Window.hpp"
#include "Mesh.hpp"
#include "Model.hpp"
#include "Shader.hpp"
#include "Camera3D.hpp"

#include "model/model.h"
#include "defines.h"

typedef struct par_shapes_mesh_s par_shapes_mesh;

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

  using TriangleNormal = std::array<float, 3>;

  struct MeshTriangle
  {
    std::array<uint16_t, 3> index;
  };

  struct MyMesh
  {
    std::vector<MeshVertex> vertices;
    std::vector<TriangleNormal> normals;
    std::vector<MeshTriangle> triangles;

    void generateAndFree(par_shapes_mesh* shape);
  };

  class PieceRenderer
  {

  };

  class Batch
  {
    raylib::MeshUnmanaged _oldMesh;
    MyMesh _mesh;
    
    unsigned int _vaoID;
    unsigned int _vboIDs[5];

    unsigned int _vboVertices, _vboNormals, _vboIndices, _vboTransforms, _vboColorShades;
  
    /* per instance data */
    std::vector<float16> _colorShadesData;
    std::vector<float16> _transformsData;

    std::vector<InstanceData> _instanceData;

    void update();

  public:
    ~Batch();

    void setup(raylib::MeshUnmanaged&& mesh, FlatShader* shader);
    void release();
    void draw(FlatShader* shader);

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
    Batch _halfCylinderBatch;
    Batch _studBatch;

    std::vector<Batch*> _shapeBatches;

    struct Shaders
    {
      FlatShader flatShading;
    } shaders;

    MyMesh generateCube(const vec3& size);
    MyMesh generateCylinder(float radius, float height);
    MyMesh generateHalfCylinder();

  public:
    static constexpr int EDGE_COMPLEXITY = 6;

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
