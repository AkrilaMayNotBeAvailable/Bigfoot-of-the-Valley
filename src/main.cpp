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

    // Habilitamos o Z-buffer. Veja slides 104-116 do documento Aula_09_Projecoes.pdf.
    glEnable(GL_DEPTH_TEST);
    // Habilitamos o Backface Culling. Veja slides 8-13 do documento Aula_02_Fundamentos_Matematicos.pdf, slides 23-34 do documento Aula_13_Clipping_and_Culling.pdf e slides 112-123 do documento Aula_14_Laboratorio_3_Revisao.pdf.
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);

    float prev_time = (float)glfwGetTime();
    while(!glfwWindowShouldClose(window)){
        float current_time = (float)glfwGetTime();
        float delta_t = current_time - prev_time;
        prev_time = current_time;

        ShotgunRecoilCounter(delta_t); // EXTRACTED FUNCTION

        for(BigfootInstance& instance : g_Bigfoots){
            if(instance.enemy.IsDead() && instance.death_animation_started)
                instance.death_timer += delta_t;
        }

        if(g_PlayerFallAnimationStarted)
            g_PlayerFallTimer += delta_t;

        SpectatorMechanic(delta_t); // EXTRACTED FUNCTION
        UpdateSpectatorController(delta_t);
        UpdateBigfootCamRequest(window, delta_t); // EXTRACTED FUNCTION

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

        if (g_GameState.status == GameStatus::Playing && movement_key_pressed){
            g_CameraBobTimer += delta_t * (running_key_pressed ? 11.5f : 7.2f);

            float target_bob = running_key_pressed ? 1.0f : 0.45f;
            g_CameraBobAmount += (target_bob - g_CameraBobAmount) * 8.0f * delta_t;
        }
        else{
            g_CameraBobAmount += (0.0f - g_CameraBobAmount) * 8.0f * delta_t;
        }

        InGameUpdateMovementAndCollectibles(window, delta_t);

        glm::vec4 player_position = g_Camera.GetPosition();
        UpdatePlayerWalkIntensity(delta_t, player_position);

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

        for(const BigfootInstance& instance : g_Bigfoots){
            if (instance.enemy.GetState() == BigfootState::Attacking){
                bigfoot_attacking = true;
                break;
            }
        }

        if(g_GameState.status == GameStatus::Playing && bigfoot_attacking){
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

            if(g_PlayerFallAnimationStarted){
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

            if(g_UsePerspectiveProjection){
                // Projeção Perspectiva.
                // Para definição do field of view (FOV), veja slides 205-215 do documento Aula_09_Projecoes.pdf.
                float field_of_view = 3.141592 / 3.0f;
                projection = Matrix_Perspective(field_of_view, screenRatio, nearplane, farplane);
            } else {
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

        for(size_t i = 0; i < g_Bigfoots.size(); ++i){
            BigfootInstance& instance = g_Bigfoots[i];
            glm::vec3 bigfoot_position = instance.enemy.GetPosition();
            float bigfoot_yaw = UpdateBigfootFacing(i, bigfoot_position, delta_t);
            float bigfoot_death_progress = instance.death_animation_started ? instance.death_timer / 1.15f : 0.0f;

            DrawBigfootModel(bigfoot_position, bigfoot_yaw, current_time, bigfoot_death_progress, instance.movement_intensity);

            if(!has_map_bigfoot && !instance.enemy.IsDead()){
                map_bigfoot_position = bigfoot_position;
                map_bigfoot_yaw = bigfoot_yaw;
                has_map_bigfoot = true;
            }
        }

        // Esfera de debug da hitbox do Pé Grande.
        // Usamos o mesmo raio que será usado para tiro/colisão.
        if(g_DrawBigfootHitSphere){
            std::vector<BoxObstacle> shot_boxes = GetBigfootShotBoxes();

            glDisable(GL_CULL_FACE);
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

            for(const BigfootInstance& instance : g_Bigfoots){
                glm::vec3 bigfoot_position = instance.enemy.GetPosition();

                for(const BoxObstacle& shot_box : shot_boxes){
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

        for(const Collectible& collectible : collectibles){
            if(collectible.collected)
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
        if(AllCollectiblesCollected()){
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
        DrawMainMenu(window);
        TextDrawingChunk(window);

        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    StopBackgroundMusic();
    glfwTerminate();

    return 0;
}