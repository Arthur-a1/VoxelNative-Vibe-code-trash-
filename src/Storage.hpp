#pragma once
#include "Core.hpp"
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <string>
namespace voxel {
class FileHandle {
    HANDLE handle_=INVALID_HANDLE_VALUE;
public:
    explicit FileHandle(HANDLE h):handle_(h) {}
    ~FileHandle() { if(handle_!=INVALID_HANDLE_VALUE) CloseHandle(handle_); }
    FileHandle(const FileHandle&)=delete;
    FileHandle& operator=(const FileHandle&)=delete;
    HANDLE get() const {return handle_;}
    bool valid() const {return handle_!=INVALID_HANDLE_VALUE;}
};
inline std::runtime_error ioError(const char* action) {return std::runtime_error(std::string(action)+" Win32="+std::to_string(GetLastError()));}
inline std::filesystem::path savePath() {
    PWSTR raw=nullptr;
    const HRESULT hr=SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&raw);
    if(FAILED(hr)) throw std::runtime_error("LocalAppData unavailable");
    // Copy after adopting ownership, so exceptions cannot leak the COM allocation.
    struct Owner { PWSTR p; ~Owner(){CoTaskMemFree(p);} } owner{raw};
    auto folder=std::filesystem::path(raw)/L"VoxelNative";
    std::filesystem::create_directories(folder);return folder/L"world.vxn";
}
inline bool load(World& world,const std::filesystem::path& path) {
    FileHandle file(CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,FILE_ATTRIBUTE_NORMAL,nullptr));
    if(!file.valid()) {if(GetLastError()==ERROR_FILE_NOT_FOUND) return false;throw ioError("Open save");}
    LARGE_INTEGER size{};if(!GetFileSizeEx(file.get(),&size)) throw ioError("Save size");
    if(size.QuadPart!=static_cast<LONGLONG>(World::volume+12)) throw std::runtime_error("Invalid save size; original preserved");
    std::vector<std::uint8_t> data(static_cast<std::size_t>(size.QuadPart));DWORD read=0;
    if(!ReadFile(file.get(),data.data(),static_cast<DWORD>(data.size()),&read,nullptr)||read!=data.size()) throw ioError("Read save");
    if(!decode(world,data)) throw std::runtime_error("Invalid save version, checksum or block; original preserved");
    return true;
}
inline void save(const World& world,const std::filesystem::path& path) {
    const auto data=encode(world);auto temp=path;temp+=L".tmp";
    {
        FileHandle file(CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(!file.valid()) throw ioError("Create temporary save");
        DWORD written=0;
        if(!WriteFile(file.get(),data.data(),static_cast<DWORD>(data.size()),&written,nullptr)||written!=data.size()) throw ioError("Write save");
        if(!FlushFileBuffers(file.get())) throw ioError("Flush save");
    }
    // Replace only after a complete flushed write in the same directory.
    if(!MoveFileExW(temp.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)) throw ioError("Commit save");
}
inline void logError(const char* message) noexcept {
    OutputDebugStringA(message);OutputDebugStringA("\n");
    try {
        const auto path=savePath().parent_path()/L"errors.log";
        FileHandle file(CreateFileW(path.c_str(),FILE_APPEND_DATA,FILE_SHARE_READ,nullptr,OPEN_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr));
        if(file.valid()) {
            const std::string line=std::string(message)+"\r\n";DWORD written=0;
            WriteFile(file.get(),line.data(),static_cast<DWORD>(line.size()),&written,nullptr);
        }
    } catch(...) { /* Reporting must not throw from an error handler. */ }
}
}
