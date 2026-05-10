#include "cube_mapped_model.h"

#include <filesystem>
#include <vector>

#include "model.h"
#include "texture_manager.h"

CubeMappedModel::CubeMappedModel(TextureManager &textureManager, const std::filesystem::path &modelPath,
                                 const std::filesystem::path &imagePath, std::optional<Handle> borrowHandle)
    : Model{textureManager, modelPath, borrowHandle}
{
    std::vector<int> materialIndices;
    materialIndices.reserve(primitives_.size());

    for (Primitive &primitive : primitives_)
        materialIndices.push_back(primitive.materialIndex);

    textureManager.loadCubemapTextures(handle_, imagePath, materialIndices);
}