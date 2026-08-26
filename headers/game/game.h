#pragma once

#include <Jolt/Jolt.h>

#include <Jolt/Core/Core.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <memory>

#include "application/graphics_settings.h"
#include "application/input.h"
#include "audio/audio_player.h"
#include "physics/jolt_impl.h"
#include "physics/player_physics.h"
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
    static constexpr size_t joltStackSize_{10 * 1024 * 1024}; // 10 MB
    static constexpr JPH::uint maxPhysicsJobs_{2048u};

    // Critical resources
    ThreadPool threadPool_;
    TextureManager textureManager_;
    Renderer renderer_;

    // Jolt Physics
    BPLayerInterfaceImpl broadPhaseLayer_;
    ObjectLayerPairFilterImpl objectLayerPairFilter_;
    ObjectVsBroadPhaseLayerFilterImpl objectVsBroadPhaseLayerFilter_;
    JPH::TempAllocatorImpl tempAllocator_{joltStackSize_};
    JPH::JobSystemSingleThreaded jobSystem_{maxPhysicsJobs_};
    JPH::PhysicsSystem physicsSystem_;

    std::unique_ptr<Scene> scene_;
    PlayerPhysics physics_;
    AudioPlayer audio_;
    Camera playerCamera_;

    // Setup
    void registerJolt();

    PlayerState updatePhysics(const Input &input, PlayerPhysics &physics, float deltaTime);
    void updateAudio(const PlayerState &state, float deltaTime);
};