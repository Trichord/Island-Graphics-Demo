#include "game/game.h"

#include <Jolt/Jolt.h>

#include <Jolt/Core/Core.h>
#include <Jolt/Core/JobSystemSingleThreaded.h>
#include <Jolt/Core/TempAllocator.h>
#include <Jolt/Physics/PhysicsSystem.h>
#include <glm/glm.hpp>
#include <iostream>
#include <memory>
#include <thread>

#include "application/graphics_settings.h"
#include "application/input.h"
#include "physics/jolt_impl.h"
#include "physics/player_physics.h"
#include "physics/player_state.h"
#include "scene/island_scene/island_scene.h"
#include <algorithm>

namespace
{

// Annoying workaround because of Jolt's PhysicsSystem.Init. All of Jolt must be setup before attempting to create
// Player Physics.
PlayerPhysics initPhysics(JPH::PhysicsSystem &physicsSystem, BPLayerInterfaceImpl &broadPhase,
                          ObjectVsBroadPhaseLayerFilterImpl &bfFilter, ObjectLayerPairFilterImpl &pairFilter,
                          JPH::TempAllocatorImpl &tempAllocator)
{
    // Physics engine constants
    constexpr JPH::uint maxBodies{1024};
    constexpr JPH::uint maxBodyPairs{1024};
    constexpr JPH::uint maxContactConstraints{1024};

    physicsSystem.Init(maxBodies, 0, maxBodyPairs, maxContactConstraints, broadPhase, bfFilter, pairFilter);

    return PlayerPhysics{physicsSystem, tempAllocator};
}
} // namespace

Game::Game(const GraphicsSettings &settings, int renderWidth, int renderHeight)
    : threadPool_{std::max(2u, std::thread::hardware_concurrency() / 2u)}, textureManager_{threadPool_, settings},
      renderer_{textureManager_, settings, renderWidth, renderHeight},
      scene_{std::make_unique<IslandScene>(textureManager_, "./res/scenes/island_scene.json")},
      physics_{initPhysics(physicsSystem_, broadPhaseLayer_, objectVsBroadPhaseLayerFilter_, objectLayerPairFilter_,
                           tempAllocator_)},
      audio_{*scene_, "./res/scenes/island_audio.json"}
{

    // Initialize scene dependent resources
    playerCamera_ = {scene_->getCameraDescription(), settings.renderWidth / static_cast<float>(settings.renderHeight)};
    renderer_.initializeResources(*scene_, playerCamera_);

    std::cout << "Creating Collision Mesh... ";
    physics_.initializeScene(*scene_);
    std::cout << "Done" << std::endl;

    threadPool_.stopAllThreads();
}

void Game::resizeExternalResolution(int width, int height)
{
    renderer_.updateExternalResolution(width, height);
}

void Game::update(const Input &input, float time, float deltaTime)
{
    playerCamera_.addPitch(input.mouseDeltaY);
    playerCamera_.addYaw(input.mouseDeltaX);

    PlayerState state{updatePhysics(input, physics_, deltaTime)};
    scene_->onUpdate(state, time, deltaTime);
    updateAudio(state, deltaTime);

    // Draw
    renderer_.render(*scene_, playerCamera_, time, deltaTime);

    // Clear
    scene_->clearUpdates();
}

PlayerState Game::updatePhysics(const Input &input, PlayerPhysics &physics, float deltaTime)
{
    physics.update(input, playerCamera_.getFront(), deltaTime);

    // Synchronize camera
    const glm::vec3 pos{physics.getInterpolatedPosition()};
    playerCamera_.setPosition(pos);

    return {
        .position = pos,
        .front = playerCamera_.getFront(),
        .velocity = physics.getVelocity(),
        .supported = physics.isSupported(),
        .grounded = physics.isGrounded(),
        .jumpTriggered = physics.jumpEvent(),
        .landingTriggered = physics.landingEvent(),
        .footstepTriggered = physics.footstepEvent(),
        .running = input.sprintToggle,
    };
}

void Game::updateAudio(const PlayerState &state, float deltaTime)
{
    audio_.updateTriggers(scene_->getUpdatedEntries());

    audio_.playCharacterEffects(state);
    audio_.playTriggers(state, deltaTime);
}