#include "read_only_files.hpp"
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#include <windows.h>
#else
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#endif

bool ReadOnlyFiles::validName(const std::string& name)
{
    if (name.empty() || name.size()>64 || name=="." || name=="..") return false;
    for (unsigned char c : name)
        if (!((c>='a' && c<='z') || (c>='A' && c<='Z') || (c>='0' && c<='9') || c=='.' || c=='_' || c=='-')) return false;
    return true;
}

std::shared_ptr<const ReadOnlyFiles::Bytes> ReadOnlyFiles::find(const std::string& name) const
{
    const auto entry=entries.find(name);
    return entry==entries.end() ? nullptr : entry->second;
}

std::shared_ptr<const ReadOnlyFiles> ReadOnlyFiles::load(const std::filesystem::path& directory,std::string& error)
{
    error.clear();
    try {
        const auto root=std::filesystem::canonical(directory);
        if (!std::filesystem::is_directory(root)) throw std::runtime_error("Directory required");
        auto snapshot=std::make_shared<ReadOnlyFiles>();
        size_t total=0;
        for (const auto& entry : std::filesystem::directory_iterator(root))
        {
            const auto name=entry.path().filename().string();
            if (!validName(name) || snapshot->entries.size()>=MaxFiles) throw std::runtime_error("Unsupported file name or file count");
            auto data=std::make_shared<Bytes>();
#ifdef _WIN32
            struct File { HANDLE handle; ~File() { if(handle!=INVALID_HANDLE_VALUE) CloseHandle(handle); } };
            File file{CreateFileW(entry.path().c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,
                FILE_FLAG_OPEN_REPARSE_POINT|FILE_FLAG_BACKUP_SEMANTICS,nullptr)};
            if (file.handle==INVALID_HANDLE_VALUE) throw std::runtime_error("Failed to open file");
            BY_HANDLE_FILE_INFORMATION info{};
            if (!GetFileInformationByHandle(file.handle,&info) || GetFileType(file.handle)!=FILE_TYPE_DISK ||
                (info.dwFileAttributes & (FILE_ATTRIBUTE_DIRECTORY|FILE_ATTRIBUTE_REPARSE_POINT)))
                throw std::runtime_error("Regular file required; links and directories rejected");
            std::wstring finalName(32768,L'\0');
            const auto length=GetFinalPathNameByHandleW(file.handle,finalName.data(),DWORD(finalName.size()),FILE_NAME_NORMALIZED);
            if (!length || length>=finalName.size()) throw std::runtime_error("Failed to resolve file");
            finalName.resize(length);
            if (finalName.rfind(L"\\\\?\\UNC\\",0)==0) finalName=L"\\\\"+finalName.substr(8);
            else if (finalName.rfind(L"\\\\?\\",0)==0) finalName=finalName.substr(4);
            const auto parent=std::filesystem::path(finalName).parent_path().native();
            if (CompareStringOrdinal(parent.c_str(),-1,root.c_str(),-1,TRUE)!=CSTR_EQUAL)
                throw std::runtime_error("File is outside the selected directory");
            const uint64_t size=(uint64_t(info.nFileSizeHigh)<<32)|info.nFileSizeLow;
            if (size>MaxFileSize || size>MaxTotalSize-total) throw std::runtime_error("Size limit exceeded");
            data->resize(size);
            DWORD read=0;
            if (size && (!ReadFile(file.handle,data->data(),DWORD(size),&read,nullptr) || read!=size))
                throw std::runtime_error("Incomplete read");
#else
            struct File { int fd; ~File() { if(fd>=0) ::close(fd); } };
            File folder{::open(root.c_str(),O_RDONLY|O_DIRECTORY|O_NOFOLLOW)};
            if(folder.fd<0) throw std::runtime_error("Failed to open directory");
            File file{::openat(folder.fd,name.c_str(),O_RDONLY|O_NOFOLLOW|O_NONBLOCK)};
            struct stat info{};
            if(file.fd<0 || ::fstat(file.fd,&info) || !S_ISREG(info.st_mode) || info.st_size<0)
                throw std::runtime_error("Regular file required; links rejected");
            const auto size=uint64_t(info.st_size);
            if(size>MaxFileSize || size>MaxTotalSize-total) throw std::runtime_error("Size limit exceeded");
            data->resize(size);
            size_t count=0;
            while(count<data->size()) {
                const auto read=::read(file.fd,data->data()+count,data->size()-count);
                if(read<=0) throw std::runtime_error("Incomplete read");
                count+=size_t(read);
            }
#endif
            total+=data->size();
            snapshot->entries.emplace(name,std::move(data));
        }
        return snapshot;
    }
    catch(const std::exception& exception) { error=std::string("Read-only mount: ")+exception.what(); return nullptr; }
}
