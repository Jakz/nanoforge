#include "renderer.h"

extern Data data;

// Replica la trasform di DrawModel: T(pos) * R(rot) * S(scale) * model.transform
static inline Matrix MakeDrawTransform(Vector3 pos, float scale, Matrix rot, const raylib::Matrix& modelMatrix) {
  Matrix S = MatrixScale(scale, scale, scale);
  Matrix TS = MatrixMultiply(S, modelMatrix);          // S * model.transform
  Matrix T = MatrixTranslate(pos.x, pos.y, pos.z);
  return MatrixMultiply(T, TS);                            // T * (S * model.transform)
}

// Disegna i 12 bordi del cubo dato centro e dimensioni (in local space) + trasform finale
static inline void DrawCubeEdgesFast(float w, float h, float d, const Matrix& world, Color col)
{
  const float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;

  // 8 vertici in local space
  Vector3 v[8] = {
      {- hw, - hh, - hd},
      {+ hw, - hh, - hd},
      {+ hw, + hh, - hd},
      {- hw, + hh, - hd},
      {- hw, - hh, + hd},
      {+ hw, - hh, + hd},
      {+ hw, + hh, + hd},
      {- hw, + hh, + hd}
  };

  // indici dei 12 spigoli
  const int e[12][2] = {
      {0,1},{1,2},{2,3},{3,0}, // back (Z-)
      {4,5},{5,6},{6,7},{7,4}, // front (Z+)
      {0,4},{1,5},{2,6},{3,7}  // verticali
  };

  // trasforma e disegna
  for (int i = 0; i < 12; ++i) {
    Vector3 a = Vector3Transform(v[e[i][0]], world);
    Vector3 b = Vector3Transform(v[e[i][1]], world);
    DrawLine3D(a, b, col);
    DrawCylinderEx(a, b, 0.02f, 0.02f, 8, col);
  }
}

// Disegna wireframe cilindro verticale (asse Y)
void DrawCylinderWireframe(Vector3 center, float radius, float height, int segments, Color col, const Matrix& world, const Camera3D& cam)
{
  float y0 = center.y;
  float y1 = center.y + height;

  // --- cerchio top e bottom ---
  for (int i = 0; i < segments; i++) {
    float a0 = (2 * PI * i) / segments;
    float a1 = (2 * PI * (i + 1)) / segments;
    Vector3 p0 = { center.x + radius * cosf(a0), y0, center.z + radius * sinf(a0) };
    Vector3 p1 = { center.x + radius * cosf(a1), y0, center.z + radius * sinf(a1) };
    Vector3 q0 = { center.x + radius * cosf(a0), y1, center.z + radius * sinf(a0) };
    Vector3 q1 = { center.x + radius * cosf(a1), y1, center.z + radius * sinf(a1) };

    DrawCylinderEx(p0, p1, 0.04f, 0.04f, 8, col); // bottom circle
    DrawCylinderEx(q0, q1, 0.04f, 0.04f, 8, col); // top circle
  }

  // --- due generatrici silhouette (a destra/sinistra rispetto alla camera) ---
  Vector3 viewDir = Vector3Normalize(Vector3Subtract(cam.position, center));
  // proietta in XZ
  Vector2 v = { viewDir.x, viewDir.z };
  float len = sqrtf(v.x * v.x + v.y * v.y);
  if (len < 1e-5f) { v = { 1,0 }; len = 1; }
  v.x /= len; v.y /= len;
  // perpendicolare in XZ
  Vector2 t = { -v.y, v.x };

  Vector3 a0 = { center.x + radius * t.x, y0, center.z + radius * t.y };
  Vector3 a1 = { center.x + radius * t.x, y1, center.z + radius * t.y };
  Vector3 b0 = { center.x - radius * t.x, y0, center.z - radius * t.y };
  Vector3 b1 = { center.x - radius * t.x, y1, center.z - radius * t.y };

  DrawCylinderEx(a0, a1, 0.04f, 0.04f, 8, col);
  DrawCylinderEx(b0, b1, 0.04f, 0.04f, 8, col);
}


//TODO: these are duplicated from main.cpp, move to a common header
constexpr float side = 3.8f;   // lato
constexpr float height = 3.1f;
constexpr float studHeight = 1.4f;
constexpr float studDiameter = 2.5f;

void MyDrawMeshInstanced(const Mesh& mesh, const Material& material, const gfx::InstanceData* transforms, int instances);

void gfx::Renderer::renderLayerGrid2d(vec2 base, const nb::Layer* layer, size2d_t layerSize, size2d_t cellSize)
{
  /* draw a thin black grid with half opacity over the pieces */
  for (int x = 0; x <= layerSize.width; ++x)
  {
    vec2 p0 = vec2(base.x + x * cellSize.width, base.y);
    vec2 p1 = vec2(base.x + x * cellSize.width, base.y + layerSize.height * cellSize.height);
    DrawLineV(p0, p1, color(0, 0, 0, 100));
  }

  for (int y = 0; y <= layerSize.height; ++y)
  {
    vec2 p0 = vec2(base.x, base.y + y * cellSize.height);
    vec2 p1 = vec2(base.x + layerSize.width * cellSize.width, base.y + y * cellSize.height);
    DrawLineV(p0, p1, color(0, 0, 0, 100));
  }

  /* draw pieces of layer below with half opacity */
  auto prev = layer->prev();
  if (prev)
  {
    for (const nb::Piece& piece : prev->pieces())
    {
      vec2 pos = vec2(base.x + piece.x() * cellSize.width, base.y + piece.y() * cellSize.height);
      vec2 size = vec2(piece.width() * cellSize.width, piece.height() * cellSize.height);
      DrawRectangleV(pos, size, piece.color()->top().Fade(0.5f));
      DrawRectangleLinesEx(rect(pos.x, pos.y, size.x, size.y), 1.0f, piece.color()->edge().Fade(0.8f));
    }
  }
  
  /* draw pieces as rect with outline using piece color */
  for (const nb::Piece& piece : layer->pieces())
  {
    vec2 pos = vec2(base.x + piece.x() * cellSize.width, base.y + piece.y() * cellSize.height);
    vec2 size = vec2(piece.width() * cellSize.width, piece.height() * cellSize.height);

    DrawRectangleV(pos, size, piece.color()->top());
    DrawRectangleLinesEx(rect(pos.x, pos.y, size.x, size.y), 2.0f, piece.color()->edge());
  }


}

void gfx::Renderer::render(const nb::Model* model)
{
  _studData.clear();
  renderModel(model);
  renderStuds();
}

void gfx::Renderer::renderLayerGrid3d(layer_index_t index, size2d_t size)
{
  for (int x = 0; x < size.width; ++x)
  {
    /* draw vertical lines using DrawCylinderEx */
    Vector3 p0 = { x * side, index * height, 0.0f };
    Vector3 p1 = { x * side, index * height, (size.height - 1) * side };
    DrawCylinderEx(p0, p1, 0.02f, 0.02f, EDGE_COMPLEXITY, raylib::Color(80, 80, 80, 100));
  }

  for (int y = 0; y < size.height; ++y)
  {
    /* draw horizontal lines using DrawCylinderEx */
    Vector3 p0 = { 0.0f, index * height, y * side };
    Vector3 p1 = { (size.width - 1) * side, index * height, y * side };
    DrawCylinderEx(p0, p1, 0.02f, 0.02f, EDGE_COMPLEXITY, raylib::Color(80, 80, 80, 100));
  }
}

void gfx::Renderer::renderLayer(const nb::Layer* layer)
{
  /* compute the matrix for the layer */
  raylib::Matrix layerTransform = raylib::Matrix::Translate(0.0f, layer->index() * height, 0.0f);

  std::vector<InstanceData> transforms(layer->pieces().size());

  size_t i = 0;
  for (const nb::Piece& piece : layer->pieces())
  {
    /* translate inside layer according to position */
    raylib::Matrix pieceTransform = raylib::Matrix::Translate((piece.x() + piece.width() * 0.5f) * side, height * 0.5f, (piece.y() + piece.height() * 0.5f) * side);

    for (int y = 0; y < piece.height(); ++y)
      for (int x = 0; x < piece.width(); ++x)
      {
        _studData.push_back({ layerTransform * raylib::Matrix::Translate((piece.x() + x + 0.5f) * side, height, (piece.y() + y + 0.5f) * side), piece.color() });

        raylib::Vector3 center = raylib::Vector3::Zero();
        center = center.Transform(_studData.back().matrix);

        DrawCylinderWireframe(center, studDiameter / 2.0f, studHeight, 32, piece.color()->edge(), MatrixIdentity(), _camera);
      }


    pieceTransform = raylib::Matrix::Scale(piece.width(), 1.0f, piece.height()) * pieceTransform;
 
    /* combine layer and piece matrix */
    transforms[i] = { layerTransform * pieceTransform, piece.color() };
    ++i;

    DrawCubeEdgesFast(side, height, side, transforms[i - 1].matrix, piece.color()->edge());
  }

  _cubeBatch.MyDrawMeshInstanced(data.meshes.cube, data.materials.flatMaterial, transforms.data(), transforms.size());
}

void gfx::Renderer::renderStuds()
{
  if (_studData.empty())
    return;

  _studBatch.MyDrawMeshInstanced(data.meshes.stud, data.materials.flatMaterial, _studData.data(), _studData.size());

  _studData.clear();
}

void gfx::Renderer::renderModel(const nb::Model* model)
{
  for (const auto& layer : model->layers())
    renderLayer(layer.get());

  renderLayerGrid3d(0, size2d_t(MOCK_LAYER_SIZE, MOCK_LAYER_SIZE));
}

void gfx::Batch::MyDrawMeshInstanced(const Mesh& mesh, const Material& material, const gfx::InstanceData* data, int instances)
{
  constexpr size_t MAX_MATERIAL_MAPS = 4;

  // Instancing required variables
  float16* instanceTransforms = NULL;
  unsigned int instancesVboId = 0;
  unsigned int colorsVboId = 0;

  // Bind shader program
  rlEnableShader(material.shader.id);

  Matrix matModel = MatrixIdentity();
  Matrix matView = rlGetMatrixModelview();
  Matrix matProjection = rlGetMatrixProjection();

  // Create instances buffer
  instanceTransforms = (float16*)RL_MALLOC(instances * sizeof(float16));

  // Fill buffer with instances transformations as float16 arrays
  for (int i = 0; i < instances; i++)
    instanceTransforms[i] = MatrixToFloatV(data[i].matrix);

  // Enable mesh VAO to attach new buffer
  rlEnableVertexArray(mesh.vaoId);

  instancesVboId = rlLoadVertexBuffer(instanceTransforms, instances * sizeof(float16), false);

  for (unsigned int i = 0; i < 4; i++)
  {
    auto baseLocation = material.shader.locs[SHADER_LOC_VERTEX_INSTANCE_TX];
    rlEnableVertexAttribute(baseLocation + i);
    rlSetVertexAttribute(baseLocation + i, 4, RL_FLOAT, 0, sizeof(float16), i * sizeof(Vector4));
    rlSetVertexAttributeDivisor(baseLocation + i, 1);
  }

  float16* colorShades = (float16*)RL_MALLOC(instances * sizeof(float16));
  for (int i = 0; i < instances; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      colorShades[i].v[j * 4 + 0] = data[i].color->colors[j].r / 255.0f;
      colorShades[i].v[j * 4 + 1] = data[i].color->colors[j].g / 255.0f;
      colorShades[i].v[j * 4 + 2] = data[i].color->colors[j].b / 255.0f;
      colorShades[i].v[j * 4 + 3] = data[i].color->colors[j].a / 255.0f;
    }
  }

  colorsVboId = rlLoadVertexBuffer(colorShades, instances * sizeof(float16), false);

  for (unsigned int i = 0; i < 4; i++)
  {
    auto baseLocation = material.shader.locs[SHADER_LOC_COLOR_SHADE];
    rlEnableVertexAttribute(baseLocation + i);
    rlSetVertexAttribute(baseLocation + i, 4, RL_FLOAT, 0, sizeof(float16), i * sizeof(Vector4));
    rlSetVertexAttributeDivisor(baseLocation + i, 1);
  }

  rlDisableVertexBuffer();
  rlDisableVertexArray();

  // Upload model normal matrix (if locations available)
  if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1)
    rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(matModel)));

  // Bind active texture maps (if available)
  for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
  {
    if (material.maps[i].texture.id > 0)
    {
      // Select current shader texture slot
      rlActiveTextureSlot(i);

      // Enable texture for active slot
      if ((i == MATERIAL_MAP_IRRADIANCE) ||
        (i == MATERIAL_MAP_PREFILTER) ||
        (i == MATERIAL_MAP_CUBEMAP)) rlEnableTextureCubemap(material.maps[i].texture.id);
      else rlEnableTexture(material.maps[i].texture.id);

      rlSetUniform(material.shader.locs[SHADER_LOC_MAP_DIFFUSE + i], &i, SHADER_UNIFORM_INT, 1);
    }
  }

  // Calculate model-view-projection matrix (MVP)
  Matrix matModelViewProjection = MatrixIdentity();
  matModelViewProjection = MatrixMultiply(matView, matProjection);

  // Send combined model-view-projection matrix to shader
  rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

  // Draw mesh instanced
  if (mesh.indices != NULL)
    rlDrawVertexArrayElementsInstanced(0, mesh.triangleCount * 3, 0, instances);
  else
    rlDrawVertexArrayInstanced(0, mesh.vertexCount, instances);

  // Unbind all bound texture maps
  for (int i = 0; i < MAX_MATERIAL_MAPS; i++)
  {
    if (material.maps[i].texture.id > 0)
    {
      // Select current shader texture slot
      rlActiveTextureSlot(i);

      // Disable texture for active slot
      if ((i == MATERIAL_MAP_IRRADIANCE) ||
        (i == MATERIAL_MAP_PREFILTER) ||
        (i == MATERIAL_MAP_CUBEMAP)) rlDisableTextureCubemap();
      else rlDisableTexture();
    }
  }

  // Disable all possible vertex array objects (or VBOs)
  rlDisableVertexArray();
  rlDisableVertexBuffer();
  rlDisableVertexBufferElement();

  // Disable shader program
  rlDisableShader();

  // Remove instance transforms buffer
  rlUnloadVertexBuffer(instancesVboId);
  RL_FREE(instanceTransforms);

  rlUnloadVertexBuffer(colorsVboId);
  RL_FREE(colorShades);
}

    #if defined(GRAPHICS_API_OPENGL_ES2)
        #include "glad_gles2.h"       // Required for: OpenGL functionality 
        #define glGenVertexArrays glGenVertexArraysOES
        #define glBindVertexArray glBindVertexArrayOES
        #define glDeleteVertexArrays glDeleteVertexArraysOES
        #define GLSL_VERSION            100
    #else
        #if defined(__APPLE__)
            #define GL_SILENCE_DEPRECATION // Silence Opengl API deprecation warnings 
            #include <OpenGL/gl3.h>     // OpenGL 3 library for OSX
            #include <OpenGL/gl3ext.h>  // OpenGL 3 extensions library for OSX
        #else
            #include "glad.h"       // Required for: OpenGL functionality 
        #endif
        #define GLSL_VERSION            330
    #endif

void gfx::Batch::setup()
{
  glGenVertexArrays(1, &_vaoID);
  glBindVertexArray(_vaoID);

  glGenBuffers(3, &_vboIDs[0]);

  _vboVertices = _vboIDs[0];
  _vboTransforms = _vboIDs[1];
  _vboColorShades = _vboIDs[2];

  glBindVertexArray(0);

  //rlBindBuffer(RL_ARRAY_BUFFER, _vboIDs[0]);
  //rlBufferData(RL_ARRAY_BUFFER, _transformsData.size() * sizeof(float16), _transformsData.data(), RL_STATIC_DRAW);
}

void gfx::Batch::update(const raylib::Mesh& mesh, const std::vector<gfx::InstanceData>& instancesData)
{
  glBindVertexArray(_vaoID);

  glBindBuffer(GL_ARRAY_BUFFER, _vboVertices);
  glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * sizeof(Vector3), mesh.vertices, GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, _vboTransforms);
  glBufferData(GL_ARRAY_BUFFER, _transformsData.size() * sizeof(float16), _transformsData.data(), GL_STATIC_DRAW);

  glBindBuffer(GL_ARRAY_BUFFER, _vboColorShades);
  glBufferData(GL_ARRAY_BUFFER, _colorShadesData.size() * sizeof(float16), _colorShadesData.data(), GL_STATIC_DRAW);

  glBindVertexArray(0);
}
