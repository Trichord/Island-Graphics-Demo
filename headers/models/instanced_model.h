#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include "glad/glad.h"
#include "instance_data.h"
#include "model.h"
#include "texture_manager.h"

class InstancedModel : public Model
{
  public:
    InstancedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                   const std::filesystem::path &positionsPath, float minSize = 1.0f, float maxSize = 1.0f,
                   unsigned int numPositions = 0);
    InstancedModel(TextureManager &textureManager, const ModelHandle borrowHandle,
                   const std::filesystem::path &modelPath, const std::filesystem::path &positionsPath,
                   float minSize = 1.0f, float maxSize = 1.0f, unsigned int numPositions = 0);
    InstancedModel(const InstancedModel &) = delete;
    InstancedModel &operator=(const InstancedModel &) = delete;
    InstancedModel(InstancedModel &&other) noexcept;
    InstancedModel &operator=(InstancedModel &&other) = delete;
    ~InstancedModel();

    void resetVisibility() override;
    std::span<InstancedData> getInstanceData() override;

  private:
    std::vector<InstancedData> instances;

    GLuint instancesVBO{0};

    void loadPositions(const std::filesystem::path &positionsPath, unsigned int numPositions);
    void generateInstanceMatrices(float minSize, float maxSize);
    void generateWorldAABBs();
    void deleteBuffers();
};
