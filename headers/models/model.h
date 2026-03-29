#pragma once

#include <filesystem>
#include <span>
#include <vector>

#include "aabb.h"
#include "primitive.h"
#include "renderable.h"
#include "texture_manager.h"

class Model : public IRenderable
{
  public:
    Model(TextureManager &textureManager, const std::filesystem::path &path);
    Model(TextureManager &textureManager, const ModelHandle borrowHandle, const std::filesystem::path &path);
    Model(const Model &) = delete;
    Model &operator=(const Model &) = delete;
    Model(Model &&other) noexcept;
    Model &operator=(Model &&other) = delete;
    ~Model();

    AABB &getAABB() override;
    void setMeshVisibility(bool visible) override;
    const ModelHandle getHandle() const override;
    std::span<Primitive::Mesh> getMeshes() override;
    std::optional<std::vector<Primitive::Mesh>> loadCollisionMesh() override;
    std::optional<std::vector<Instance>> generateInstancedData() override;

  protected:
    ModelHandle handle_{0};
    TextureManager &textureManager_;
    std::filesystem::path filePath_;
    std::vector<Primitive::Mesh> meshes_;
    AABB aabb_;
    bool hasMaterials_{false};

    void loadModel(const std::filesystem::path &path, bool borrowed);
    void calculateAABB();
    void assignMaterialIndex(std::size_t newIndex);
    bool hasMaterials() const;
};
