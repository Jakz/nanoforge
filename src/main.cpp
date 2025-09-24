#include "raylib.hpp"
#include "Vector2.hpp"
#include "Vector3.hpp"
#include "Color.hpp"
#include "Window.hpp"
#include "Mesh.hpp"
#include "Model.hpp"
#include "Shader.hpp"

#include <vector>
#include <array>

// https://nanoblocks.fandom.com/wiki/Nanoblocks_Wiki
// https://blockguide.ch/

void DrawCylinderSilhouette(const Vector3& center, float r, float h, const Camera3D& cam, Color col) {
  // Direzione di vista proiettata sul piano XZ
  Vector3 v = { cam.position.x - center.x, 0.0f, cam.position.z - center.z };
  float len = std::sqrt(v.x * v.x + v.z * v.z);
  if (len < 1e-5f) {
    // camera sopra il cilindro: fallback a due direzioni fisse
    v = { 1.0f, 0.0f, 0.0f };
    len = 1.0f;
  }
  else {
    v.x /= len; v.z /= len;
  }

  // Versore tangente (perpendicolare in XZ): ruota v di +90� nel piano XZ
  Vector3 t = { -v.z, 0.0f, v.x };

  // Punti alle basi delle due generatrici
  Vector3 a0 = { center.x + r * t.x, center.y,         center.z + r * t.z };
  Vector3 a1 = { center.x + r * t.x, center.y + h,     center.z + r * t.z };
  Vector3 b0 = { center.x - r * t.x, center.y,         center.z - r * t.z };
  Vector3 b1 = { center.x - r * t.x, center.y + h,     center.z - r * t.z };

  //DrawLine3D(a0, a1, col);
  //DrawLine3D(b0, b1, col);

  DrawLineEx(GetWorldToScreen(a0, cam), GetWorldToScreen(b0, cam), 5.0f, col);
}

auto vertShader = R"(
#version 330

// Attributi standard di raylib
in vec3 vertexPosition;
in vec3 vertexNormal;

uniform mat4 mvp;
uniform mat4 matModel;

out vec3 vNormalWorld;

void main() {
    vNormalWorld = normalize((matModel * vec4(vertexNormal, 0.0)).xyz);
    gl_Position = mvp * vec4(vertexPosition, 1.0);
}
)";

auto fragShader = R"(
#version 330

in vec3 vNormalWorld;

uniform vec3 colorUp;     // colore per facce "verso l'alto"
uniform vec3 colorRight;   // colore per tutte le altre
uniform vec3 colorLeft;
uniform float yThreshold; // soglia (0..1). Tipico 0.2-0.4

out vec4 fragColor;

void main()
{
    float isUp = step(yThreshold, vNormalWorld.y);   // 1 se Y>=soglia, altrimenti 0

    if (isUp > 0.5)
    {
      fragColor = vec4(colorUp, 1.0);
    }
    else
    {      
      vec3 col = (vNormalWorld.x > 0.0) ? colorRight : colorLeft;
      fragColor = vec4(col, 1.0);
    }
}
)";

struct PieceColor
{
  std::array<raylib::Color, 4> colors;

  const Color& top() const { return colors[0]; }
  const Color& left() const { return colors[1]; }
  const Color& right() const { return colors[2]; }
  const Color& edge() const { return colors[3]; }

  Vector3 topV() const { return Vector3{top().r / 255.0f, top().g / 255.0f, top().b / 255.0f}; }
  Vector3 leftV() const { return Vector3{left().r / 255.0f, left().g / 255.0f, left().b / 255.0f}; }
  Vector3 rightV() const { return Vector3{right().r / 255.0f, right().g / 255.0f, right().b / 255.0f}; }
};

PieceColor lime = {
  Color{ 164, 219, 15, 255 },
  Color{ 147, 205, 14, 255 },
  Color{ 112, 173, 11, 255 },
  Color{ 107, 166, 11, 255 }
};

// Replica la trasform di DrawModel: T(pos) * R(rot) * S(scale) * model.transform
static inline Matrix MakeDrawTransform(Vector3 pos, float scale, Matrix rot, const Model& model) {
  Matrix S = MatrixScale(scale, scale, scale);
  Matrix TS = MatrixMultiply(S, model.transform);          // S * model.transform
  Matrix T = MatrixTranslate(pos.x, pos.y, pos.z);
  return MatrixMultiply(T, TS);                            // T * (S * model.transform)
}

// Disegna i 12 bordi del cubo dato centro e dimensioni (in local space) + trasform finale
static inline void DrawCubeEdgesFast(Vector3 centerLocal, float w, float h, float d, const Matrix& world, Color col) {
  const float hw = w * 0.5f, hh = h * 0.5f, hd = d * 0.5f;

  // 8 vertici in local space
  Vector3 v[8] = {
      {centerLocal.x - hw, centerLocal.y - hh, centerLocal.z - hd},
      {centerLocal.x + hw, centerLocal.y - hh, centerLocal.z - hd},
      {centerLocal.x + hw, centerLocal.y + hh, centerLocal.z - hd},
      {centerLocal.x - hw, centerLocal.y + hh, centerLocal.z - hd},
      {centerLocal.x - hw, centerLocal.y - hh, centerLocal.z + hd},
      {centerLocal.x + hw, centerLocal.y - hh, centerLocal.z + hd},
      {centerLocal.x + hw, centerLocal.y + hh, centerLocal.z + hd},
      {centerLocal.x - hw, centerLocal.y + hh, centerLocal.z + hd}
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

    DrawCylinderEx(p0, p1, 0.02f, 0.02f, 8, col); // bottom circle
    DrawCylinderEx(q0, q1, 0.02f, 0.02f, 8, col); // top circle
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

  DrawCylinderEx(a0, a1, 0.02f, 0.02f, 8, col);
  DrawCylinderEx(b0, b1, 0.02f, 0.02f, 8, col);
}

constexpr float side = 3.8f;   // lato
constexpr float height = 3.1f;
constexpr float studHeight = 1.4f;
constexpr float studDiameter = 2.5f;

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
    raylib::Shader flatShading;
  } shaders;
  
  struct Models
  {
    raylib::Model cube;
    raylib::Model stud;
  } models;

  void init();
};

void Data::init()
{
  raylib::Mesh cube = GenMeshCube(side, height, side);
  models.cube.Load(cube);

  raylib::Mesh stud = GenMeshCylinder(studDiameter / 2.0f, studHeight, 32);
  models.stud.Load(stud);

  shaders.flatShading = raylib::Shader::LoadFromMemory(vertShader, fragShader);
  shaders.flatShading.locs[SHADER_LOC_MATRIX_MVP] = shaders.flatShading.GetLocation("mvp");
  shaders.flatShading.locs[SHADER_LOC_MATRIX_MODEL] = shaders.flatShading.GetLocation("matModel");

  models.cube.materials[0].shader = shaders.flatShading;
  models.stud.materials[0].shader = shaders.flatShading;
}

Data data;

int main(int arg, char* argv[])
{
  SetConfigFlags(FLAG_MSAA_4X_HINT);

  InitWindow(1000, 700, "Nanoblock shader test - up vs side");

  // Camera orbitale
  Camera3D cam = { 0 };
  cam.position = { 10.0f, 10.0f, 10.0f };
  cam.target = { 0.0f,  0.0f,  0.0f };
  cam.up = { 0.0f,  1.0f,  0.0f };
  cam.fovy = 45.0f;
  cam.projection = CAMERA_PERSPECTIVE;

  // Uniform palette
  int locUp = data.shaders.flatShading.GetLocation("colorUp");
  int locLeft = data.shaders.flatShading.GetLocation("colorLeft");
  int locRight = data.shaders.flatShading.GetLocation("colorRight");
  int locThr = data.shaders.flatShading.GetLocation("yThreshold");

  float thr = 0.3f; 

  auto top = lime.topV();
  auto left = lime.leftV();
  auto right = lime.rightV();
  data.shaders.flatShading.SetValue(locUp, &top, SHADER_UNIFORM_VEC3);
  data.shaders.flatShading.SetValue(locLeft, &left, SHADER_UNIFORM_VEC3);
  data.shaders.flatShading.SetValue(locRight, &right, SHADER_UNIFORM_VEC3);
  data.shaders.flatShading.SetValue(locThr, &thr, SHADER_UNIFORM_FLOAT);

  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    UpdateCamera(&cam, CAMERA_ORBITAL);

    BeginDrawing();
    ClearBackground(RAYWHITE);

    BeginMode3D(cam);
    DrawGrid(20, 10.0f);

    // Disegna cubo al centro
    Vector3 pos = { 0, height / 2, 0 };
    float   scale = 1.0f;
    Matrix  rot = MatrixIdentity();

    data.models.cube.Draw(pos, raylib::Vector3(), 0.0f, raylib::Vector3(1.0f, 1.0f, 1.0f), WHITE);

    // Pass 2: wireframe bordi (sovrapposto)
    Matrix world = MakeDrawTransform(pos, scale, rot, model);
    // centro locale del box � (0,0,0) per GenMeshCube
    DrawCubeEdgesFast({ 0, 0, 0 }, side, height, side, world, lime.edge());

    {
      DrawModel(studModel, { 0, height, 0 }, 1.0f, WHITE);
      DrawCylinderWireframe({ 0, 0, 0 }, studDiameter / 2.0f, studHeight, 32, lime.edge(), MatrixIdentity(), cam);
    }



    EndMode3D();
    EndDrawing();
  }

  UnloadModel(model);
  UnloadModel(studModel);
  UnloadShader(sh);
  CloseWindow();
  return 0;
}

int maizzn(int argc, char *argv[])
{
  SetConfigFlags(FLAG_MSAA_4X_HINT);

  raylib::Window bootstrap(1, 1, "Bootstrap");

  int monitor = GetCurrentMonitor();
  raylib::Vector2 screenSize = raylib::Vector2(GetMonitorWidth(monitor), GetMonitorHeight(monitor));
  screenSize.x = std::min(screenSize.x * 0.666f, 1920.0f);
  screenSize.y = screenSize.x * 1.0f/(16.0f/10.0f);  // 16:9 aspect ratio

  bootstrap.Close();

  raylib::Window window = raylib::Window(screenSize.x, screenSize.y, "Nanoblock");
  SetTargetFPS(60);

  Camera camera = { 0 };
  camera.position = { 50.0f, 50.0f, 50.0f }; 
  camera.target = { 0.0f, 0.0f, 0.0f };
  camera.up = { 0.0f, 1.0f, 0.0f }; 
  camera.fovy = 45.0f; 
  camera.projection = CAMERA_PERSPECTIVE;



  Color fillColor = Color{164, 219, 15, 255};
  Color strokeColor = Color{147, 205, 14, 255};


  while (!WindowShouldClose())
  {
    UpdateCamera(&camera, CAMERA_PERSPECTIVE);
    
    BeginDrawing();
    ClearBackground(RAYWHITE);
   // DrawTexture(tex, 0, 0, WHITE);

    BeginMode3D(camera);

    // Base cubo
    DrawCube(
      { 0.0f, height / 2.0f, 0.0f }, // centro a met� altezza
      side, height, side,
      fillColor
    );
    DrawCubeWires(
      { 0.0f, height / 2.0f, 0.0f },
      side, height, side,
      strokeColor
    );

    // Stud cilindrico
    DrawCylinder(
      { 0.0f, height, 0.0f },   // centro del fondo del cilindro
      studDiameter / 2.0f,          // raggio base
      studDiameter / 2.0f,          // raggio top (cilindro dritto)
      studHeight,                 // altezza
      32,                         // segmenti
      fillColor
    );
    /*DrawCylinderSilhouette(
      { 0.0f, height, 0.0f },
      studDiameter / 2.0f,
      studHeight,
      camera,
      BLACK);*/

    DrawCylinderWires(
      { 0.0f, height, 0.0f },
      studDiameter / 2.0f,
      studDiameter / 2.0f,
      studHeight,
      32,
      strokeColor
    );

    EndMode3D();

    DrawText("Muovi la camera con mouse", 10, 10, 20, DARKGRAY);

    EndDrawing();
  }

  CloseWindow();

  return 0;
}

/*

    {
      "name": "GREEN_LIME",
      "colors": [
        [164, 219, 15],
        [147, 205, 14],
        [112, 173, 11],
        [107, 166, 11]
      ]
    },

*/