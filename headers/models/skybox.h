#pragma once

#include <filesystem>
#include <optional>
#include <span>
#include <vector>

#include "aabb.h"
#include "primitive.h"
#include "renderable.h"
#include "texture_manager.h"

class Skybox : public Renderable
{
  public:
    Skybox() = default;
    Skybox(TextureManager &textureManager, const std::filesystem::path &imagePath);
    Skybox(TextureManager &textureManager, const Handle borrowHandle);

    AABB &getAABB() override;
    Handle getHandle() const override;
    std::span<Primitive> getPrimitives() override;
    std::optional<Handle> getBorrowedHandle() const override;
    std::optional<std::vector<Primitive>> loadCollisionMesh() const override;

  protected:
    AABB aabb_;
    Handle handle_{0};
    std::optional<Handle> borrowedHandle_{};

    std::vector<Primitive> primitives_;
};