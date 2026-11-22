#pragma once
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <cstring>
#include "capture_defaults.hpp"

namespace ps {
enum class CaptureSettingState { On,Off,Unsupported,Unavailable };
struct CaptureSettingResult {
    CaptureSettingState state=CaptureSettingState::Unavailable;
    LSTATUS error=ERROR_SUCCESS;
    bool fromWindowsDefault=false;
};

inline std::optional<bool> queryWindowsCaptureDefault() {
    const HMODULE ntdll=GetModuleHandleW(L"ntdll.dll");
    if(!ntdll) return std::nullopt;
    const FARPROC proc=GetProcAddress(ntdll,"RtlGetVersion");
    if(!proc) return std::nullopt;
    using VersionFunction=LONG (WINAPI*)(OSVERSIONINFOW*);
    VersionFunction versionFunction=nullptr;
    static_assert(sizeof(versionFunction)==sizeof(proc),"Unexpected function-pointer representation");
    std::memcpy(&versionFunction,&proc,sizeof(versionFunction));
    OSVERSIONINFOEXW version{};version.dwOSVersionInfoSize=sizeof(version);
    if(versionFunction(reinterpret_cast<OSVERSIONINFOW*>(&version))<0) return std::nullopt;
    std::optional<unsigned> revision;
    if(version.dwBuildNumber==22621) {
        HKEY key=nullptr;
        if(RegOpenKeyExW(HKEY_LOCAL_MACHINE,L"SOFTWARE\\Microsoft\\Windows NT\\CurrentVersion",0,
            KEY_QUERY_VALUE|KEY_WOW64_64KEY,&key)==ERROR_SUCCESS) {
            DWORD value=0,type=0,size=sizeof(value);
            const auto code=RegQueryValueExW(key,L"UBR",nullptr,&type,reinterpret_cast<BYTE*>(&value),&size);
            RegCloseKey(key);
            if(code==ERROR_SUCCESS && type==REG_DWORD && size==sizeof(value)) revision=value;
        }
    }
    return windowsCaptureDefault(version.dwMajorVersion,version.dwMinorVersion,version.dwBuildNumber,
        revision,version.wProductType==VER_NT_WORKSTATION);
}
using CaptureDefaultResolver=std::optional<bool>(*)();

// No constructor/read side effects. Only an explicit set() creates or changes a value.
// An injectable root/path allows Windows CI to test an isolated temporary HKCU key.
class WindowsCaptureSetting {
    HKEY root_;
    const wchar_t* path_;
    CaptureDefaultResolver defaultResolver_;
    CaptureSettingResult resolveDefault() const {
        const auto enabled=defaultResolver_ ? defaultResolver_() : std::nullopt;
        if(!enabled) return {CaptureSettingState::Unavailable,ERROR_NOT_SUPPORTED};
        return {*enabled ? CaptureSettingState::On : CaptureSettingState::Off,ERROR_SUCCESS,true};
    }
public:
    static constexpr const wchar_t* ValueName=L"PrintScreenKeyForSnippingEnabled";
    explicit WindowsCaptureSetting(HKEY root=HKEY_CURRENT_USER,const wchar_t* path=L"Control Panel\\Keyboard",
        CaptureDefaultResolver defaultResolver=queryWindowsCaptureDefault)
        :root_(root),path_(path),defaultResolver_(defaultResolver) {}

    CaptureSettingResult read() const {
        HKEY key=nullptr;
        LSTATUS code=RegOpenKeyExW(root_,path_,0,KEY_QUERY_VALUE,&key);
        if(code==ERROR_FILE_NOT_FOUND || code==ERROR_PATH_NOT_FOUND) return resolveDefault();
        if(code!=ERROR_SUCCESS) return {CaptureSettingState::Unavailable,code};
        DWORD type=0,value=0,size=sizeof(value);
        code=RegQueryValueExW(key,ValueName,nullptr,&type,reinterpret_cast<BYTE*>(&value),&size);
        RegCloseKey(key);
        if(code==ERROR_FILE_NOT_FOUND) return resolveDefault();
        if(code==ERROR_MORE_DATA) return {CaptureSettingState::Unsupported,ERROR_INVALID_DATA};
        if(code!=ERROR_SUCCESS) return {CaptureSettingState::Unavailable,code};
        if(type!=REG_DWORD || size!=sizeof(DWORD) || value>1) return {CaptureSettingState::Unsupported,ERROR_INVALID_DATA};
        return {value ? CaptureSettingState::On : CaptureSettingState::Off,ERROR_SUCCESS};
    }

    LSTATUS set(bool enabled) const {
        HKEY key=nullptr;
        LSTATUS code=RegCreateKeyExW(root_,path_,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr);
        if(code!=ERROR_SUCCESS) return code;
        const DWORD value=enabled ? 1u : 0u;
        code=RegSetValueExW(key,ValueName,0,REG_DWORD,reinterpret_cast<const BYTE*>(&value),sizeof(value));
        RegCloseKey(key);
        return code;
    }
};
}
