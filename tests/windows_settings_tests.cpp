#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#include "../src/windows_capture_setting.hpp"
#include <cassert>
#include <iostream>
#include <string>

namespace {
unsigned defaultReads=0;
std::optional<bool> defaultOn() { ++defaultReads;return true; }
std::optional<bool> defaultOff() { return false; }
std::optional<bool> unresolvedDefault() { return std::nullopt; }
}

class TemporaryRegistryRoot {
    std::wstring path_;
public:
    HKEY key=nullptr;
    TemporaryRegistryRoot() {
        path_=L"Software\\PrintShift\\Tests\\capture-"+std::to_wstring(GetCurrentProcessId())+L"-"+std::to_wstring(GetTickCount64());
        LSTATUS code=RegCreateKeyExW(HKEY_CURRENT_USER,path_.c_str(),0,nullptr,0,KEY_ALL_ACCESS,nullptr,&key,nullptr);
        assert(code==ERROR_SUCCESS);
    }
    ~TemporaryRegistryRoot() {
        if(key) RegCloseKey(key);
        RegDeleteTreeW(HKEY_CURRENT_USER,path_.c_str());
    }
};

int main() {
    TemporaryRegistryRoot root;
    const ps::WindowsCaptureSetting setting(root.key,L"Keyboard",defaultOn);
    const auto initial=setting.read();
    assert(initial.state==ps::CaptureSettingState::On && initial.fromWindowsDefault);
    const ps::WindowsCaptureSetting offSetting(root.key,L"MissingOff",defaultOff);
    const auto offInitial=offSetting.read();
    assert(offInitial.state==ps::CaptureSettingState::Off && offInitial.fromWindowsDefault);
    const ps::WindowsCaptureSetting unknownSetting(root.key,L"MissingUnknown",unresolvedDefault);
    assert(unknownSetting.read().state==ps::CaptureSettingState::Unavailable);
    HKEY child=nullptr;
    // Resolve the initial On/Off state without creating a key or writing a preference.
    assert(RegOpenKeyExW(root.key,L"Keyboard",0,KEY_READ,&child)==ERROR_FILE_NOT_FOUND);
    assert(setting.set(initial.state!=ps::CaptureSettingState::On)==ERROR_SUCCESS);
    const auto firstClick=setting.read();
    assert(firstClick.state==ps::CaptureSettingState::Off && !firstClick.fromWindowsDefault);
    assert(offSetting.set(offInitial.state!=ps::CaptureSettingState::On)==ERROR_SUCCESS);
    const auto offFirstClick=offSetting.read();
    assert(offFirstClick.state==ps::CaptureSettingState::On && !offFirstClick.fromWindowsDefault);
    assert(setting.set(true)==ERROR_SUCCESS);
    const unsigned readsBeforeExplicit=defaultReads;
    const auto explicitOn=setting.read();
    assert(explicitOn.state==ps::CaptureSettingState::On && !explicitOn.fromWindowsDefault);
    assert(defaultReads==readsBeforeExplicit); // An explicit Windows preference wins.
    assert(RegOpenKeyExW(root.key,L"Keyboard",0,KEY_ALL_ACCESS,&child)==ERROR_SUCCESS);
    DWORD type=0,value=0,size=sizeof(value);
    assert(RegQueryValueExW(child,ps::WindowsCaptureSetting::ValueName,nullptr,&type,reinterpret_cast<BYTE*>(&value),&size)==ERROR_SUCCESS);
    assert(type==REG_DWORD && size==sizeof(value) && value==1);
    const DWORD unrelated=42;
    assert(RegSetValueExW(child,L"Unrelated",0,REG_DWORD,reinterpret_cast<const BYTE*>(&unrelated),sizeof(unrelated))==ERROR_SUCCESS);
    assert(setting.set(false)==ERROR_SUCCESS);
    assert(setting.read().state==ps::CaptureSettingState::Off);
    size=sizeof(value);
    assert(RegQueryValueExW(child,L"Unrelated",nullptr,&type,reinterpret_cast<BYTE*>(&value),&size)==ERROR_SUCCESS && value==42);
    assert(RegDeleteValueW(child,ps::WindowsCaptureSetting::ValueName)==ERROR_SUCCESS);
    const auto reset=setting.read();
    assert(reset.state==ps::CaptureSettingState::On && reset.fromWindowsDefault);
    const DWORD unsupported=2;
    assert(RegSetValueExW(child,ps::WindowsCaptureSetting::ValueName,0,REG_DWORD,reinterpret_cast<const BYTE*>(&unsupported),sizeof(unsupported))==ERROR_SUCCESS);
    assert(setting.read().state==ps::CaptureSettingState::Unsupported);
    const wchar_t invalid[]=L"not a DWORD";
    assert(RegSetValueExW(child,ps::WindowsCaptureSetting::ValueName,0,REG_SZ,reinterpret_cast<const BYTE*>(invalid),sizeof(invalid))==ERROR_SUCCESS);
    assert(setting.read().state==ps::CaptureSettingState::Unsupported);
    const wchar_t shortInvalid[]=L"x";
    assert(RegSetValueExW(child,ps::WindowsCaptureSetting::ValueName,0,REG_SZ,reinterpret_cast<const BYTE*>(shortInvalid),sizeof(shortInvalid))==ERROR_SUCCESS);
    assert(setting.read().state==ps::CaptureSettingState::Unsupported);
    const BYTE tiny=1;
    assert(RegSetValueExW(child,ps::WindowsCaptureSetting::ValueName,0,REG_BINARY,&tiny,sizeof(tiny))==ERROR_SUCCESS);
    assert(setting.read().state==ps::CaptureSettingState::Unsupported);
    assert(setting.set(true)==ERROR_SUCCESS);
    assert(setting.read().state==ps::CaptureSettingState::On);
    RegCloseKey(child);
    std::cout << "Windows setting tests passed using an isolated temporary registry root.\n";
}
