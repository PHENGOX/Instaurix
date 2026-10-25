#pragma once
#include <cstdint>
#include <vector>
#include <filesystem>
#include <string>

// Development RGB framebuffer, unrelated to native console GPU command streams.
struct Frame {
    uint32_t width = 0, height = 0;
    std::vector<uint8_t> rgb;
    bool savePng(const std::filesystem::path& path,std::string& error) const;
};
