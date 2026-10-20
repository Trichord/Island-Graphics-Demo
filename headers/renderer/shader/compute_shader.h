#pragma once

#include "renderer/shader/shader_base.h"

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

class ComputeShader : public Shader
{
  public:
    ComputeShader() = default;
    ComputeShader(const std::filesystem::path &path, const std::vector<std::string> &defines = {});
    ComputeShader(const ComputeShader &) = delete;
    ComputeShader &operator=(const ComputeShader &) = delete;
    ComputeShader(ComputeShader &&other) = default;
    ComputeShader &operator=(ComputeShader &&other) = default;

    void dispatch(std::uint32_t x, std::uint32_t y, std::uint32_t z);
};