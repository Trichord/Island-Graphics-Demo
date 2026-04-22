#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "aabb.h"
#include "model.h"
#include "primitive.h"
#include "texture_manager.h"

class BaseModel : public Model
{
  public:
    BaseModel(TextureManager &textureManager, const std::filesystem::path &path);
    BaseModel(TextureManager &textureManager, const ModelHandle borrowHandle, const std::filesystem::path &path);

    AABB &getAABB() override;
    ModelHandle getHandle() const override;
    std::span<Primitive> getPrimitives() override;
    std::optional<std::vector<Primitive>> loadCollisionMesh() const override;

  protected:
    ModelHandle handle_{0};
    TextureManager &textureManager_;
    std::filesystem::path filePath_;
    std::vector<Primitive> primitives_;
    AABB aabb_;

    void loadModel(const std::filesystem::path &path, bool borrowed);
    void calculateAABB();
};
