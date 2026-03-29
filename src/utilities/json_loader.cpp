#include "json_loader.h"

#include <filesystem>
#include <fstream>
#include <iostream>
#include <set>

#include "graphics_settings.h"

#include <cstdlib>
#include <iomanip>
#include <system_error>

#include "json/json.hpp"

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
        std::cerr << e.what() << std::endl;
        return {};
    }
}

void jsonLoader::saveConfigToJSON(const GraphicsSettings &settings, const std::filesystem::path target)
{
    json outputJSON = settings;
    std::ofstream out(target);

    if (!out.is_open())
    {
        std::cerr << "ERROR: Could not save settings to " << target.string() << std::endl
                  << std::generic_category().message(errno) << std::endl;
        return;
    }

    out << std::setw(4) << outputJSON << std::endl;

    if (!out)
    {
        std::cerr << "ERROR: Could not save settings to " << target.string() << std::endl
                  << std::generic_category().message(errno) << std::endl;
    }
}