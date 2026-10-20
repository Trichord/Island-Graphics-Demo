#pragma once

#include "Jolt/Jolt.h"

#include <Jolt/Physics/PhysicsSystem.h>
#include <functional>
#include <memory>
#include <string>

#include "application/graphics_settings.h"
#include "application/input.h"
#include "audio/audio_player.h"
#include "game/camera.h"
#include "physics/physics_world.h"
#include "physics/player_state.h"
#include "renderer/renderer.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"
#include "utilities/thread_pool.h"

/**
 * @brief High level coordinator between the renderer and game world. Owns all resources to load and play the game.
 * Recieves user input from application layer above.
 */
class Game
{
  public:
    Game(const GraphicsSettings &settings, int windowWidth, int windowHeight,
         const std::function<void(const std::string &)> &updateProgress);
    Game(const Game &) = delete;
    Game &operator=(const Game &) = delete;
    Game(Game &&other) = delete;
    Game &operator=(Game &&other) = delete;
    ~Game() = default;

    void resizeExternalResolution(int width, int height);
    void update(const UserInput &input, float time, float deltaTime);

  private:
    ThreadPool threadPool_;
    TextureManager textureManager_;
    Renderer renderer_;
    std::unique_ptr<Scene> scene_;
    PhysicsWorld physics_;
    AudioPlayer audio_;
    Camera playerCamera_;

    PlayerState updatePhysics(const UserInput &input, float deltaTime);
    void updateAudio(const PlayerState &state, float deltaTime);
};