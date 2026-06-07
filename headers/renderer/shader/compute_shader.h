#pragma once

#include "renderer/shader/shader_base.h"

#include <filesystem>
#include <string>
#include <vector>

class ComputeShader : public Shader
{
  public:
    ComputeShader() = default;
    ComputeShader(const std::filesystem::path &path, const std::vector<std::string> &defines = {});
    ComputeShader(ComputeShader &&other) = default;
    ComputeShader &operator=(ComputeShader &&other) = default;

    void dispatch(int x, int y, int z);
};