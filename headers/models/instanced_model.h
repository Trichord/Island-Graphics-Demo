#pragma once

#include <filesystem>
#include <vector>

#include "glad/glad.h"
#include "instance_data.h"
#include "model.h"
#include "renderable.h"
#include "texture_manager.h"

class InstancedModel : public Model {
 public:
  InstancedModel(TextureManager& textureManager,
                 const std::filesystem::path& modelPath,
                 const std::filesystem::path& positionsPath,
                 float minSize = 1.0f, float maxSize = 1.0f,
                 unsigned int numPositions = 0);
  InstancedModel(TextureManager& textureManager,
                 const InstancedModel& borrowTarget,
                 const std::filesystem::path& modelPath,
                 const std::filesystem::path& positionsPath,
                 float minSize = 1.0f, float maxSize = 1.0f,
                 unsigned int numPositions = 0);
  InstancedModel(const InstancedModel&) = delete;
  InstancedModel& operator=(const InstancedModel&) = delete;
  InstancedModel(InstancedModel&& other) noexcept;
  InstancedModel& operator=(InstancedModel&& other) = delete;
  ~InstancedModel();

  void sortInstances() override;
  void resetVisibility() override;
  std::size_t getInstanceCount() const override;
  std::optional<std::span<InstanceData>> getInstanceData() override;

 private:
  std::vector<InstanceData> instances;
  std::size_t visibleInstances{0};

  GLuint instancesVBO{0};

  void loadPositions(const std::filesystem::path& positionsPath,
                     unsigned int numPositions);
  void setupInstancedBuffers();
  void generateInstanceMatrices(float minSize, float maxSize);
  void generateWorldAABBs();
  void deleteBuffers();
};
