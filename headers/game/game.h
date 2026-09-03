#pragma once

#include "Jolt/Jolt.h"

#include <Jolt/Physics/PhysicsSystem.h>
#include <memory>

#include "application/graphics_settings.h"
#include "application/input.h"
#include "audio/audio_player.h"
#include "physics/physics_world.h"
#include "physics/player_state.h"
#include "renderer/renderer.h"
#include "scene/camera.h"
#include "scene/scene.h"
#include "utilities/texture_manager.h"
#include "utilities/thread_pool.h"

class Game
{
  public:
    Game(const GraphicsSettings &settings, int renderWidth, int renderHeight);
    Game(const Game &) = delete;
    Game &operator=(const Game &) = delete;
    Game(Game &&other) = delete;
    Game &operator=(Game &&other) = delete;
    ~Game() = default;

    void resizeExternalResolution(int width, int height);
    void update(const Input &input, float time, float deltaTime);

  private:
    // Critical resources
    ThreadPool threadPool_;
    TextureManager textureManager_;
    Renderer renderer_;

    // Jolt Physics
    JPH::PhysicsSystem physicsSystem_;

    std::unique_ptr<Scene> scene_;
    PhysicsWorld physics_;
    AudioPlayer audio_;
    Camera playerCamera_;

    // Setup
    void registerJolt();

    PlayerState updatePhysics(const Input &input, float deltaTime);
    void updateAudio(const PlayerState &state, float deltaTime);
};