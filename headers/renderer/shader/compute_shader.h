#pragma once

#include "shader_base.h"

#include <cstdint>
#include <filesystem>

class ComputeShader : public Shader
{
  public:
    ComputeShader() = default;
    ComputeShader(const std::filesystem::path &path);
    ComputeShader(ComputeShader &&other) = default;
    ComputeShader &operator=(ComputeShader &&other) = default;

    void dispatch(int x, int y, int z);
};