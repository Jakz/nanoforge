#include "raylib.hpp"
#include "Vector2.hpp"
#include "Window.hpp"

#include <vector>
#include <array>

// https://nanoblocks.fandom.com/wiki/Nanoblocks_Wiki
// https://blockguide.ch/

void DrawCylinderSilhouette(const Vector3& center, float r, float h, const Camera3D& cam, Color col) {
  // Direzione di vista proiettata sul piano XZ
  Vector3 v = { cam.position.x - center.x, 0.0f, cam.position.z - center.z };
  float len = sqrtf(v.x * v.x + v.z * v.z);
  if (len < 1e-5f) {
    // camera sopra il cilindro: fallback a due direzioni fisse
    v = { 1.0f, 0.0f, 0.0f };
    len = 1.0f;
  }
  else {
    v.x /= len; v.z /= len;
  }

  // Versore tangente (perpendicolare in XZ): ruota v di +90° nel piano XZ
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
    gl_Position = mvp * matModel * vec4(vertexPosition, 1.0);
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
  std::array<Color, 4> colors;

  const Color& top() const { return colors[0]; }
  const Color& left() const { return colors[1]; }
  const Color& right() const { return colors[2]; }
  const Color& edge() const { return colors[3]; }

  Vector3 topV() const { return Vector3(top().r / 255.0f, top().g / 255.0f, top().b / 255.0f); }
  Vector3 leftV() const { return Vector3(left().r / 255.0f, left().g / 255.0f, left().b / 255.0f); }
  Vector3 rightV() const { return Vector3(right().r / 255.0f, right().g / 255.0f, right().b / 255.0f); }
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
  }
}

// Disegna wireframe cilindro verticale (asse Y)
void DrawCylinderWireframe(Vector3 center, float radius, float height, int segments, Color col, const Camera3D& cam)
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

    DrawLine3D(p0, p1, col); // bottom circle
    DrawLine3D(q0, q1, col); // top circle
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

  DrawLine3D(a0, a1, col);
  DrawLine3D(b0, b1, col);
}

constexpr float side = 3.8f;   // lato
constexpr float height = 3.1f;
constexpr float studHeight = 1.4f;
constexpr float studDiameter = 2.5f;

int main(int arg, char* argv[])
{
  SetConfigFlags(FLAG_MSAA_4X_HINT);

  InitWindow(1000, 700, "Nanoblock shader test - up vs side");

  // Camera orbitale
  Camera3D cam = { 0 };
  cam.position = { 10.0f, 1.0f, 10.0f };
  cam.target = { 0.0f,  0.0f,  0.0f };
  cam.up = { 0.0f,  1.0f,  0.0f };
  cam.fovy = 45.0f;
  cam.projection = CAMERA_PERSPECTIVE;

  Mesh cube = GenMeshCube(side, height, side);
  Model model = LoadModelFromMesh(cube);

  Mesh stud = GenMeshCylinder(studDiameter / 2.0f, studHeight, 32);
  Model studModel = LoadModelFromMesh(stud);

  // Carica shader
  Shader sh = LoadShaderFromMemory(vertShader, fragShader);
  // Mappa i loc standard (raylib spesso lo fa già, ma esplicitiamo)
  sh.locs[SHADER_LOC_MATRIX_MVP] = GetShaderLocation(sh, "mvp");
  sh.locs[SHADER_LOC_MATRIX_MODEL] = GetShaderLocation(sh, "matModel");

  // Uniform palette
  int locUp = GetShaderLocation(sh, "colorUp");
  int locLeft = GetShaderLocation(sh, "colorLeft");
  int locRight = GetShaderLocation(sh, "colorRight");
  int locThr = GetShaderLocation(sh, "yThreshold");

  // Toni “manuale istruzioni”
  Vector3 upCol = { 0.95f, 0.92f, 0.85f }; // top più chiaro
  Vector3 sideCol = { 0.85f, 0.80f, 0.74f }; // lati più scuri
  float thr = 0.3f; // considera "up" se normale.y >= 0.3

  auto top = lime.topV();
  auto left = lime.leftV();
  auto right = lime.rightV();
  SetShaderValue(sh, locUp, &top, SHADER_UNIFORM_VEC3);
  SetShaderValue(sh, locLeft, &left, SHADER_UNIFORM_VEC3);
  SetShaderValue(sh, locRight, &right, SHADER_UNIFORM_VEC3);
  SetShaderValue(sh, locThr, &thr, SHADER_UNIFORM_FLOAT);

  // Applica shader al materiale del modello
  model.materials[0].shader = sh;
  model.transform = MatrixIdentity();

  studModel.materials[0].shader = sh;

  SetTargetFPS(60);

  while (!WindowShouldClose()) {
    UpdateCamera(&cam, CAMERA_FREE);

    BeginDrawing();
    ClearBackground({ 240,240,240,255 });

    BeginMode3D(cam);
    DrawGrid(20, 10.0f);

    DrawPlane({ 0,0,0 }, { 20, 20 }, { 220,220,220,255 });  // grande piano piatto a Y=0


    // Disegna cubo al centro
    Vector3 pos = { 0, 0, 0 };
    float   scale = 1.0f;
    Matrix  rot = MatrixIdentity();

    // Pass 1: riempimento
    DrawModel(model, pos, scale, WHITE);

    BoundingBox bb = GetMeshBoundingBox(cube);
    printf("BB local: minY=%.2f maxY=%.2f (size=%.2f)\n", bb.min.y, bb.max.y, bb.max.y - bb.min.y);

    // Pass 2: wireframe bordi (sovrapposto)
    Matrix world = MakeDrawTransform(pos, scale, rot, model);
    // centro locale del box è (0,0,0) per GenMeshCube
    DrawCubeEdgesFast({ 0, 0, 0 }, side, height, side, world, lime.edge());

    {
      DrawModel(studModel, { 0, height / 2, 0 }, 1.0f, WHITE);
      DrawCylinderWireframe({ 0, height / 2 + studHeight, 0 }, studDiameter / 2.0f, studHeight, 32, lime.edge(), cam);
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



  Color fillColor = Color(164, 219, 15, 255);
  Color strokeColor = Color(147, 205, 14, 255);


  while (!WindowShouldClose())
  {
    UpdateCamera(&camera, CAMERA_PERSPECTIVE);
    
    BeginDrawing();
    ClearBackground(RAYWHITE);
   // DrawTexture(tex, 0, 0, WHITE);

    BeginMode3D(camera);

    // Base cubo
    DrawCube(
      { 0.0f, height / 2.0f, 0.0f }, // centro a metà altezza
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