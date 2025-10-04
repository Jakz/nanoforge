#include "renderer.h"

#include "context.h"
#include "input.h"

nb::layer_iterator_t gfx::TopDownGrid::begin() const
{
  layer_index_t topMostLayer = std::min(
    _context->renderer->_topDown._offset + _context->renderer->_topDown._shown - 1,
    _context->model->lastLayerIndex()
  );

  layer_index_t shown = std::min(
    topMostLayer,
    _context->model->layerCount()
  );

  return nb::layer_iterator_t(topMostLayer, 0);
}

nb::layer_iterator_t gfx::TopDownGrid::end() const
{
  layer_index_t topMostLayer = std::min(
    _context->renderer->_topDown._offset + _context->renderer->_topDown._shown - 1,
    _context->model->lastLayerIndex()
  );
  
  return nb::layer_iterator_t(topMostLayer, _shown);
}

auto vertShader = R"(
#version 330

layout(location=0) in vec3 vertexPosition;
layout(location=1) in vec3 vertexNormal;
layout(location=2) in mat4 instanceTransform;
layout(location=6) in mat4 colorShades;

uniform mat4 mvp;

out vec3 vNormalWorld;
flat out mat4 vColorShades;

void main()
{
  mat3 normalMatrix = transpose(inverse(mat3(instanceTransform)));
  vNormalWorld = normalize(mat3(instanceTransform) * vertexNormal);
  vColorShades = colorShades;

  gl_Position = mvp * instanceTransform * vec4(vertexPosition, 1.0);
}
)";

auto fragShader = R"(
#version 330

in vec3 vNormalWorld;
flat in mat4 vColorShades;

const float yThreshold = 0.3;

layout(location = 0) out vec4 fragColor;

void main()
{
  vec3 normal = normalize(vNormalWorld);
  float isUp = step(yThreshold, normal.y);   // 1 se Y>=soglia, altrimenti 0
  if (isUp > 0.5)
  {
    fragColor = vColorShades[0];
  }
  else
  {      
    fragColor = (normal.x > 0.0) ? vColorShades[2] : vColorShades[1];
  }

  fragColor = vColorShades[0];
}
)";

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

#include "par_shapes.h"
#include <array>

static void par_shapes__hemicylinder(float const* uv, float* xyz, void* userdata)
{
  float theta = uv[1] * 1 * PAR_PI;
  xyz[0] = sinf(theta);
  xyz[1] = cosf(theta);
  xyz[2] = uv[0];
}


static void par_shapes__cross3(float* result, float const* a, float const* b)
{
  float x = (a[1] * b[2]) - (a[2] * b[1]);
  float y = (a[2] * b[0]) - (a[0] * b[2]);
  float z = (a[0] * b[1]) - (a[1] * b[0]);
  result[0] = x;
  result[1] = y;
  result[2] = z;
}

static void par_shapes__mix3(float* d, float const* a, float const* b, float t)
{
  float x = b[0] * t + a[0] * (1 - t);
  float y = b[1] * t + a[1] * (1 - t);
  float z = b[2] * t + a[2] * (1 - t);
  d[0] = x;
  d[1] = y;
  d[2] = z;
}

static void par_shapes__scale3(float* result, float a)
{
  result[0] *= a;
  result[1] *= a;
  result[2] *= a;
}
static void par_shapes__normalize3(float* v)
{
  float lsqr = sqrt(v[0] * v[0] + v[1] * v[1] + v[2] * v[2]);
  if (lsqr > 0) {
    par_shapes__scale3(v, 1.0f / lsqr);
  }
}

par_shapes_mesh* par_shapes_create_hemidisk(float radius, int slices,
  float const* center, float const* normal)
{
  par_shapes_mesh* mesh = PAR_CALLOC(par_shapes_mesh, 1);
  mesh->npoints = slices + 1 + 1;
  mesh->points = PAR_MALLOC(float, 3 * mesh->npoints);
  float* points = mesh->points;
  *points++ = 0;
  *points++ = 0;
  *points++ = 0;
  for (int i = 0; i < slices + 1; i++) {
    float theta = i * PAR_PI / slices;
    *points++ = radius * cos(theta);
    *points++ = radius * sin(theta);
    *points++ = 0;
  }
  float nnormal[3] = { normal[0], normal[1], normal[2] };
  par_shapes__normalize3(nnormal);
  mesh->normals = PAR_MALLOC(float, 3 * mesh->npoints);
  float* norms = mesh->normals;
  for (int i = 0; i < mesh->npoints; i++) {
    *norms++ = nnormal[0];
    *norms++ = nnormal[1];
    *norms++ = nnormal[2];
  }
  mesh->ntriangles = slices;
  mesh->triangles = PAR_MALLOC(PAR_SHAPES_T, 3 * mesh->ntriangles);
  PAR_SHAPES_T* triangles = mesh->triangles;
  for (int i = 0; i < slices; i++)
  {
    *triangles++ = 0;
    *triangles++ = 1 + i;
    *triangles++ = 1 + (i + 1);
  }
  float k[3] = { 0, 0, -1 };
  float axis[3];
  par_shapes__cross3(axis, nnormal, k);
  par_shapes__normalize3(axis);
  par_shapes_rotate(mesh, acos(nnormal[2]), axis);
  par_shapes_translate(mesh, center[0], center[1], center[2]);
  return mesh;
}

Mesh GenMeshHemiCylinder(float radius, float height, int slices)
{
  Mesh mesh = { 0 };

  // Instance a cylinder that sits on the Z=0 plane using the given tessellation
  // levels across the UV domain.  Think of "slices" like a number of pizza
  // slices, and "stacks" like a number of stacked rings
  // Height and radius are both 1.0, but they can easily be changed with par_shapes_scale
  par_shapes_mesh* cylinder = par_shapes_create_parametric(par_shapes__hemicylinder, slices, 1, 0);
  par_shapes_scale(cylinder, radius, radius, height);
  par_shapes_rotate(cylinder, -PI / 2.0f, std::array<float, 3>{ 1, 0, 0 }.data());

  
  // Generate an orientable disk shape (top cap)
  par_shapes_mesh* capTop = par_shapes_create_hemidisk(radius, slices, std::array<float, 3>{ 0, 0, 0 }.data(), std::array<float, 3>{ 0, 0, 1 }.data());
  capTop->tcoords = PAR_MALLOC(float, 2 * capTop->npoints);
  for (int i = 0; i < 2 * capTop->npoints; i++) capTop->tcoords[i] = 0.0f;
  par_shapes_rotate(capTop, -PI / 2.0f, std::array<float, 3>{ 1, 0, 0 }.data());
  par_shapes_rotate(capTop, -90 * DEG2RAD, std::array<float, 3>{ 0, 1, 0 }.data());
  par_shapes_translate(capTop, 0, height, 0);

  // Generate an orientable disk shape (bottom cap)
  par_shapes_mesh* capBottom = par_shapes_create_hemidisk(radius, slices, std::array<float, 3>{ 0, 0, 0 }.data(), std::array<float, 3>{ 0, 0, -1 }.data());
  capBottom->tcoords = PAR_MALLOC(float, 2 * capBottom->npoints);
  for (int i = 0; i < 2 * capBottom->npoints; i++) capBottom->tcoords[i] = 0.95f;
  par_shapes_rotate(capBottom, PI / 2.0f, std::array<float, 3>{ 1, 0, 0 }.data());
  par_shapes_rotate(capBottom, -90 * DEG2RAD, std::array<float, 3>{ 0, 1, 0 }.data());

  par_shapes_merge_and_free(cylinder, capTop);
  par_shapes_merge_and_free(cylinder, capBottom);
  

  size_t triangles = cylinder->ntriangles;

  mesh.vertices = (float*)RL_MALLOC(triangles * 3 * 3 * sizeof(float));
  mesh.texcoords = (float*)RL_MALLOC(triangles * 3 * 2 * sizeof(float));
  mesh.normals = (float*)RL_MALLOC(triangles * 3 * 3 * sizeof(float));

  mesh.vertexCount = triangles * 3;
  mesh.triangleCount = triangles;

  for (int k = 0; k < mesh.vertexCount; k++)
  {
    mesh.vertices[k * 3] = cylinder->points[cylinder->triangles[k] * 3];
    mesh.vertices[k * 3 + 1] = cylinder->points[cylinder->triangles[k] * 3 + 1];
    mesh.vertices[k * 3 + 2] = cylinder->points[cylinder->triangles[k] * 3 + 2];

    mesh.normals[k * 3] = cylinder->normals[cylinder->triangles[k] * 3];
    mesh.normals[k * 3 + 1] = cylinder->normals[cylinder->triangles[k] * 3 + 1];
    mesh.normals[k * 3 + 2] = cylinder->normals[cylinder->triangles[k] * 3 + 2];

    mesh.texcoords[k * 2] = cylinder->tcoords[cylinder->triangles[k] * 2];
    mesh.texcoords[k * 2 + 1] = cylinder->tcoords[cylinder->triangles[k] * 2 + 1];
  }

  par_shapes_free_mesh(cylinder);

  // Upload vertex data to GPU (static mesh)
  UploadMesh(&mesh, false);

  ExportMesh(mesh, "cylinder.obj");

  return mesh;
}

//TODO: these are duplicated from main.cpp, move to a common header
constexpr float side = 3.8f;   // lato
constexpr float height = 3.1f;
constexpr float studHeight = 1.4f;
constexpr float studDiameter = 2.5f;

gfx::Renderer::Renderer(Context* context) : _context(context), _topDown(context) { }

void gfx::Renderer::init()
{
  shaders.flatShading.shader = raylib::Shader::LoadFromMemory(vertShader, fragShader);
  shaders.flatShading.shader.locs[SHADER_LOC_MATRIX_MVP] = shaders.flatShading->GetLocation("mvp");
  shaders.flatShading.locationInstanceTransform = shaders.flatShading->GetLocationAttrib("instanceTransform");
  shaders.flatShading.locationColorShade = shaders.flatShading->GetLocationAttrib("colorShades");

  materials.flatMaterial.shader = shaders.flatShading.shader;
  
  _cubeBatch.setup(raylib::MeshUnmanaged::Cube(side, height, side), &shaders.flatShading);
  //_cylinderBatch.setup(raylib::MeshUnmanaged::Cylinder(side / 2, height, 32), &shaders.flatShading);
  _cylinderBatch.setup(GenMeshHemiCylinder(side / 2, height, 32), &shaders.flatShading);
  _studBatch.setup(raylib::MeshUnmanaged::Cylinder(studDiameter / 2.0f, studHeight, 32), &shaders.flatShading);

  /* we need to shift all vertices of cylinder because it's zero aligned */
  for (int i = 0; i < _cylinderBatch.mesh().vertexCount; ++i)
    _cylinderBatch.mesh().vertices[i * 3 + 1] -= height * 0.5f;
  rlEnableVertexArray(_cylinderBatch.mesh().vaoId);
  rlUpdateVertexBuffer(*_cylinderBatch.mesh().vboId, _cylinderBatch.mesh().vertices, _cylinderBatch.mesh().vertexCount * 3 * sizeof(float), 0);
  rlDisableVertexArray();

  _shapeBatches.resize(2);
  _shapeBatches[0] = &_cubeBatch;
  _shapeBatches[1] = &_cylinderBatch;
}

void gfx::Renderer::deinit()
{
  materials.flatMaterial.Unload();
}

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

  /* draw hover if present */
  if (_context->input->hover())
  {
    const coord3d_t& hover = *_context->input->hover();
    if (hover.z == layer->index() || _context->prefs.ui.drawHoverOnAllLayers)
    {
      vec2 pos = vec2(base.x + hover.x * cellSize.width, base.y + hover.y * cellSize.height);
      vec2 size = vec2(cellSize.width * _context->brush->width(), cellSize.height * _context->brush->height());
      DrawRectangleV(pos, size, color(180, 0, 0, 100));
      DrawRectangleLinesEx(rect(pos.x, pos.y, size.x, size.y), 2.0f, color(255, 0, 0, 200));
    }
  }
}

void gfx::Renderer::render(const nb::Model* model)
{
  _studBatch.instanceData().clear();

  renderModel(model);
  renderStuds();
}

void gfx::Renderer::renderLayerGrid3d(layer_index_t index, size2d_t size)
{
  for (int x = 0; x <= size.width; ++x)
  {
    /* draw vertical lines using DrawCylinderEx */
    Vector3 p0 = { x * side, index * height, 0.0f };
    Vector3 p1 = { x * side, index * height, (size.height) * side };
    DrawCylinderEx(p0, p1, 0.02f, 0.02f, EDGE_COMPLEXITY, raylib::Color(80, 80, 80, 100));
  }

  for (int y = 0; y <= size.height; ++y)
  {
    /* draw horizontal lines using DrawCylinderEx */
    Vector3 p0 = { 0.0f, index * height, y * side };
    Vector3 p1 = { (size.width) * side, index * height, y * side };
    DrawCylinderEx(p0, p1, 0.02f, 0.02f, EDGE_COMPLEXITY, raylib::Color(80, 80, 80, 100));
  }
}

void gfx::Renderer::prepareStudsForPiece(const nb::Piece* piece, const raylib::Matrix& layerTransform)
{
  if (piece->studs() == nb::StudMode::None)
    return;
  else if (piece->studs() == nb::StudMode::Centered)
  {
    _studBatch.instanceData().push_back({ layerTransform * raylib::Matrix::Translate((piece->x() + piece->width() * 0.5f) * side, height, (piece->y() + piece->height() * 0.5f) * side), piece->color()});

    raylib::Vector3 center = raylib::Vector3::Zero();
    center = center.Transform(_studBatch.instanceData().back().matrix);

    DrawCylinderWireframe(center, studDiameter / 2.0f, studHeight, 32, piece->color()->edge(), MatrixIdentity(), _camera);
  }
  else
  {
    for (int y = 0; y < piece->height(); ++y)
      for (int x = 0; x < piece->width(); ++x)
      {
        _studBatch.instanceData().push_back({ layerTransform * raylib::Matrix::Translate((piece->x() + x + 0.5f) * side, height, (piece->y() + y + 0.5f) * side), piece->color() });

        raylib::Vector3 center = raylib::Vector3::Zero();
        center = center.Transform(_studBatch.instanceData().back().matrix);

        DrawCylinderWireframe(center, studDiameter / 2.0f, studHeight, 32, piece->color()->edge(), MatrixIdentity(), _camera);
      }
  }
}

void gfx::Renderer::renderLayer(const nb::Layer* layer)
{
  /* compute the matrix for the layer */
  raylib::Matrix layerTransform = raylib::Matrix::Translate(0.0f, layer->index() * height, 0.0f);

  for (const nb::Piece& piece : layer->pieces())
  {
    /* translate inside layer according to position */
    raylib::Matrix pieceTransform = raylib::Matrix::Translate((piece.x() + piece.width() * 0.5f) * side, height * 0.5f, (piece.y() + piece.height() * 0.5f) * side);

    prepareStudsForPiece(&piece, layerTransform);
    
    pieceTransform = raylib::Matrix::Scale(piece.width(), 1.0f, piece.height()) * pieceTransform;
    auto finalTransform = layerTransform * pieceTransform;
    
    if (piece.type() == nb::PieceType::Round)
    {
      _cylinderBatch.instanceData().push_back({ finalTransform, piece.color() });

      raylib::Vector3 center = vec3(0.0f, -height / 2.0f, 0.0f);
      center = center.Transform(finalTransform);

      DrawCylinderWireframe(center, side / 2, height, 32, piece.color()->edge(), MatrixIdentity(), _camera);
    }
    else
    {
      _cubeBatch.instanceData().push_back({ finalTransform, piece.color() });
      DrawCubeEdgesFast(side, height, side, finalTransform, piece.color()->edge());
    }

  }

  for (auto* batch : _shapeBatches)
    batch->draw(materials.flatMaterial);
}

void gfx::Renderer::renderStuds()
{
  _studBatch.draw(materials.flatMaterial);
  _studBatch.instanceData().clear();
}

void gfx::Renderer::renderModel(const nb::Model* model)
{
  _cylinderBatch.instanceData().clear();
  _cubeBatch.instanceData().clear();
  
  for (const auto& layer : model->layers())
    renderLayer(layer.get());

  renderLayerGrid3d(0, size2d_t(MOCK_LAYER_SIZE, MOCK_LAYER_SIZE));
}

#include "glad/glad.h"

gfx::Batch::~Batch()
{
  //_mesh.Unload();
}

void gfx::Batch::draw(const Material& material)
{
  if (_instanceData.empty())
    return;
  
  constexpr size_t MAX_MATERIAL_MAPS = 4;

  // Instancing required variables
  float16* instanceTransforms = NULL;

  // Bind shader program
  rlEnableShader(material.shader.id);

  Matrix matModel = MatrixIdentity();
  Matrix matView = rlGetMatrixModelview();
  Matrix matModelView = MatrixIdentity();
  Matrix matProjection = rlGetMatrixProjection();


  // Accumulate internal matrix transform (push/pop) and view matrix
  // NOTE: In this case, model instance transformation must be computed in the shader
  matModelView = MatrixMultiply(rlGetMatrixTransform(), matView);

  // Upload model normal matrix (if locations available)
  if (material.shader.locs[SHADER_LOC_MATRIX_NORMAL] != -1)
    rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_NORMAL], MatrixTranspose(MatrixInvert(matModel)));


  update(_mesh);
  
  // Try binding vertex array objects (VAO)
  // or use VBOs if not possible
  rlEnableVertexArray(_vaoID);

  // Calculate model-view-projection matrix (MVP)
  Matrix matModelViewProjection = MatrixMultiply(matModelView, matProjection);

  // Send combined model-view-projection matrix to shader
  rlSetUniformMatrix(material.shader.locs[SHADER_LOC_MATRIX_MVP], matModelViewProjection);

  // Draw mesh instanced
  if (_mesh.indices != NULL)
    rlDrawVertexArrayElementsInstanced(0, _mesh.triangleCount * 3, 0, _instanceData.size());
  else
    rlDrawVertexArrayInstanced(0, _mesh.vertexCount, _instanceData.size());

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
  RL_FREE(instanceTransforms);
}


void gfx::Batch::setup(raylib::MeshUnmanaged&& mesh, FlatShader* shader)
{
  //glGenVertexArrays(1, &_vaoID);
  _mesh = std::move(mesh);
  _vaoID = mesh.vaoId;
  glBindVertexArray(_vaoID);

  glGenBuffers(3, &_vboIDs[0]);

  _vboVertices = _vboIDs[0];
  _vboTransforms = _vboIDs[1];
  _vboColorShades = _vboIDs[2];
  
  rlEnableVertexBuffer(_vboTransforms);
  for (unsigned int i = 0; i < 4; i++)
  {
    auto baseLocation = shader->locationInstanceTransform;
    rlEnableVertexAttribute(baseLocation + i);
    rlSetVertexAttribute(baseLocation + i, 4, RL_FLOAT, 0, sizeof(float16), i * sizeof(Vector4));
    rlSetVertexAttributeDivisor(baseLocation + i, 1);
  }

  rlEnableVertexBuffer(_vboColorShades);
  for (unsigned int i = 0; i < 4; i++)
  {
    auto baseLocation = shader->locationColorShade;
    rlEnableVertexAttribute(baseLocation + i);
    rlSetVertexAttribute(baseLocation + i, 4, RL_FLOAT, 0, sizeof(float16), i * sizeof(Vector4));
    rlSetVertexAttributeDivisor(baseLocation + i, 1);
  }

  glBindVertexArray(0);

  //rlBindBuffer(RL_ARRAY_BUFFER, _vboIDs[0]);
  //rlBufferData(RL_ARRAY_BUFFER, _transformsData.size() * sizeof(float16), _transformsData.data(), RL_STATIC_DRAW);
}

void gfx::Batch::release()
{
  _mesh.Unload();
  glDeleteBuffers(3, &_vboIDs[0]);
}

void gfx::Batch::update(const Mesh& mesh)
{
  const size_t count = _instanceData.size();
  
  rlEnableVertexArray(_vaoID);

  //glBindBuffer(GL_ARRAY_BUFFER, _vboVertices);
  //glBufferData(GL_ARRAY_BUFFER, mesh.vertexCount * sizeof(Vector3), mesh.vertices, GL_STATIC_DRAW);

  _transformsData.resize(_instanceData.size());
  for (int i = 0; i < count; i++)
    _transformsData[i] = MatrixToFloatV(_instanceData[i].matrix);
  
  glBindBuffer(GL_ARRAY_BUFFER, _vboTransforms);
  glBufferData(GL_ARRAY_BUFFER, _transformsData.size() * sizeof(float16), _transformsData.data(), GL_STATIC_DRAW);

  _colorShadesData.resize(_instanceData.size());
  for (int i = 0; i < count; ++i)
  {
    for (int j = 0; j < 4; ++j)
    {
      _colorShadesData[i].v[j * 4 + 0] = _instanceData[i].color->colors[j].r / 255.0f;
      _colorShadesData[i].v[j * 4 + 1] = _instanceData[i].color->colors[j].g / 255.0f;
      _colorShadesData[i].v[j * 4 + 2] = _instanceData[i].color->colors[j].b / 255.0f;
      _colorShadesData[i].v[j * 4 + 3] = _instanceData[i].color->colors[j].a / 255.0f;
    }
  }
  
  glBindBuffer(GL_ARRAY_BUFFER, _vboColorShades);
  glBufferData(GL_ARRAY_BUFFER, count * sizeof(float16), _colorShadesData.data(), GL_STATIC_DRAW);

  rlDisableVertexBuffer();
  rlDisableVertexArray();
}
