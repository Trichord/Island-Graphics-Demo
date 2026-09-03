#include "game/game.h"

#include <glm/glm.hpp>
#include <iostream>
#include <memory>

#include "application/graphics_settings.h"
#include "application/input.h"
#include "physics/physics_world.h"
#include "physics/player_state.h"
#include "scene/island_scene/island_scene.h"
#include <algorithm>
#include <thread>

Game::Game(const GraphicsSettings &settings, int renderWidth, int renderHeight)
    : threadPool_{std::max(2u, std::thread::hardware_concurrency() / 2u)}, textureManager_{threadPool_, settings},
      renderer_{textureManager_, settings, renderWidth, renderHeight},
      scene_{std::make_unique<IslandScene>(textureManager_, "./res/scenes/island_scene.json")},
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

    PlayerState state{updatePhysics(input, deltaTime)};
    scene_->onUpdate(state, time, deltaTime);
    updateAudio(state, deltaTime);

    // Draw
    renderer_.render(*scene_, playerCamera_, time, deltaTime);

    // Clear
    scene_->clearUpdates();
}

PlayerState Game::updatePhysics(const Input &input, float deltaTime)
{
    physics_.update(input, playerCamera_.getFront(), deltaTime);
    physics_.synchronizeEntries(*scene_);

    // Synchronize camera
    const glm::vec3 pos{physics_.getPlayerPosition()};
    playerCamera_.setPosition(pos);

    return {
        .position = pos,
        .front = playerCamera_.getFront(),
        .velocity = physics_.getPlayerVelocity(),
        .supported = physics_.playerIsSupported(),
        .grounded = physics_.playerIsGrounded(),
        .jumpTriggered = physics_.jumpEvent(),
        .landingTriggered = physics_.landingEvent(),
        .footstepTriggered = physics_.footstepEvent(),
        .running = input.sprintToggle,
    };
}

void Game::updateAudio(const PlayerState &state, float deltaTime)
{
    audio_.updateTriggers(scene_->getUpdatedEntries());

    audio_.playCharacterEffects(state);
    audio_.playTriggers(state, deltaTime);
}