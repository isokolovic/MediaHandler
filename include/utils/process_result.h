#pragma once
#include <string>

namespace media_handler::utils {
    struct ProcessResult {
        bool success;
        std::string message;

        static ProcessResult OK() { return { true,  "" }; }
        static ProcessResult Error(std::string msg) { return { false, std::move(msg) }; }
    };
}