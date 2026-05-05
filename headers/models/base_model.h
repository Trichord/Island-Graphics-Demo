#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "aabb.h"
#include "model.h"
#include "primitive.h"
#include "texture_manager.h"

class Model : public Renderable
{
  public:
    Model(TextureManager &textureManager, const std::filesystem::path &path);
    Model(TextureManager &textureManager, const Handle borrowHandle, const std::filesystem::path &path);

    AABB &getAABB() override;
    Handle getHandle() const override;
    std::span<Primitive> getPrimitives() override;
    std::optional<Handle> getBorrowedHandle() const override;
    std::optional<std::vector<Primitive>> loadCollisionMesh() const override;

  protected:
    AABB aabb_;
    Handle handle_{0};
    std::optional<Handle> borrowedHandle_{};

    TextureManager &textureManager_;
    std::filesystem::path filePath_;
    std::vector<Primitive> primitives_;
    
    void loadModel(const std::filesystem::path &path, bool borrowed);
    void calculateAABB();
};
