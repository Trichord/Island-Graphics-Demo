#pragma once

#include <filesystem>
#include <glm/glm.hpp>
#include <memory>
#include <unordered_map>
#include <vector>

#include "audio/audio_engine.h"
#include "audio/sound_effect.h"
#include "audio_file.h"
#include "models/aabb.h"
#include "physics/player_state.h"
#include "scene/handle.h"
#include "scene/scene.h"

class AudioPlayer
{
  public:
    AudioPlayer(const Scene &scene, const std::filesystem::path &audioJson);

    void playCharacterEffects(const PlayerState &state);
    void updateTriggers(UpdatedEntriesView updatedEntries);
    void playTriggers(const PlayerState &state, float deltaTime);

  private:
    struct Trigger
    {
        AABB innerAABB;
        AABB outerAABB;
        std::size_t soundIndex{0};
        bool enabled{false};
        bool oneShot{false};
        bool loop{false};
        bool hasPlayed{false};

        float repeatDelay{0.0f};
        float timeAccumulator{0.0f};

        float fadeDistance{0.0f};

        float getFadeFactor(const glm::vec3 &position) const
        {
            // No fade distance defined
            if (fadeDistance <= 0.0f)
                return 1.0f;

            const glm::vec3 boxLocalPos{glm::abs(position - innerAABB.getCenter())};
            const glm::vec3 distFromAABB{boxLocalPos - innerAABB.getHalfExtent()};
            const glm::vec3 outside{glm::max(distFromAABB, glm::vec3{0.0f})};
            const float dist{glm::length(outside)};

            const float t{glm::clamp(dist / fadeDistance, 0.0f, 1.0f)};
            return 1.0f - glm::smoothstep(0.0f, 1.0f, t);
        }
    };

    struct SoundRange
    {
        std::size_t start{0};
        std::size_t count{0};
    };

    struct CharacterSounds
    {
        SoundRange footsteps;
        SoundRange landings;
        SoundRange jumps;
    };

    AudioEngine engine_;
    std::vector<Trigger> triggers_;
    CharacterSounds characterSounds_;
    std::vector<std::unique_ptr<SoundEffect>> soundEffects_;
    std::unordered_map<Handle, std::size_t> triggersByHandle_;

    SoundRange addSounds();
    std::vector<SoundEffectEntry> createLandings(const std::vector<SoundEffectEntry> &footsteps, float volumeMultiplier,
                                                 float pitchMultiplier);

    void playJumpEffect();
    void playLandingEffect();
    void playFootstepEffect();
};
