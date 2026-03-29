#include <filesystem>

#include "graphics_settings.h"

namespace jsonLoader
{
GraphicsSettings loadSettingsFromJSON(const std::filesystem::path &path);
void saveSettingsToJSON(const GraphicsSettings &settings, const std::filesystem::path target);
} // namespace jsonLoader
