#pragma once
#include "utils/config.h"
#include "utils/utils.h"
#include <string>
#include <vector>

namespace media_handler::utils {

    struct AppArgs {
        std::vector<std::string> inputs;
        std::string output_dir = "output";
        bool retry_failed = false;
        bool organize_by_date = false;
        bool show_help = false;

        Config cfg;
    };

    AppArgs parse_command_line(int argc, char** argv, std::shared_ptr<spdlog::logger>& logger);

} // namespace media_handler::utils