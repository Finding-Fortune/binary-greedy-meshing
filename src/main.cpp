#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <glad/glad.h>
#include <algorithm>
#include "misc/shader.h"
#include "misc/camera.h"
#include "misc/noise.h"
#include "misc/utility.h"
#include "misc/timer.h"
#include "rendering/chunk_renderer.h"

#define BM_IMPLEMENTATION
#include "mesher.h"

void create_chunk();

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

GLFWwindow* init_window() {
  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_SAMPLES, 2);

  GLFWwindow* window = glfwCreateWindow(WINDOW_WIDTH, WINDOW_HEIGHT, "Binary Greedy Meshing V2", nullptr, nullptr);
  if (!window) {
    fprintf(stderr, "Unable to create GLFW window\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return nullptr;
  }

  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
  glfwMakeContextCurrent(window);

  if (!gladLoadGL()) {
    fprintf(stderr, "Unable to initialize glad\n");
    glfwDestroyWindow(window);
    glfwTerminate();
    return nullptr;
  }

  return window;
}

void GLAPIENTRY message_callback(
  GLenum source,
  GLenum type,
  GLuint id,
  GLenum severity,
  GLsizei length,
  const GLchar* message,
  const void* userParam
) {
  std::string SEVERITY = "";
  switch (severity) {
  case GL_DEBUG_SEVERITY_LOW:
    SEVERITY = "LOW";
    break;
  case GL_DEBUG_SEVERITY_MEDIUM:
    SEVERITY = "MEDIUM";
    break;
  case GL_DEBUG_SEVERITY_HIGH:
    SEVERITY = "HIGH";
    break;
  case GL_DEBUG_SEVERITY_NOTIFICATION:
    SEVERITY = "NOTIFICATION";
    break;
  }
  fprintf(stderr, "GL CALLBACK: %s type = 0x%x, severity = %s, message = %s\n",
    type == GL_DEBUG_TYPE_ERROR ? "** GL ERROR **" : "",
    type, SEVERITY.c_str(), message);
}

bool init_opengl() {
  glEnable(GL_DEBUG_OUTPUT);
  glDebugMessageControl(GL_DONT_CARE, GL_DONT_CARE, GL_DEBUG_SEVERITY_NOTIFICATION, 0, nullptr, GL_FALSE);
  glDebugMessageCallback(message_callback, 0);

  glEnable(GL_DEPTH_TEST);

  glFrontFace(GL_CCW);
  glCullFace(GL_BACK);
  glEnable(GL_CULL_FACE);

  glClearColor(0.529f, 0.808f, 0.922f, 0.0f);

  glEnable(GL_MULTISAMPLE);

  return true;
};

void glfw_error_callback(int error, const char* description) {
  fprintf(stderr, "GLFW error %d: %s\n", error, description);
}

Shader* shader = nullptr;
Camera* camera = nullptr;
Noise noise;

float last_x = 0.0f;
float last_y = 0.0f;

enum class MESH_TYPE : int {
  TERRAIN,
  SPHERE,
  RANDOM,
  CHECKERBOARD,
  EMPTY,
  Count
};

int mesh_type = (int) MESH_TYPE::TERRAIN;

struct ChunkRenderData {
  glm::ivec3 chunkPos;
  std::vector<DrawElementsIndirectCommand*> faceDrawCommands = { nullptr };
};

MeshData meshData;
ChunkRenderer chunkRenderer;
std::vector<ChunkRenderData> chunkRenderData;

void mouse_callback(GLFWwindow* window, double xpos, double ypos) {
  camera->processMouseMovement(xpos - last_x, last_y - ypos);
  last_x = xpos;
  last_y = ypos;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mods) {
  if (key == GLFW_KEY_ESCAPE && action == GLFW_RELEASE) {
    glfwSetWindowShouldClose(window, true);
  }

  else if (key == GLFW_KEY_X && action == GLFW_RELEASE) {
    GLint lastPolyMode[2];
    glGetIntegerv(GL_POLYGON_MODE, lastPolyMode);
    if (lastPolyMode[0] == GL_FILL) {
      glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
    } else {
      glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
    }
  }

  else if (key == GLFW_KEY_1 && action == GLFW_RELEASE) {
    printf("Forward: %.1f, %.1f, %.1f \n", camera->front.x, camera->front.y, camera->front.z);
  }

  else if (key == GLFW_KEY_SPACE && action == GLFW_RELEASE) {
    // create_chunk();
  }

  else if (key == GLFW_KEY_TAB && action == GLFW_RELEASE) {
    mesh_type++;
    if (mesh_type >= (int) MESH_TYPE::Count) {
      mesh_type = 0;
    }
    // create_chunk();
  }
}

void create_chunk() {
  uint8_t* voxels = new uint8_t[CS_P3] { 0 };
  memset(voxels, 0, CS_P3);
  memset(meshData.opaqueMask, 0, CS_P2 * sizeof(uint64_t));

  switch (mesh_type) {
  case (int) MESH_TYPE::TERRAIN:
  {
    // noise.generateTerrain(voxels, meshData.opaqueMask, 30);
    break;
  }

  case (int) MESH_TYPE::RANDOM:
  {
    noise.generateWhiteNoiseTerrain(voxels, meshData.opaqueMask, 30);
    break;
  }

  case (int) MESH_TYPE::CHECKERBOARD:
  {
    for (int x = 1; x < CS_P; x++) {
      for (int y = 1; y < CS_P; y++) {
        for (int z = 1; z < CS_P; z++) {
          if (x % 2 == 0 && y % 2 == 0 && z % 2 == 0) {
            voxels[get_yzx_index(x, y, z)] = 1;
            meshData.opaqueMask[(y * CS_P) + x] |= 1ull << z;

            voxels[get_yzx_index(x - 1, y - 1, z)] = 2;
            meshData.opaqueMask[((y - 1) * CS_P) + (x - 1)] |= 1ull << z;

            voxels[get_yzx_index(x - 1, y, z - 1)] = 3;
            meshData.opaqueMask[(y * CS_P) + (x - 1)] |= 1ull << (z - 1);

            voxels[get_yzx_index(x, y - 1, z - 1)] = 4;
            meshData.opaqueMask[((y - 1) * CS_P) + x] |= 1ull << (z - 1);
          }
        }
      }
    }
    break;
  }

  case (int) MESH_TYPE::SPHERE:
  {
    int r = CS_P / 2;
    for (int x = -r; x < r; x++) {
      for (int y = -r; y < r; y++) {
        for (int z = -r; z < r; z++) {
          if (std::sqrt(x * x + y * y + z * z) < 30.0f) {
            voxels[get_yzx_index(x + r, y + r, z + r)] = 2;
            meshData.opaqueMask[((y + r) * CS_P) + (x + r)] |= 1ull << (z + r);
          }
        }
      }
    }
    break;
  }

  case (int) MESH_TYPE::EMPTY:
  {
    // empty!
    break;
  }
  }

  {
    int iterations = 1;
    Timer timer(std::to_string(iterations) + " iterations", true);

    for (int i = 0; i < iterations; i++) {
      mesh(voxels, meshData);
    }
  }

  if (meshData.vertexCount) {
    //auto drawCommand = chunkRenderer.getDrawCommand(meshData.vertexCount, 0);
    //chunkRenderer.buffer(*drawCommand, meshData.vertices->data());
    //drawCommands.push_back(drawCommand);
  }

  printf("vertex count: %i\n", meshData.vertexCount);
}

int main(int argc, char* argv[]) {
  glfwSetErrorCallback(glfw_error_callback);

  if (!glfwInit()) {
    fprintf(stderr, "Unable to initialize GLFW\n");
    return 1;
  }

  auto window = init_window();
  if (!window) {
    return 1;
  }
  glfwSetWindowPos(window, 0, 31);
  glfwSwapInterval(0);

  glfwSetCursorPosCallback(window, mouse_callback);
  glfwSetKeyCallback(window, key_callback);

  if (!init_opengl()) {
    fprintf(stderr, "Unable to initialize glad/opengl\n");
    return 1;
  }

  meshData.opaqueMask = new uint64_t[CS_P2] { 0 };
  meshData.faceMasks = new uint64_t[CS_2 * 6] { 0 };

  meshData.vertices = new std::vector<uint64_t>(10000);
  meshData.maxVertices = 10000;

  chunkRenderer.init();

  // create_chunk();

  uint8_t* voxels = new uint8_t[CS_P3] { 0 };

  for (uint32_t x = 0; x < 5; x++) {
    for (uint32_t z = 0; z < 5; z++) {
      memset(voxels, 0, CS_P3);
      memset(meshData.opaqueMask, 0, CS_P2 * sizeof(uint64_t));

      noise.generateTerrainV2(voxels, meshData.opaqueMask, x, z, 30);

      mesh(voxels, meshData);

      if (meshData.vertexCount) {
        glm::ivec3 chunkPos = glm::ivec3(x, y, z);
        std::vector<DrawElementsIndirectCommand*> commands = { nullptr, nullptr, nullptr, nullptr, nullptr, nullptr };

        for (uint32_t i = 0; i <= 5; i++) {
          if (meshData.faceVertexLength[i]) {
            uint32_t y = 0;
            uint32_t baseInstance = (i << 24) | (z << 16) | (y << 8) | x;

            auto drawCommand = chunkRenderer.getDrawCommand(meshData.faceVertexLength[i], baseInstance);

            commands[i] = drawCommand;

            chunkRenderer.buffer(*drawCommand, meshData.vertices->data() + meshData.faceVertexBegin[i]);
          }
        }

        chunkRenderData.push_back(ChunkRenderData({ chunkPos, commands }));
      }
    }
  }

  shader = new Shader("main", "main");
  camera = new Camera(glm::vec3(31, 65, -5));
  camera->handleResolution(WINDOW_WIDTH, WINDOW_HEIGHT);

  float forwardMove = 0.0f;
  float rightMove = 0.0f;
  float noclipSpeed = 250.0f;

  float deltaTime = 0.0f;

  auto lastFrame = glfwGetTime();
  while (!glfwWindowShouldClose(window)) {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    float currentFrame = glfwGetTime();
    deltaTime = currentFrame - lastFrame;
    lastFrame = currentFrame;

    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS) forwardMove = 1.0f;
    else if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) forwardMove = -1.0f;
    else forwardMove = 0.0f;

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS) rightMove = 1.0f;
    else if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) rightMove = -1.0f;
    else rightMove = 0.0f;
    auto wishdir = (camera->front * forwardMove) + (camera->right * rightMove);
    camera->position += noclipSpeed * wishdir * deltaTime;

    glm::ivec3 cameraChunkPos = glm::floor(camera->position / glm::vec3(CS));

    for (const auto& data : chunkRenderData) {
      for (int i = 0; i < 6; i++) {
        auto& d = data.faceDrawCommands[i];
        if (d) {
          switch (i) {
            case 0:
              if (cameraChunkPos.y >= data.chunkPos.y) {
                chunkRenderer.addDrawCommand(*d);
              }
              break;

            case 1:
              if (cameraChunkPos.y <= data.chunkPos.y) {
                chunkRenderer.addDrawCommand(*d);
              }
              break;

            case 2:
              if (cameraChunkPos.x >= data.chunkPos.x) {
                chunkRenderer.addDrawCommand(*d);
              }
              break;

            case 3:
              if (cameraChunkPos.x <= data.chunkPos.x) {
                chunkRenderer.addDrawCommand(*d);
              }
              break;

            case 4:
              if (cameraChunkPos.z >= data.chunkPos.z) {
                chunkRenderer.addDrawCommand(*d);
              }
              break;

            case 5:
              if (cameraChunkPos.z <= data.chunkPos.z) {
                chunkRenderer.addDrawCommand(*d);
              }
              break;
          }
        }
      }
    }

    chunkRenderer.render(*shader, *camera);

    glfwSwapBuffers(window);
    glfwPollEvents();
  }

  return 0;
}