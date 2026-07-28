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



int main(int argc, char* argv[]){
    // Inicializamos a biblioteca GLFW, utilizada para criar uma janela do
    // sistema operacional, onde poderemos renderizar com OpenGL.
    // Criamos uma janela do sistema operacional, com 1280 colunas e 720 linhas
    // de pixels, e com título "INF01047 ...".
    GLFWwindow* window;
    // Razão de proporção da janela (largura/altura). Veja função FramebufferSizeCallback().
    float screenRatio = 1.0f;
    glfwSetWindowUserPointer(window, &screenRatio);
    InitialOpenGLFrameWorkConfiguration(window);

    // Definimos a função de callback que será chamada sempre que o usuário fizer algum input
    //=============== BIG REFACTOR NEEDED - Hard Difficult
    glfwSetKeyCallback(window, KeyCallback);
    glfwSetMouseButtonCallback(window, MouseButtonCallback);
    glfwSetCursorPosCallback(window, CursorPosCallback);
    glfwSetWindowFocusCallback(window, WindowFocusCallback);
    glfwSetScrollCallback(window, ScrollCallback);
    //==================================================
    PrintInfoGPU();

    // Carregamos os shaders de vértices e de fragmentos que serão utilizados
    // para renderização. Veja slides 180-200 do documento Aula_03_Rendering_Pipeline_Grafico.pdf.
    LoadShadersFromFiles();

    // Carregamos duas imagens para serem utilizadas como textura
    LoadTextureImage("../../data/textures/textura_tijolos.png");      // TextureImage0
    LoadTextureImage("../../data/textures/textura_grama.png");         // TextureImage1
    LoadTextureImage("../../data/textures/monster-zero-ultra/MonsterUltra_em.png"); // TextureImage2
    LoadTextureImage("../../data/textures/rocky_terrain_02_diff_1k.jpg"); // TextureImage3
    

    // Construímos a representação de objetos geométricos através de malhas de triângulos
    ObjModel spheremodel("../../data/models/sphere.obj");
    ComputeNormals(&spheremodel);
    BuildTrianglesAndAddToVirtualScene(&spheremodel);

    ObjModel bunnymodel("../../data/models/bunny.obj");
    ComputeNormals(&bunnymodel);
    BuildTrianglesAndAddToVirtualScene(&bunnymodel);

    ObjModel planemodel("../../data/models/plane.obj");
    ComputeNormals(&planemodel);
    BuildTrianglesAndAddToVirtualScene(&planemodel);

    ObjModel cubemodel("../../data/models/cube.obj");
    ComputeNormals(&cubemodel);
    BuildTrianglesAndAddToVirtualScene(&cubemodel);

    ObjModel monsterdrinkmodel("../../data/models/monster-zero-ultra/MonsterSubs.obj", "../../data/models/monster-zero-ultra/");
    ComputeNormals(&monsterdrinkmodel);
    BuildTrianglesAndAddToVirtualScene(&monsterdrinkmodel);

    // Carro do estacionamento: shape único "Car_Cube" com 8 materiais (.mtl),
    // separado em peças "Car_Cube_<Material>" por BuildTrianglesAndAddToVirtualScene.
    ObjModel carmodel("../../data/models/car/Car.obj", "../../data/models/car/");
    ComputeNormals(&carmodel);
    BuildTrianglesAndAddToVirtualScene(&carmodel);

    // Banco de madeira: shape unico "Box008" (Z-up, rotacionado em DrawCampusBench).
    ObjModel benchmodel("../../data/models/wooden-bench/16452_WoodenBench_NEW.obj", "../../data/models/wooden-bench/");
    ComputeNormals(&benchmodel);
    BuildTrianglesAndAddToVirtualScene(&benchmodel);

    // Shotgun em primeira pessoa (Remington 870). Modelo deitado no eixo X:
    // +X e a boca do cano, -X a empunhadura. Shapes "Cube.002_Cube.003",
    // "Cube.000_Cube.016" e "Cube.001_Cube.017"; sem .mtl (cor vem do object_id).
    ObjModel shotgunmodel("../../data/models/shotgun/Remengton_870.obj", "../../data/models/shotgun/");
    ComputeNormals(&shotgunmodel);
    BuildTrianglesAndAddToVirtualScene(&shotgunmodel);

    if( argc > 1 ){
        ObjModel model(argv[1]);
        BuildTrianglesAndAddToVirtualScene(&model);
    }

    // Inicializamos o código para renderização de texto.
    TextRendering_Init();
    TextRendering_InitRect();
    LoadPrestigeMemory();
    StartBackgroundMusic();
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glfwGetCursorPos(window, &g_LastCursorPosX, &g_LastCursorPosY);
    g_MouseCaptured = true;
    g_FirstCapturedMouseFrame = true;

    // Habilitamos o Z-buffer. Veja slides 104-116 do documento Aula_09_Projecoes.pdf.
    glEnable(GL_DEPTH_TEST);

    // Habilitamos o Backface Culling. Veja slides 8-13 do documento Aula_02_Fundamentos_Matematicos.pdf, slides 23-34 do documento Aula_13_Clipping_and_Culling.pdf e slides 112-123 do documento Aula_14_Laboratorio_3_Revisao.pdf.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    // Ficamos em um loop infinito, renderizando, até que o usuário feche a janela
    float prev_time = (float)glfwGetTime();
    while (!glfwWindowShouldClose(window))
    {
        float current_time = (float)glfwGetTime();
        float delta_t = current_time - prev_time;
        prev_time = current_time;

        if (g_ShotgunRecoilTimer > 0.0f)
        {
            g_ShotgunRecoilTimer -= delta_t;

            if (g_ShotgunRecoilTimer < 0.0f)
                g_ShotgunRecoilTimer = 0.0f;
        }

        for (BigfootInstance& instance : g_Bigfoots)
        {
            if (instance.enemy.IsDead() && instance.death_animation_started)
                instance.death_timer += delta_t;
        }

        if (g_PlayerFallAnimationStarted)
            g_PlayerFallTimer += delta_t;

        if (g_GameState.status == GameStatus::Won &&
            g_SpectatorMode &&
            g_SpectatorAutoAdvanceTimer >= 0.0f)
        {
            g_SpectatorAutoAdvanceTimer -= delta_t;

            if (g_SpectatorAutoAdvanceTimer <= 0.0f)
            {
                bool keep_aggressive = g_SpectatorAggressiveMode;
                ResetGame(true);
                g_SpectatorMode = true;
                g_SpectatorAggressiveMode = keep_aggressive;
                g_SpectatorWantsShoot = false;
                g_SpectatorRunning = false;
                g_SpectatorMovementDirection = glm::vec3(0.0f, 0.0f, 0.0f);
                g_SpectatorLastPosition = glm::vec3(0.0f, 0.0f, 0.0f);
                g_SpectatorDetourDirection = glm::vec3(0.0f, 0.0f, 0.0f);
                g_SpectatorHasLastPosition = false;
                g_SpectatorStuckTimer = 0.0f;
                g_SpectatorDetourTimer = 0.0f;
                g_SpectatorTransitMode = 0;
                g_SpectatorTransitPortal = -1;
                g_SpectatorTransitDoor = -1;
                g_SpectatorAutoAdvanceTimer = -1.0f;
                g_SpectatorAutoRetryTimer = -1.0f;
            }
        }

        if (g_GameState.status == GameStatus::Lost &&
            g_SpectatorMode &&
            g_SpectatorAutoRetryTimer >= 0.0f)
        {
            g_SpectatorAutoRetryTimer -= delta_t;

            if (g_SpectatorAutoRetryTimer <= 0.0f)
            {
                g_SelectedPrestigeLevel = ClampPrestigeLevel(g_RunPrestigeLevel);
                bool keep_aggressive = g_SpectatorAggressiveMode;
                ResetGame(true);
                g_SpectatorMode = true;
                g_SpectatorAggressiveMode = keep_aggressive;
                g_SpectatorWantsShoot = false;
                g_SpectatorRunning = false;
                g_SpectatorMovementDirection = glm::vec3(0.0f, 0.0f, 0.0f);
                g_SpectatorLastPosition = glm::vec3(0.0f, 0.0f, 0.0f);
                g_SpectatorDetourDirection = glm::vec3(0.0f, 0.0f, 0.0f);
                g_SpectatorHasLastPosition = false;
                g_SpectatorStuckTimer = 0.0f;
                g_SpectatorDetourTimer = 0.0f;
                g_SpectatorTransitMode = 0;
                g_SpectatorTransitPortal = -1;
                g_SpectatorTransitDoor = -1;
                g_SpectatorAutoAdvanceTimer = -1.0f;
                g_SpectatorAutoRetryTimer = -1.0f;
            }
        }

        UpdateSpectatorController(delta_t);

        // Câmera na cabeça do Pé Grande: ativa enquanto o jogador segura Alt,
        // tem segundos de visão acumulados e existe um Pé Grande vivo. Drena os
        // segundos a cada frame e desliga sozinha ao zerar. Indisponível no modo
        // espectador (piloto-automático de IA).
        {
            bool alt_held =
                glfwGetKey(window, GLFW_KEY_LEFT_ALT) == GLFW_PRESS ||
                glfwGetKey(window, GLFW_KEY_RIGHT_ALT) == GLFW_PRESS;

            glm::vec4 cam_pos = g_Camera.GetPosition();

            // Contexto que pertence ao main: o jogador está pedindo a câmera
            // (ALT durante o jogo, fora do espectador/mapa) e há um Pé Grande
            // alvo por perto. A política do recurso (segundos de visão) e o
            // consumo ficam dentro de UpdateBigfootCam.
            bool wants_cam =
                g_GameState.status == GameStatus::Playing &&
                !g_SpectatorMode &&
#if MAP_VIEW_ENABLED
                !g_MapView.IsActive() &&
#endif
                alt_held;

            bool target_available =
                NearestLiveBigfootIndex(glm::vec3(cam_pos.x, cam_pos.y, cam_pos.z)) >= 0;

            UpdateBigfootCam(wants_cam, target_available, delta_t);
        }

        bool movement_key_pressed =
            glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS ||
            (g_SpectatorMode &&
             (GetSpectatorMovementDirection().x != 0.0f || GetSpectatorMovementDirection().z != 0.0f));

        bool running_key_pressed =
            glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
            glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS ||
            (g_SpectatorMode && IsSpectatorRunning());

        if (g_GameState.status == GameStatus::Playing && movement_key_pressed)
        {
            g_CameraBobTimer += delta_t * (running_key_pressed ? 11.5f : 7.2f);

            float target_bob = running_key_pressed ? 1.0f : 0.45f;
            g_CameraBobAmount += (target_bob - g_CameraBobAmount) * 8.0f * delta_t;
        }
        else
        {
            g_CameraBobAmount += (0.0f - g_CameraBobAmount) * 8.0f * delta_t;
        }

        if (g_GameState.status == GameStatus::Playing
#if MAP_VIEW_ENABLED
            && !g_MapView.IsActive()
#endif
           )
        {
            // Snapshot do estado dos coletáveis para detectar pickups e alertar
            // o Pé Grande para a posição onde o jogador "fez barulho".
            std::vector<Collectible>& collectibles_before = GetSceneCollectibles();
            std::vector<bool> was_collected;
            was_collected.reserve(collectibles_before.size());
            for (const Collectible& c : collectibles_before)
                was_collected.push_back(c.collected);

            // No Modo Monstro o jogador continua podendo andar com WASD (apenas
            // não atira); o corpo do caçador anima as pernas conforme se move.
            if (g_SpectatorMode)
                g_Player.UpdateAutonomous(GetSpectatorMovementDirection(), IsSpectatorRunning(), delta_t);
            else
                g_Player.Update(window, delta_t);

            const std::vector<Collectible>& collectibles_after = GetSceneCollectibles();
            for (size_t i = 0; i < collectibles_after.size() && i < was_collected.size(); ++i)
            {
                if (!was_collected[i] && collectibles_after[i].collected)
                {
                    // 10 segundos cobrem ~45m a velocidade de ronda — atravessa
                    // boa parte do mapa sem deixá-lo eternamente "obcecado".
                    (void)collectibles_after;
                }
            }
        }

        glm::vec4 player_position = g_Camera.GetPosition();

        // Intensidade de caminhada do jogador, derivada do deslocamento real.
        // Anima as pernas do corpo do caçador exibido no Modo Monstro.
        if (g_GameState.status == GameStatus::Playing)
        {
            glm::vec3 cur = glm::vec3(player_position.x, player_position.y, player_position.z);
            float target_intensity = 0.0f;

            if (g_PlayerHasPrevPosition && delta_t > 0.0001f)
            {
                glm::vec3 moved = cur - g_PlayerPrevPosition;
                moved.y = 0.0f;
                float speed = sqrt(moved.x*moved.x + moved.z*moved.z) / delta_t;
                target_intensity = speed / 5.8f; // normaliza pela velocidade de caminhada
                if (target_intensity > 1.0f)
                    target_intensity = 1.0f;
            }

            float blend = 1.0f - exp(-delta_t * 8.0f);
            g_PlayerWalkIntensity += (target_intensity - g_PlayerWalkIntensity) * blend;
            g_PlayerPrevPosition = cur;
            g_PlayerHasPrevPosition = true;
        }

        if (g_GameState.status == GameStatus::Playing
#if MAP_VIEW_ENABLED
            && !g_MapView.IsActive()
#endif
#if BIGFOOT_FREEZE_DEBUG_ENABLED
            && !g_BigfootFrozen
#endif
           )
        {
            for (BigfootInstance& instance : g_Bigfoots)
            {
                instance.enemy.Update(
                    glm::vec3(player_position.x, player_position.y, player_position.z),
                    delta_t
                );
            }
        }

        bool bigfoot_attacking = false;

        for (const BigfootInstance& instance : g_Bigfoots)
        {
            if (instance.enemy.GetState() == BigfootState::Attacking)
            {
                bigfoot_attacking = true;
                break;
            }
        }

        if (g_GameState.status == GameStatus::Playing && bigfoot_attacking)
        {
            PlayGameSound(GameSound::BigfootKillsPlayer);
            g_PlayerFallAnimationStarted = true;
            g_PlayerFallTimer = 0.0f;
            g_GameState.status = GameStatus::Lost;

            if (g_SpectatorMode)
                g_SpectatorAutoRetryTimer = 2.0f;
        }

        if (g_GameState.status == GameStatus::Playing &&
            AllCollectiblesCollected() &&
            IsPlayerInsideSafeZone(player_position))
        {
            SetGameWon();
        }

        // Tiro com o botão esquerdo do mouse.
        bool shoot_button_pressed =
            glfwGetMouseButton(window, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS ||
            (g_SpectatorMode && ShouldSpectatorShoot() && g_ShotgunRecoilTimer <= 0.0f);

        if (g_GameState.status == GameStatus::Playing &&
            !IsBigfootCamActive() &&
            shoot_button_pressed &&
            !g_ShootButtonWasPressed &&
            g_ShotgunRecoilTimer <= 0.0f)
        {
            float reload_mult = GetUpgradeValue(UpgradeId::ReloadSpeed);
            if (reload_mult < 0.1f) reload_mult = 0.1f;
            g_ShotgunCurrentRecoilDuration = g_Player.IsEnergyBoostActive()
                ? SHOTGUN_RECOIL_DURATION * 0.5f / reload_mult
                : SHOTGUN_RECOIL_DURATION / reload_mult;
            g_ShotgunRecoilTimer = g_ShotgunCurrentRecoilDuration;
            PlayGameSound(GameSound::Shotgun);

            int hit_bigfoot_index = ShotHitsBigfoot(g_Camera.GetPosition(), g_Camera.GetViewVector());

            if (hit_bigfoot_index >= 0)
            {
                printf("Acertou o Pe Grande!\n");

                BigfootInstance& hit_bigfoot = g_Bigfoots[(size_t)hit_bigfoot_index];

                hit_bigfoot.enemy.TakeDamage(
                    12.5f,
                    glm::vec3(player_position.x, player_position.y, player_position.z)
                );

                if (hit_bigfoot.enemy.IsDead())
                {
                    hit_bigfoot.death_animation_started = true;
                    hit_bigfoot.death_timer = 0.0f;
                    PlayGameSound(GameSound::BigfootDies);

                    if (AreAllBigfootsDead())
                        SetGameWon();
                }
                else
                {
                    PlayRandomBigfootRoar();
                }
            }
            else
            {
                printf("Errou o tiro.\n");
            }
        }

        g_ShootButtonWasPressed = shoot_button_pressed;

        // Aqui executamos as operações de renderização

        // Definimos a cor do "fundo" do framebuffer como branco.  Tal cor é
        // definida como coeficientes RGBA: Red, Green, Blue, Alpha; isto é:
        // Vermelho, Verde, Azul, Alpha (valor de transparência).
        // Conversaremos sobre sistemas de cores nas aulas de Modelos de Iluminação.
        //
        //           R     G     B     A
#if DAY_MODE_DEBUG_ENABLED
        if (g_DayMode)
            glClearColor(0.53f, 0.75f, 0.92f, 1.0f); // céu de dia, azul claro
        else
#endif
            glClearColor(0.015f, 0.018f, 0.026f, 1.0f);

        // "Pintamos" todos os pixels do framebuffer com a cor definida acima,
        // e também resetamos todos os pixels do Z-buffer (depth buffer).
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Pedimos para a GPU utilizar o programa de GPU criado acima (contendo
        // os shaders de vértice e fragmentos).
        glUseProgram(g_GpuProgramID);


        glm::vec4 camera_position_c  = g_Camera.GetPosition();
        glm::vec4 camera_view_vector = g_Camera.GetViewVector();
        glm::vec4 camera_up_vector   = g_Camera.GetUpVector();

        glm::mat4 view;
        glm::mat4 projection;

#if MAP_VIEW_ENABLED
        if (g_MapView.IsActive())
        {
            view       = g_MapView.GetViewMatrix();
            projection = g_MapView.GetProjectionMatrix(screenRatio);
        }
        else
#endif
        {
            // Computamos a matriz "View" utilizando os parâmetros da câmera para
            // definir o sistema de coordenadas da câmera.  Veja slides 2-14, 184-190 e 236-242 do documento Aula_08_Sistemas_de_Coordenadas.pdf.
            int bigfoot_cam_index = IsBigfootCamActive()
                ? NearestLiveBigfootIndex(glm::vec3(camera_position_c.x, camera_position_c.y, camera_position_c.z))
                : -1;

            if (IsBigfootCamActive() && bigfoot_cam_index >= 0)
            {
                // Câmera posicionada na cabeça do Pé Grande vivo mais próximo,
                // olhando na direção em que ele anda. O jogador apenas observa
                // (não controla o Pé Grande), então não aplicamos bob nem queda.
                const BigfootInstance& bf = g_Bigfoots[(size_t)bigfoot_cam_index];
                view = ComputeBigfootCamView(bf.enemy.GetPosition(), bf.render_yaw);
            }
            else
            {
            view = Matrix_Camera_View(camera_position_c, camera_view_vector, camera_up_vector);

            if (!g_PlayerFallAnimationStarted && g_CameraBobAmount > 0.001f)
            {
                float vertical_bob = sin(g_CameraBobTimer) * 0.045f * g_CameraBobAmount;
                float roll_bob = sin(g_CameraBobTimer * 0.5f) * 0.018f * g_CameraBobAmount;

                view = Matrix_Rotate_Z(roll_bob)
                    * Matrix_Translate(0.0f, vertical_bob, 0.0f)
                    * view;
            }

            if (g_PlayerFallAnimationStarted)
            {
                float fall = g_PlayerFallTimer / 1.10f;

                if (fall > 1.0f)
                    fall = 1.0f;

                fall = fall * fall * (3.0f - 2.0f * fall);

                // O olho desce até perto do chão (de ~1.7 para ~0.30), como se o
                // jogador caísse, e só então aplicamos o tombamento para o lado.
                const float ground_eye_y = 0.55f;
                glm::vec4 fallen_position = camera_position_c;
                fallen_position.y = camera_position_c.y
                    + (ground_eye_y - camera_position_c.y) * fall;

                view = Matrix_Camera_View(fallen_position, camera_view_vector, camera_up_vector);

                view = Matrix_Rotate_Z(1.35f * fall)
                    * Matrix_Rotate_X(-0.65f * fall)
                    * view;
            }
            }

            // Agora computamos a matriz de Projeção.
            // Note que, no sistema de coordenadas da câmera, os planos near e far
            // estão no sentido negativo! Veja slides 176-204 do documento Aula_09_Projecoes.pdf.
            float nearplane = -0.1f;  // Posição do "near plane"
            float farplane  = -220.0f; // Posição do "far plane"

            if (g_UsePerspectiveProjection)
            {
                // Projeção Perspectiva.
                // Para definição do field of view (FOV), veja slides 205-215 do documento Aula_09_Projecoes.pdf.
                float field_of_view = 3.141592 / 3.0f;
                projection = Matrix_Perspective(field_of_view, screenRatio, nearplane, farplane);
            }
            else
            {
                // Projeção Ortográfica.
                // Para definição dos valores l, r, b, t ("left", "right", "bottom", "top"),
                // PARA PROJEÇÃO ORTOGRÁFICA veja slides 219-224 do documento Aula_09_Projecoes.pdf.
                // Para simular um "zoom" ortográfico, computamos o valor de "t"
                // utilizando a variável g_CameraDistance.
                float t = 1.5f*g_CameraDistance/2.5f;
                float b = -t;
                float r = t*screenRatio;
                float l = -r;
                projection = Matrix_Orthographic(l, r, b, t, nearplane, farplane);
            }
        }

        glm::mat4 model = Matrix_Identity(); // Transformação identidade de modelagem

        // Enviamos as matrizes "view" e "projection" para a placa de vídeo
        // (GPU). Veja o arquivo "shader_vertex.glsl", onde estas são
        // efetivamente aplicadas em todos os pontos.
        glUniformMatrix4fv(g_view_uniform       , 1 , GL_FALSE , glm::value_ptr(view));
        glUniformMatrix4fv(g_projection_uniform , 1 , GL_FALSE , glm::value_ptr(projection));
#if MAP_VIEW_ENABLED
        glUniform1i(g_map_view_uniform, g_MapView.IsActive() ? 1 : 0);
#endif
        // Liga o filtro de fúria da "Visão do Monstro" quando a câmera do Pé
        // Grande está ativa; u_time alimenta as scanlines/grão e u_resolution
        // permite a vinheta.
        glUniform1i(g_monster_vision_uniform, IsBigfootCamActive() ? 1 : 0);
        glUniform1f(g_time_uniform, current_time);
        {
            int fb_w, fb_h;
            glfwGetFramebufferSize(window, &fb_w, &fb_h);
            glUniform2f(g_resolution_uniform, (float)fb_w, (float)fb_h);
        }

        #define SPHERE 0
        #define BUNNY  1
        #define PLANE  2
        #define SAFE_ZONE 3
        #define BIGFOOT 4

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

        for (size_t i = 0; i < g_Bigfoots.size(); ++i)
        {
            BigfootInstance& instance = g_Bigfoots[i];
            glm::vec3 bigfoot_position = instance.enemy.GetPosition();
            float bigfoot_yaw = UpdateBigfootFacing(i, bigfoot_position, delta_t);
            float bigfoot_death_progress = instance.death_animation_started ? instance.death_timer / 1.15f : 0.0f;

            DrawBigfootModel(bigfoot_position, bigfoot_yaw, current_time, bigfoot_death_progress, instance.movement_intensity);

            if (!has_map_bigfoot && !instance.enemy.IsDead())
            {
                map_bigfoot_position = bigfoot_position;
                map_bigfoot_yaw = bigfoot_yaw;
                has_map_bigfoot = true;
            }
        }

        // Esfera de debug da hitbox do Pé Grande.
        // Usamos o mesmo raio que será usado para tiro/colisão.
        if (g_DrawBigfootHitSphere)
        {
            std::vector<BoxObstacle> shot_boxes = GetBigfootShotBoxes();

            glDisable(GL_CULL_FACE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

            for (const BigfootInstance& instance : g_Bigfoots)
            {
                glm::vec3 bigfoot_position = instance.enemy.GetPosition();

                for (const BoxObstacle& shot_box : shot_boxes)
                {
                    model = Matrix_Translate(bigfoot_position.x, bigfoot_position.y, bigfoot_position.z)
                        * Matrix_Rotate_Y(instance.render_yaw)
                        * Matrix_Translate(shot_box.center.x, shot_box.center.y, shot_box.center.z)
                        * Matrix_Scale(shot_box.size.x, shot_box.size.y, shot_box.size.z);

                    glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
                    glUniform1i(g_object_id_uniform, SAFE_ZONE); // Reaproveita o verde do shader.
                    DrawVirtualObject("the_cube");
                }
            }

            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glEnable(GL_CULL_FACE);
        }

        // Desenhamos os itens coletáveis.
        // Por enquanto usamos esferas pequenas como placeholder visual.
        std::vector<Collectible>& collectibles = GetSceneCollectibles();

        for (const Collectible& collectible : collectibles)
        {
            if (collectible.collected)
                continue;

            float bob = 0.18f * sin(current_time * 2.4f + collectible.center.x * 0.37f);

            model = Matrix_Translate(collectible.center.x, 0.18f + bob, collectible.center.z)
                * Matrix_Rotate_Y(current_time * 1.9f)
                * Matrix_Rotate_Z(0.22f)
                * Matrix_Scale(0.34f, 0.34f, 0.34f);

            glUniformMatrix4fv(g_model_uniform, 1, GL_FALSE, glm::value_ptr(model));
            glUniform1i(g_object_id_uniform, OBJECT_MONSTER_DRINK);
            DrawVirtualObject("pCylinder2");
        }

        // Desenhamos a zona segura/final somente depois que todos os itens forem coletados.
        if (AllCollectiblesCollected())
        {
            const SafeZone& safe_zone = GetSafeZone();

            DrawSafeZoneBeacon(safe_zone, current_time);
        }

#if MAP_VIEW_ENABLED
        if (g_MapView.IsActive())
        {
            DrawMapViewOverlay(window, camera_position_c, camera_view_vector, map_bigfoot_position, map_bigfoot_yaw);
        }
#endif

        // Imprimimos na tela os ângulos de Euler que controlam a rotação do
        // terceiro cubo.
        if (g_GameState.status != GameStatus::MainMenu &&
            g_GameState.status != GameStatus::UpgradeShop &&
            g_GameState.status != GameStatus::ConfirmReset
#if MAP_VIEW_ENABLED
            && !g_MapView.IsActive()
#endif
           )
        {
            if (IsBigfootCamActive())
            {
                // No Modo Monstro vemos o jogador de fora (pela cabeça do Pé
                // Grande), então desenhamos o corpo do caçador na posição dele
                // em vez do viewmodel de arma em primeira pessoa.
                float player_yaw = atan2(camera_view_vector.x, camera_view_vector.z);
                glm::vec3 player_feet = glm::vec3(
                    camera_position_c.x,
                    camera_position_c.y - 1.7f,
                    camera_position_c.z
                );
                DrawPlayerModel(player_feet, player_yaw, current_time, g_PlayerWalkIntensity,
                    g_ShotgunRecoilTimer, g_ShotgunCurrentRecoilDuration);
            }
            else if (!g_PlayerFallAnimationStarted)
            {
                // Ao morrer (animação de queda), a arma some junto com o jogador.
                DrawFirstPersonWeapon(camera_position_c, camera_view_vector, camera_up_vector, g_ShotgunRecoilTimer, g_ShotgunCurrentRecoilDuration);
            }
        }

        // Filtro esverdeado de "adrenalina" enquanto o energy boost estiver ativo.
        if (g_GameState.status == GameStatus::Playing && g_Player.IsEnergyBoostActive())
        {
            int fb_w, fb_h;
            glfwGetFramebufferSize(window, &fb_w, &fb_h);

            float remaining = g_Player.GetEnergyBoostTimeRemaining();
            float fade_out  = g_Player.IsInfiniteBoostCheatActive()
                ? 1.0f
                : (remaining < 0.6f ? remaining / 0.6f : 1.0f);
            float pulse     = 0.5f + 0.5f * sin((float)glfwGetTime() * 4.5f);
            float alpha     = (0.12f + 0.05f * pulse) * fade_out;

            TextRendering_DrawRectPx(window, 0, 0, fb_w, fb_h, 0.20f, 1.00f, 0.35f, alpha);
        }

        // (O antigo tint vermelho 2D do Modo Monstro foi substituído pelo filtro
        // térmico/infravermelho aplicado por fragmento em shader_fragment.glsl,
        // via uniform u_monster_vision_active.)

        // TextRendering_ShowEulerAngles(window);

        // Imprimimos na informação sobre a matriz de projeção sendo utilizada.
        // TextRendering_ShowProjection(window);

        // Imprimimos na tela informação sobre o número de quadros renderizados
        // por segundo (frames per second).
        TextRendering_ShowFramesPerSecond(window);

#if SHOW_COORDS_DEBUG_ENABLED
        if (g_ShowCoordsDebug)
        {
            glm::vec4 dbg_pos = g_Camera.GetPosition();
            char coords_buf[64];
            int coords_chars = snprintf(coords_buf, sizeof(coords_buf),
                "X=%.2f Y=%.2f Z=%.2f", dbg_pos.x, dbg_pos.y, dbg_pos.z);
            float lh = TextRendering_LineHeight(window);
            float cw = TextRendering_CharWidth(window);
            TextRendering_PrintString(window, coords_buf,
                1.0f - (coords_chars + 1) * cw, 1.0f - 2.0f * lh, 1.0f);
        }
#endif

        if (g_GameState.status == GameStatus::MainMenu)
        {
            int max_visible_levels = 6;
            int first_level = g_SelectedPrestigeLevel - max_visible_levels / 2;

            if (first_level < 0)
                first_level = 0;

            if (first_level > g_HighestUnlockedPrestigeLevel - max_visible_levels + 1)
                first_level = g_HighestUnlockedPrestigeLevel - max_visible_levels + 1;

            if (first_level < 0)
                first_level = 0;

            int last_level = first_level + max_visible_levels - 1;

            if (last_level > g_HighestUnlockedPrestigeLevel)
                last_level = g_HighestUnlockedPrestigeLevel;

            int visible_rows = last_level - first_level + 1;
            int selected_row_in_view = g_SelectedPrestigeLevel - first_level;

            DrawMainMenuPanel(window, selected_row_in_view, visible_rows);

            TextRendering_PrintString(window, "Pe Grande do Vale", -0.70f, 0.28f, 1.55f);
            TextRendering_PrintString(window, "Colete os energeticos e fuja para a zona segura.", -0.70f, 0.14f, 1.0f);

            // Cabecalho e colunas alinhadas (fonte e proporcional, entao posicionamos
            // cada coluna em um x fixo em NDC).
            float col_marker = -0.58f;
            float col_level  = -0.50f;
            float col_status = -0.32f;
            float col_pes    = -0.12f;
            float col_latas  =  0.02f;
            float col_vida   =  0.18f;
            float col_vel    =  0.40f;
            float header_scale = 0.80f;
            float row_scale    = 0.78f;

            TextRendering_PrintString(window, "Nivel",      col_level,  -0.08f, header_scale);
            TextRendering_PrintString(window, "Status",     col_status, -0.08f, header_scale);
            TextRendering_PrintString(window, "Pes",        col_pes,    -0.08f, header_scale);
            TextRendering_PrintString(window, "Latas",      col_latas,  -0.08f, header_scale);
            TextRendering_PrintString(window, "Vida",       col_vida,   -0.08f, header_scale);
            TextRendering_PrintString(window, "Velocidade", col_vel,    -0.08f, header_scale);

            for (int level = first_level; level <= last_level; ++level)
            {
                float health_multiplier = GetPrestigeHealthMultiplierForLevel(level);
                float speed_multiplier = GetPrestigeSpeedMultiplierForLevel(level);
                bool is_selected = (level == g_SelectedPrestigeLevel);
                bool is_frontier = (level == g_HighestUnlockedPrestigeLevel);
                const char* marker = is_selected ? ">>" : "  ";
                const char* status = is_frontier ? "NOVO" : "vencido";

                float row_y = -0.17f - (level - first_level) * 0.085f;

                char buf[32];

                TextRendering_PrintString(window, marker, col_marker, row_y, row_scale);

                snprintf(buf, sizeof(buf), "%02d", level + 1);
                TextRendering_PrintString(window, buf, col_level, row_y, row_scale);

                TextRendering_PrintString(window, status, col_status, row_y, row_scale);

                snprintf(buf, sizeof(buf), "%d", GetBigfootCountForLevel(level));
                TextRendering_PrintString(window, buf, col_pes, row_y, row_scale);

                snprintf(buf, sizeof(buf), "%d", GetPrestigeCollectibleCountForLevel(level));
                TextRendering_PrintString(window, buf, col_latas, row_y, row_scale);

                snprintf(buf, sizeof(buf), "x%.2f", health_multiplier);
                TextRendering_PrintString(window, buf, col_vida, row_y, row_scale);

                snprintf(buf, sizeof(buf), "x%.2f", speed_multiplier);
                TextRendering_PrintString(window, buf, col_vel, row_y, row_scale);
            }

            TextRendering_PrintString(window, "[SPACE] iniciar  [W/S] nivel  [U] loja", -0.52f, -0.66f, 0.92f);
            TextRendering_PrintString(window, "[X] Resetar progresso", -0.22f, -0.76f, 0.86f);

            char coins_hud[64];
            snprintf(coins_hud, sizeof(coins_hud), "Pontos: %d", GetRawCoins());
            TextRendering_PrintString(window, coins_hud, -0.95f, 0.82f, 1.12f);
        }
        else if (g_GameState.status == GameStatus::UpgradeShop)
        {
            DrawUpgradeShopOverlay(window);
        }
        else if (g_GameState.status == GameStatus::ConfirmReset)
        {
            DrawConfirmResetOverlay(window);
        }

        // Barra textual de vida do Pé Grande no topo da tela.
        if (g_GameState.status != GameStatus::MainMenu &&
            g_GameState.status != GameStatus::UpgradeShop &&
            g_GameState.status != GameStatus::ConfirmReset)
        {
        float total_bigfoot_health = 0.0f;
        float total_bigfoot_max_health = 0.0f;

        for (const BigfootInstance& instance : g_Bigfoots)
        {
            total_bigfoot_health += instance.enemy.GetHealth();
            total_bigfoot_max_health += instance.enemy.GetMaxHealth();
        }

        float health_ratio = (total_bigfoot_max_health > 0.0f)
            ? total_bigfoot_health / total_bigfoot_max_health
            : 0.0f;

        if (health_ratio < 0.0f)
            health_ratio = 0.0f;

        if (health_ratio > 1.0f)
            health_ratio = 1.0f;

        DrawBigfootHealthBar(window, health_ratio);

        if (g_Player.IsEnergyBoostActive())
        {
            char boost_text[64];
            if (g_Player.IsInfiniteBoostCheatActive())
                snprintf(boost_text, sizeof(boost_text), "ENERGIA x2");
            else
                snprintf(boost_text, sizeof(boost_text), "ENERGIA x2  %.1fs", g_Player.GetEnergyBoostTimeRemaining());

            TextRendering_PrintString(
                window,
                boost_text,
                -0.19f,
                -0.72f,
                1.0f
            );
        }

        // HUD temporário dos coletáveis.
        std::vector<Collectible>& hud_collectibles = GetSceneCollectibles();

        int collected_count = 0;
        int total_count = (int)hud_collectibles.size();

        for (const Collectible& collectible : hud_collectibles)
        {
            if (collectible.collected)
                collected_count++;
        }

        char collectibles_text[32];
        snprintf(
            collectibles_text,
            32,
            "Coletados: %d/%d",
            collected_count,
            total_count
        );

        TextRendering_PrintString(
            window,
            collectibles_text,
            -0.95f,
            0.82f,
            1.12f
        );

        char coins_hud[64];
        snprintf(coins_hud, sizeof(coins_hud), "Pontos: %d", GetRawCoins());
        TextRendering_PrintString(window, coins_hud, -0.95f, 0.72f, 1.02f);

        // Segundos de visão acumulados (recurso da câmera do Pé Grande, tecla Alt).
        char vision_hud[64];
        snprintf(vision_hud, sizeof(vision_hud), "Modo Monstro: %.1fs", GetVisionSeconds());
        TextRendering_PrintString(window, vision_hud, -0.95f, 0.62f, 1.02f);

        // Indicador quando o jogador está observando pela cabeça do Pé Grande.
        if (IsBigfootCamActive())
        {
            char bigfoot_cam_text[64];
            snprintf(
                bigfoot_cam_text,
                sizeof(bigfoot_cam_text),
                "MODO MONSTRO  %.1fs",
                GetVisionSeconds()
            );
            TextRendering_PrintString(window, bigfoot_cam_text, -0.32f, 0.82f, 1.0f);
        }

        if (g_GameState.status == GameStatus::Playing && g_SpectatorMode)
        {
            TextRendering_PrintString(
                window,
                g_SpectatorAggressiveMode ? "SPECTATOR IA AGRESSIVA" : "SPECTATOR IA",
                g_SpectatorAggressiveMode ? 0.42f : 0.58f,
                0.82f,
                g_SpectatorAggressiveMode ? 0.86f : 1.0f
            );
        }
        else if (g_GameState.status == GameStatus::Won &&
                 g_SpectatorMode &&
                 g_SpectatorAutoAdvanceTimer >= 0.0f)
        {
            char spectator_next_text[64];
            snprintf(
                spectator_next_text,
                sizeof(spectator_next_text),
                "SPECTATOR: proximo nivel em %.1fs",
                g_SpectatorAutoAdvanceTimer
            );
            TextRendering_PrintString(window, spectator_next_text, -0.34f, 0.44f, 0.98f);
        }
        else if (g_GameState.status == GameStatus::Lost &&
                 g_SpectatorMode &&
                 g_SpectatorAutoRetryTimer >= 0.0f)
        {
            char spectator_retry_text[64];
            snprintf(
                spectator_retry_text,
                sizeof(spectator_retry_text),
                "SPECTATOR: tentando de novo em %.1fs",
                g_SpectatorAutoRetryTimer
            );
            TextRendering_PrintString(window, spectator_retry_text, -0.38f, 0.44f, 0.98f);
        }

        if (g_GameState.status == GameStatus::Won)
        {
            char win_prestige_text[64];

            if (g_LastWinUnlockedNewLevel)
            {
                snprintf(
                    win_prestige_text,
                    sizeof(win_prestige_text),
                    "Nivel %d liberado.",
                    g_HighestUnlockedPrestigeLevel + 1
                );
            }
            else
            {
                snprintf(
                    win_prestige_text,
                    sizeof(win_prestige_text),
                    "Nivel %d concluido.",
                    g_RunPrestigeLevel + 1
                );
            }

            TextRendering_PrintString(
                window,
                "VITORIA!",
                -0.35f,
                0.80f,
                1.35f
            );
            TextRendering_PrintString(
                window,
                win_prestige_text,
                -0.38f,
                0.68f,
                1.08f
            );
            TextRendering_PrintString(
                window,
                "Aperte R para voltar ao menu.",
                -0.42f,
                0.56f,
                1.08f
            );
        }
        else if (g_GameState.status == GameStatus::Lost)
        {
            TextRendering_PrintString(
                window,
                "DERROTA! Foi papado",
                -0.40f,
                0.80f,
                1.35f
            );
            TextRendering_PrintString(
                window,
                "Aperte R para voltar ao menu.",
                -0.42f,
                0.68f,
                1.08f
            );
        }
        else if (collected_count == total_count)
        {
            TextRendering_PrintString(
                window,
                "Volte para a zona segura.",
                -0.40f,
                0.80f,
                1.12f
            );
        }

        // Mira simples no centro da tela.
#if MAP_VIEW_ENABLED
        if (!g_MapView.IsActive())
#endif
        TextRendering_PrintString(
            window,
            "x",
            -0.01f,
            0.0f,
            1.5f
        );
        }

        // O framebuffer onde OpenGL executa as operações de renderização não
        // é o mesmo que está sendo mostrado para o usuário, caso contrário
        // seria possível ver artefatos conhecidos como "screen tearing". A
        // chamada abaixo faz a troca dos buffers, mostrando para o usuário
        // tudo que foi renderizado pelas funções acima.
        // Veja o link: https://en.wikipedia.org/w/index.php?title=Multiple_buffering&oldid=793452829#Double_buffering_in_computer_graphics
        glfwSwapBuffers(window);

        // Verificamos com o sistema operacional se houve alguma interação do
        // usuário (teclado, mouse, ...). Caso positivo, as funções de callback
        // definidas anteriormente usando glfwSet*Callback() serão chamadas
        // pela biblioteca GLFW.
        glfwPollEvents();
    }

    StopBackgroundMusic();

    // Finalizamos o uso dos recursos do sistema operacional
    glfwTerminate();

    // Fim do programa
    return 0;
}