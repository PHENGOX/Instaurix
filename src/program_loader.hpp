#pragma once

#include <cstdint>
#include <cstddef>
#include <string>
#include <vector>

class Memory;

class ProgramLoader
{
public:
    enum class Format { Raw, Elf64, Nro };
    struct Region { uint64_t begin, end; uint8_t permission; };
    struct Image
    {
        Format format = Format::Raw;
        uint64_t entry = 0, stack = 0;
        std::vector<Region> regions;
        bool requiresNroRuntime = false;
        uint64_t nroModuleOffset = 0, nroFileSize = 0, nroBssBegin = 0, nroBssEnd = 0;
    };
    // Validates into a staging Memory; failure leaves the destination unchanged.
    static bool loadImage(Memory& memory, const char* filename, Image& image, std::string& error);
    static bool loadElf(Memory& memory, const std::vector<uint8_t>& bytes, Image& image, std::string& error);
    static bool loadNro(Memory& memory, const std::vector<uint8_t>& bytes, Image& image, std::string& error);
    struct RuntimeLayout { uint64_t relroBegin=0, relroEnd=0; };
    static bool validateNroRuntime(const Memory& memory, const Image& image, std::string& error, RuntimeLayout* layout = nullptr);
    static bool load(
        Memory& memory,
        const char* filename,
        uint64_t address
    );

    static uint64_t getLoadAddress();
    static size_t getProgramSize();
};
