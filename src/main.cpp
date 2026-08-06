#include <cmath>
#include <cstdio>
#include <cstdlib>

// Headers abaixo são específicos de C++
#include <set>
#include <map>
#include <stack>
#include <string>
#include <vector>
#include <limits>
#include <fstream>
#include <sstream>
#include <stdexcept>
#include <algorithm>

// Headers das bibliotecas OpenGL
#include <glad/glad.h>   // Criação de contexto OpenGL 3.3
#include <GLFW/glfw3.h>  // Criação de janelas do sistema operacional

// Headers da biblioteca GLM: criação de matrizes e vetores.
#include <glm/mat4x4.hpp>
#include <glm/vec4.hpp>
#include <glm/gtc/type_ptr.hpp>

// Headers da biblioteca para carregar modelos obj
#include <tiny_obj_loader.h>

#include <stb_image.h>

// Headers locais, definidos na pasta "include/"
#include "utils.h"
#include "matrices.h"
#include "scene.h"
#include "game_state.h"
#include "bigfoot.h"
#include "debug_flags.h"
#include "audio.h"
#include "collisions.h"
#include "upgrades.h"
#include "monster_cam.h"
#include "object_ids.h"
#include "gpu_render.h"
#include "player_model.h"
#include "glfw_setup.h"
#include "game_definitions.cpp"

//headers do jogo
#include "camera.h"
#include "player.h"
#include "map_view.h"

#define SPHERE 0
#define BUNNY  1
#define PLANE  2
#define SAFE_ZONE 3
#define BIGFOOT 4

int main(int argc, char* argv[]){
    GLFWwindow* window;
    // Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
    float screenRatio = 1.0f;
    glfwSetWindowUserPointer(window, &screenRatio);
    InitialOpenGLFrameWorkConfiguration(window); // EXTRACTED FUNCTION

    // Definimos a função de callback que será chamada sempre que o usuário fizer algum input
    //=============== BIG REFACTOR NEEDED - Really hard
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetWindowFocusCallback(window, WindowFocusCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    //==================================================
    PrintInfoGPU(); // EXTRACTED FUNCTION
    // Carregamos os shaders de vértices e de fragmentos que serão utilizados
    // para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
    LoadShadersFromFiles();
    LoadGameTextures(); // EXTRACTED FUNCTION
    BuildModels(argc, argv); // EXTRACTED FUNCTION

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();
    TextRendering_InitRect();
    LoadPrestigeMemory();
    StartBackgroundMusic();
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
    g_MouseCaptured = true;
    g_FirstCapturedMouseFrame = true;

    GLFWSetup(); // EXTRACTED FUNCTION

    bool movement_key_pressed = false; // MOVED OUTSIDE LOOP
    bool running_key_pressed = false; // MOVED OUTSIDE LOOP
    bool shoot_button_pressed = false; // MOVED OUTSIDE LOOP
    bool bigfoot_attacking = false; // MOVED OUTSIDE LOOP

    float prev_time = (float)glfwGetTime();
    while(!glfwWindowShouldClose(window)){
        float current_time = (float)glfwGetTime();
        float delta_t = current_time - prev_time;
        prev_time = current_time;

        //===================================================
        // Begin: UPDATE section:
        //===================================================
        ShotgunRecoilCounter(delta_t); // EXTRACTED FUNCTION
        UpdateDeathAnimations(delta_t); // EXTRACTED FUNCTION
        SpectatorMechanic(delta_t); // EXTRACTED FUNCTION
        UpdateSpectatorController(delta_t); // EXTRACTED FUNCTION
        UpdateBigfootCamRequest(window, delta_t); // EXTRACTED FUNCTION
        MovementAndRunningInputCheck(window, movement_key_pressed, running_key_pressed); // EXTRACTED FUNTION
        UpdateCameraBob(delta_t, movement_key_pressed, running_key_pressed); // EXTRACTED FUNCTION
        InGameUpdateMovementAndCollectibles(window, delta_t); // EXTRACTED FUNCTION
        glm::vec4 player_position = g_Camera.GetPosition();
        UpdatePlayerWalkIntensity(delta_t, player_position); // EXTRACTED FUNCTION
        BigfootUpdate(player_position, delta_t); // EXTRACTED FUNCTION
        bigfoot_attacking = IsAnyBigfootAttacking(); // EXTRACTED FUNCTION
        CheckLoseConditions(bigfoot_attacking); // EXTRACTED FUNCTION
        CheckWinConditions(player_position); // EXTRACTED FUNCTION
        shoot_button_pressed = IsShootButtonPressed(window); // EXTRACTED FUNCTION
        ShootingMechanic(shoot_button_pressed, player_position); // EXTRACTED FUNCTION
        g_ShootButtonWasPressed = shoot_button_pressed;
        //===================================================
        // End: UPDATE Section;
        //===================================================

        //===================================================
        // Begin: RENDERING section:
        //===================================================
        ClearColorBackground(); // EXTRACTED FUNCTION

        glm::vec4 camera_position_c  = g_Camera.GetPosition();
        glm::vec4 camera_view_vector = g_Camera.GetViewVector();
        glm::vec4 camera_up_vector   = g_Camera.GetUpVector();

        glm::mat4 view;
        glm::mat4 projection;

        ComputeViewAndProjectionMatrices(view, projection, camera_position_c, camera_view_vector, camera_up_vector, screenRatio); // EXTRACTED FUNCTION
        glm::mat4 model = Matrix_Identity(); // Transformação identidade de modelagem
        UpdateShaderUniforms(window, view, projection, current_time); // EXTRACTED FUNCTION

        // Atualiza os uniforms de iluminação por postes de luz a cada frame,
        // selecionando as luzes/oclusores mais próximos do player.
        UpdateLightingUniforms(glm::vec3(camera_position_c.x, camera_position_c.y, camera_position_c.z));

        DrawCampusMap();

        // Desenhamos os blocos retangulares do cenário.
        // A mesma lista será usada depois para colisão.
        // Desenhamos o Pé Grande como placeholder.
        // A funcao abaixo substitui o modelo do coelho do template.
        glm::vec3 map_bigfoot_position = glm::vec3(0.0f, 0.0f, 0.0f);
        float map_bigfoot_yaw = 0.0f;
        bool has_map_bigfoot = false;
        DrawBigfoots(has_map_bigfoot, current_time, delta_t, map_bigfoot_position, map_bigfoot_yaw); // EXTRACTED FUNTION

        // Esfera de debug da hitbox do Pé Grande.
        DrawHitBoxDebug(model, SAFE_ZONE); // EXTRACTED FUNTION

        // Desenhamos os itens coletáveis.
        // Por enquanto usamos esferas pequenas como placeholder visual.
        std::vector<Collectible>& collectibles = GetSceneCollectibles();
        DrawCollectibles(collectibles, current_time, model); // EXTRACTED FUNTION
        
        // Desenhamos a zona segura/final somente depois que todos os itens forem coletados.
        DrawSafeZone(current_time); // EXTRACTED FUNTION

#if MAP_VIEW_ENABLED
        if (g_MapView.IsActive())
        {
            DrawMapViewOverlay(window, camera_position_c, camera_view_vector, map_bigfoot_position, map_bigfoot_yaw);
        }
#endif

        RenderPlayerView(camera_view_vector, camera_position_c, camera_up_vector, current_time); // EXTRACTED FUNTION
        DrawAdrenalineBoostFilter(window); // EXTRACTED FUNTION
        TextRendering_ShowFramesPerSecond(window); // EXTRACTED FUNTION
        ShowCoordinates(window); // EXTRACTED FUNCTION
        DrawMainMenu(window); // EXTRACTED FUNCTION
        TextDrawingChunk(window); // EXTRACTED FUNCTION

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    StopBackgroundMusic();
    glfwTerminate();

    return 0;
}