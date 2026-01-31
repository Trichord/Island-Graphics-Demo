#pragma once

#include <filesystem>
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
    InstancedModel(TextureManager &textureManager, const InstancedModel &borrowTarget,
                   const std::filesystem::path &modelPath, const std::filesystem::path &positionsPath,
                   float minSize = 1.0f, float maxSize = 1.0f, unsigned int numPositions = 0);
    InstancedModel(const InstancedModel &) = delete;
    InstancedModel &operator=(const InstancedModel &) = delete;
    InstancedModel(InstancedModel &&other) noexcept;
    InstancedModel &operator=(InstancedModel &&other) = delete;
    ~InstancedModel();

    void resetVisibility() override;
    std::size_t getInstanceCount() const override;
    std::span<InstanceData> getInstanceData() override;
    void setVisibleInstanceCount(std::size_t newSize) override;

  private:
    std::vector<InstanceData> instances;
    std::size_t visibleInstances{0};

    GLuint instancesVBO{0};

    void loadPositions(const std::filesystem::path &positionsPath, unsigned int numPositions);
    void generateInstanceMatrices(float minSize, float maxSize);
    void generateWorldAABBs();
    void deleteBuffers();
};
