#include "utilities/json_loader.h"

#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <nlohmann/json.hpp>
#include <system_error>

#include "application/graphics_settings.h"

using json = nlohmann::json;

GraphicsSettings jsonLoader::loadSettingsFromJSON(const std::filesystem::path &path)
{
    if (!std::filesystem::exists(path))
        return {};

    try
    {
        std::ifstream in(path);
        json configJSON{json::parse(in)};
        return configJSON.get<GraphicsSettings>();
    }

    catch (const nlohmann::json::exception &e)
    {
        std::cerr << e.what() << "\n";
        return {};
    }
}

void jsonLoader::saveSettingsToJSON(const GraphicsSettings &settings, const std::filesystem::path target)
{
    json outputJSON = settings;
    std::ofstream out(target);

    if (!out.is_open())
    {
        std::cerr << "ERROR: Could not save settings to " << target.string() << "\n"
                  << std::generic_category().message(errno) << "\n";
        return;
    }

    out << std::setw(4) << outputJSON << "\n";

    if (!out)
    {
        std::cerr << "ERROR: Could not save settings to " << target.string() << "\n"
                  << std::generic_category().message(errno) << "\n";
    }
}