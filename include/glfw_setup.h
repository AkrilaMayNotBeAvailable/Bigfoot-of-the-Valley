#include <glad/glad.h>   // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criação de janelas do sistema operacional
#include <cstdio>
#include <cstdlib>

const int WINDOW_WIDTH = 1280;
const int WINDOW_HEIGHT = 720;

void InitialOpenGLFrameWorkConfiguration(GLFWwindow*& window, float &screenRatio);
void ErrorCallback(int error, const char* description);
void FramebufferSizeCallback(GLFWwindow* window, int width, int height);