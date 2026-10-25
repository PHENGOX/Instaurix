#pragma once
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <vector>
#include <cstdint>

class ReadOnlyFiles {
public:
    using Bytes = std::vector<uint8_t>;
    static constexpr size_t MaxFiles = 32, MaxFileSize = 4*1024*1024, MaxTotalSize = 8*1024*1024;
    static bool validName(const std::string& name);
    static std::shared_ptr<const ReadOnlyFiles> load(const std::filesystem::path& root,std::string& error);
    std::shared_ptr<const Bytes> find(const std::string& name) const;
private:
    std::map<std::string,std::shared_ptr<const Bytes>> entries;
};
