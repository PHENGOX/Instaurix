#include "program_loader.hpp"
#include "memory.hpp"

#include <fstream>

bool ProgramLoader::load(
    Memory& memory,
    const char* filename,
    uint64_t address
)
{
    return memory.loadFile(filename, address);
}

uint64_t ProgramLoader::getLoadAddress()
{
    return 0x0000;
}

size_t ProgramLoader::getProgramSize()
{
    std::ifstream file("program.bin", std::ios::binary);

    if (!file)
    {
        return 0;
    }

    file.seekg(0, std::ios::end);

    return static_cast<size_t>(file.tellg());
}