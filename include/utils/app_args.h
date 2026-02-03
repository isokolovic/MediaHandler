#pragma once

#include "utils/config.h"
#include <vector>
#include <string>
#include <memory>
#include <spdlog/logger.h>

namespace media_handler::utils {

    struct AppArgs {
        Config cfg;
        std::vector<std::string> inputs;
        bool retry_failed = false;
        bool organize_by_date = false;

        bool show_help = false;
    };

    AppArgs parse_command_line(int argc, char** argv, std::shared_ptr<spdlog::logger>& logger);

} // namespace media_handler::utils
