#ifndef UNICODE
#define UNICODE
#endif
#ifndef _UNICODE
#define _UNICODE
#endif
#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>
#include <windowsx.h>
#include <commctrl.h>
#include <shellapi.h>
#include <shlobj.h>
#include <uxtheme.h>
#include <dwmapi.h>
#include <wtsapi32.h>
#include <array>
#include <map>
#include <string>
#include <vector>
#include "engine.hpp"
#include "version.hpp"
#include "windows_capture_setting.hpp"
#include "outputs.hpp"

namespace {
constexpr wchar_t ClassName[]=L"PrintShift.Main.1";
constexpr wchar_t EditorClass[]=L"PrintShift.Editor.1";
constexpr wchar_t RunKey[]=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
constexpr UINT TrayMessage=WM_APP+1, RecordMessage=WM_APP+2, ErrorMessage=WM_APP+3;
constexpr ULONG_PTR InjectionTag=0x50534846;
enum Id { List=100,Add,Edit,Remove,GlobalEnable,Startup,StartHidden,Timeout,Hide,WindowsCapture,WindowsCaptureInfo,WindowsKeyboardSettings,
    Shortcut=200,Output,RuleEnable,Record,Save,Cancel,EditorHelp,ExactModifiers,PhysicalPositions,KeyInfo,
    TrayEnable=300,TraySettings,TrayExit };
HINSTANCE instance;
HWND mainWindow,editorWindow,listWindow,statusWindow;
HHOOK hook;
HANDLE singleton;
UINT taskbarCreated,showMessage,dpi=96;
HFONT bodyFont,titleFont,smallFont,editorFont;
UINT editorDpi=96;
HBRUSH background=CreateSolidBrush(RGB(245,247,251));
HICON activeIcon,inactiveIcon;
bool trayReady=false,startHidden=false,refreshing=false,recording=false,recordAwaitRelease=false;
bool exactModifiers=true;
bool physicalPositions=false;
unsigned inspectedKey=0,inspectedVirtual=0;
int editing=-1;
ps::Engine engine;
std::wstring configPath,lastStatus=L"Ready. Shortcuts run when you release their keys.";
std::vector<INPUT> buffered;
HWND bufferedForeground;
ps::Sequence recorded;
ps::Recorder recorder;
ps::Keys snapshotKeys;
ps::KeyTracker keyTracker;
bool haveWindowsCaptureState=false;

int D(int n) { return MulDiv(n,static_cast<int>(dpi),96); }
std::wstring wide(const std::string& s) { return std::wstring(s.begin(),s.end()); }
std::string ascii(const std::wstring& s) {
    std::string r;
    for(auto c:s) { if(c>127) throw std::runtime_error("Use English key names, for example Ctrl+Shift+P.");r+=static_cast<char>(c); }
    return r;
}
std::wstring text(HWND w) {
    int n=GetWindowTextLengthW(w);std::wstring s(n+1,L'\0');
    GetWindowTextW(w,s.data(),n+1);s.resize(n);return s;
}
void error(const std::wstring& s,HWND owner=nullptr) {
    MessageBoxW(owner ? owner : mainWindow,s.c_str(),L"PrintShift",MB_OK|MB_ICONERROR);
}
void status(const std::wstring& s) { lastStatus=s;if(statusWindow) SetWindowTextW(statusWindow,s.c_str()); }
void refreshWindowsCaptureSetting() {
    if(!mainWindow || !GetDlgItem(mainWindow,WindowsCapture)) return;
    const auto result=ps::WindowsCaptureSetting{}.read();
    bool available=true;
    std::wstring label;
    switch(result.state) {
    case ps::CaptureSettingState::On:
        CheckDlgButton(mainWindow,WindowsCapture,BST_CHECKED);haveWindowsCaptureState=true;
        label=result.fromWindowsDefault ? L"Windows preference: On (Windows default). Click to turn it Off." :
            L"Windows preference: On. Sign out if Windows has not applied the change.";break;
    case ps::CaptureSettingState::Off:
        CheckDlgButton(mainWindow,WindowsCapture,BST_UNCHECKED);haveWindowsCaptureState=true;
        label=result.fromWindowsDefault ? L"Windows preference: Off (Windows default). Click to turn it On." :
            L"Windows preference: Off. Sign out if Windows has not applied the change.";break;
    case ps::CaptureSettingState::Unsupported:
        available=false;label=L"Windows uses an unsupported value. Review this preference in Windows settings.";break;
    case ps::CaptureSettingState::Unavailable:
        available=false;label=L"Cannot read the Windows preference (error "+std::to_wstring(result.error)+L"). Open Windows settings.";break;
    }
    // Never display a guessed initial state or introduce a third checkbox position.
    ShowWindow(GetDlgItem(mainWindow,WindowsCapture),haveWindowsCaptureState ? SW_SHOWNA : SW_HIDE);
    EnableWindow(GetDlgItem(mainWindow,WindowsCapture),available);
    SetDlgItemTextW(mainWindow,WindowsCaptureInfo,label.c_str());
}
DWORD WINAPI broadcastCaptureSettingChange(void*) {
    DWORD_PTR ignored=0;
    // Send the documented notification from a worker; never block the keyboard-hook/UI thread.
    SendMessageTimeoutW(HWND_BROADCAST,WM_SETTINGCHANGE,0,reinterpret_cast<LPARAM>(L"Keyboard"),
        SMTO_ABORTIFHUNG,200,&ignored);
    return 0;
}
void toggleWindowsCaptureSetting() {
    const ps::WindowsCaptureSetting setting;
    const auto before=setting.read();
    if(before.state==ps::CaptureSettingState::Unavailable || before.state==ps::CaptureSettingState::Unsupported) {
        refreshWindowsCaptureSetting();
        error(L"This Windows preference cannot be changed here. Open Windows keyboard settings to review it.");return;
    }
    const bool enabled=before.state!=ps::CaptureSettingState::On;
    const LSTATUS code=setting.set(enabled);
    refreshWindowsCaptureSetting();
    if(code!=ERROR_SUCCESS) {
        error(L"Windows did not allow this preference to be changed (error "+std::to_wstring(code)+L").");return;
    }
    const auto verified=setting.read();
    const auto expected=enabled ? ps::CaptureSettingState::On : ps::CaptureSettingState::Off;
    if(verified.state!=expected) {
        status(L"The Windows preference could not be verified. Open Windows keyboard settings.");return;
    }
    QueueUserWorkItem(broadcastCaptureSettingChange,nullptr,WT_EXECUTEDEFAULT);
    status(L"Windows preference saved for this user. Sign out and back in if it has not taken effect.");
}
bool startupEnabled() {
    HKEY key;bool result=false;
    if(RegOpenKeyExW(HKEY_CURRENT_USER,RunKey,0,KEY_QUERY_VALUE,&key)==ERROR_SUCCESS) {
        result=RegQueryValueExW(key,L"PrintShift",nullptr,nullptr,nullptr,nullptr)==ERROR_SUCCESS;RegCloseKey(key);
    }
    return result;
}
bool setStartup(bool on) {
    HKEY key;
    if(RegCreateKeyExW(HKEY_CURRENT_USER,RunKey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS) return false;
    LSTATUS result;
    if(on) {
        std::vector<wchar_t> path(32768);
        DWORD n=GetModuleFileNameW(nullptr,path.data(),static_cast<DWORD>(path.size()));
        std::wstring value=L"\""+std::wstring(path.data(),n)+L"\" --tray";
        if(n==0 || n>=path.size() || value.size()>259) { RegCloseKey(key);return false; }
        result=RegSetValueExW(key,L"PrintShift",0,REG_SZ,reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t)));
    } else {
        result=RegDeleteValueW(key,L"PrintShift");if(result==ERROR_FILE_NOT_FOUND) result=ERROR_SUCCESS;
    }
    RegCloseKey(key);return result==ERROR_SUCCESS;
}
bool saveConfig() {
    if(configPath.empty()) { status(L"Settings could not be saved: no writable settings folder.");return false; }
    std::wstring content=L"\ufeff[General]\r\nVersion=3\r\nEnabled="+std::to_wstring(engine.enabled)+
        L"\r\nStartHidden="+std::to_wstring(startHidden)+L"\r\nTimeout="+std::to_wstring(engine.timeout)+
        L"\r\nExactModifiers="+std::to_wstring(exactModifiers)+
        L"\r\nPhysicalPositions="+std::to_wstring(physicalPositions)+
        L"\r\nCount="+std::to_wstring(engine.rules.size())+L"\r\n";
    for(size_t i=0;i<engine.rules.size();++i) {
        auto& r=engine.rules[i];
        content+=L"\r\n[Rule"+std::to_wstring(i)+L"]\r\nShortcut="+wide(ps::format(r.sequence))+
            L"\r\nTarget="+std::to_wstring(r.target)+L"\r\nEnabled="+std::to_wstring(r.enabled)+L"\r\n";
    }
    auto temp=configPath+L".tmp";
    HANDLE f=CreateFileW(temp.c_str(),GENERIC_WRITE,0,nullptr,CREATE_ALWAYS,FILE_ATTRIBUTE_NORMAL,nullptr);
    if(f==INVALID_HANDLE_VALUE) { status(L"Settings could not be saved. Check folder permissions.");return false; }
    DWORD written=0,bytes=static_cast<DWORD>(content.size()*sizeof(wchar_t));
    bool ok=WriteFile(f,content.data(),bytes,&written,nullptr) && written==bytes;
    if(ok) ok=FlushFileBuffers(f)!=FALSE;
    CloseHandle(f);
    if(ok) ok=MoveFileExW(temp.c_str(),configPath.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH)!=FALSE;
    if(!ok) { DeleteFileW(temp.c_str());status(L"Settings could not be saved. Current changes remain active until exit."); }
    return ok;
}
void loadConfig() {
    PWSTR folder=nullptr;
    if(SUCCEEDED(SHGetKnownFolderPath(FOLDERID_LocalAppData,KF_FLAG_CREATE,nullptr,&folder))) {
        std::wstring dir=std::wstring(folder)+L"\\PrintShift";CoTaskMemFree(folder);
        if(CreateDirectoryW(dir.c_str(),nullptr) || GetLastError()==ERROR_ALREADY_EXISTS) configPath=dir+L"\\settings.ini";
    }
    if(configPath.empty() || GetFileAttributesW(configPath.c_str())==INVALID_FILE_ATTRIBUTES) {
        engine.rules.push_back({ps::parse("Ctrl+Shift+P"),0,true});return;
    }
    engine.enabled=GetPrivateProfileIntW(L"General",L"Enabled",1,configPath.c_str())!=0;
    startHidden=GetPrivateProfileIntW(L"General",L"StartHidden",0,configPath.c_str())!=0;
    exactModifiers=GetPrivateProfileIntW(L"General",L"ExactModifiers",1,configPath.c_str())!=0;
    physicalPositions=GetPrivateProfileIntW(L"General",L"PhysicalPositions",0,configPath.c_str())!=0;
    engine.timeout=std::clamp(GetPrivateProfileIntW(L"General",L"Timeout",1200,configPath.c_str()),300u,5000u);
    unsigned count=std::min(GetPrivateProfileIntW(L"General",L"Count",0,configPath.c_str()),64u);
    bool invalid=false;
    for(unsigned i=0;i<count;++i) {
        auto section=L"Rule"+std::to_wstring(i);wchar_t value[4096]{};
        GetPrivateProfileStringW(section.c_str(),L"Shortcut",L"",value,4096,configPath.c_str());
        try {
            ps::Rule r{ps::parse(ascii(value)),static_cast<int>(GetPrivateProfileIntW(section.c_str(),L"Target",0,configPath.c_str())),
                GetPrivateProfileIntW(section.c_str(),L"Enabled",1,configPath.c_str())!=0};
            if(!ps::validOutput(r.target)) throw std::runtime_error("Invalid target");
            for(auto& other:engine.rules) if(r.enabled && other.enabled && ps::ambiguous(r.sequence,other.sequence))
                throw std::runtime_error("Conflicting shortcut");
            engine.rules.push_back(std::move(r));
        } catch(...) { invalid=true; }
    }
    if(invalid) lastStatus=L"Some invalid or conflicting saved shortcuts were skipped. Review your list.";
}
HICON loadAppIcon(bool disabled=false) {
    HICON original=static_cast<HICON>(LoadImageW(instance,MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,LR_DEFAULTCOLOR));
    if(!original) return CopyIcon(LoadIconW(nullptr,IDI_APPLICATION));
    if(!disabled) return original;
    ICONINFO info{};
    if(!GetIconInfo(original,&info)) return original;
    BITMAP bitmap{};GetObjectW(info.hbmColor,sizeof(bitmap),&bitmap);
    BITMAPINFO bi{};bi.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);bi.bmiHeader.biWidth=bitmap.bmWidth;
    bi.bmiHeader.biHeight=-bitmap.bmHeight;bi.bmiHeader.biPlanes=1;bi.bmiHeader.biBitCount=32;bi.bmiHeader.biCompression=BI_RGB;
    std::vector<DWORD> pixels(static_cast<size_t>(bitmap.bmWidth)*bitmap.bmHeight);
    HDC dc=GetDC(nullptr);
    bool got=GetDIBits(dc,info.hbmColor,0,static_cast<UINT>(bitmap.bmHeight),pixels.data(),&bi,DIB_RGB_COLORS)!=0;
    if(got) {
        for(auto& p:pixels) {
            unsigned gray=((p>>16&255)*54+(p>>8&255)*183+(p&255)*19)/256;
            p=(p&0xff000000)|(gray<<16)|(gray<<8)|gray;
        }
        got=SetDIBits(dc,info.hbmColor,0,static_cast<UINT>(bitmap.bmHeight),pixels.data(),&bi,DIB_RGB_COLORS)!=0;
    }
    ReleaseDC(nullptr,dc);
    HICON result=got ? CreateIconIndirect(&info) : nullptr;
    DeleteObject(info.hbmMask);DeleteObject(info.hbmColor);
    if(result) { DestroyIcon(original);return result; }
    return original;
}
void updateTray(bool add=false) {
    NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=mainWindow;n.uID=1;
    n.uFlags=NIF_MESSAGE|NIF_ICON|NIF_TIP|NIF_SHOWTIP;n.uCallbackMessage=TrayMessage;
    n.hIcon=engine.enabled ? activeIcon : inactiveIcon;
    wcscpy_s(n.szTip,engine.enabled ? L"PrintShift - enabled" : L"PrintShift - disabled");
    if(add) {
        trayReady=Shell_NotifyIconW(NIM_ADD,&n)!=FALSE;
        if(trayReady) { n.uVersion=NOTIFYICON_VERSION_4;Shell_NotifyIconW(NIM_SETVERSION,&n); }
    } else if(trayReady) Shell_NotifyIconW(NIM_MODIFY,&n);
    SendMessageW(mainWindow,WM_SETICON,ICON_BIG,reinterpret_cast<LPARAM>(n.hIcon));
    SendMessageW(mainWindow,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(n.hIcon));
}
void showSettings() {
    ShowWindow(mainWindow,SW_RESTORE);SetForegroundWindow(mainWindow);
    if(editorWindow) { ShowWindow(editorWindow,SW_RESTORE);SetForegroundWindow(editorWindow); }
}
void hideToTray() {
    if(!trayReady) updateTray(true);
    if(trayReady) ShowWindow(mainWindow,SW_HIDE);
    else error(L"The notification icon is unavailable. The window will stay open so you can control the app.");
}
void replay() {
    auto pending=std::move(buffered);buffered.clear();
    if(!pending.empty() && SendInput(static_cast<UINT>(pending.size()),pending.data(),sizeof(INPUT))!=pending.size()) {
        engine.enabled=false;engine.reset();PostMessageW(mainWindow,ErrorMessage,0,0);
    }
}
void cancelIfFocusChanged() {
    if(!buffered.empty() && GetForegroundWindow()!=bufferedForeground) {
        buffered.clear();engine.reset();
    }
}
void resetGesture() { cancelIfFocusChanged();engine.reset();replay(); }
INPUT keyboardInput(const KBDLLHOOKSTRUCT& k,bool down) {
    INPUT in{};in.type=INPUT_KEYBOARD;
    if(k.scanCode) { in.ki.wScan=static_cast<WORD>(k.scanCode);in.ki.dwFlags=KEYEVENTF_SCANCODE; }
    else in.ki.wVk=static_cast<WORD>(k.vkCode);
    if(k.flags&LLKHF_EXTENDED) in.ki.dwFlags|=KEYEVENTF_EXTENDEDKEY;
    if(!down) in.ki.dwFlags|=KEYEVENTF_KEYUP;
    in.ki.dwExtraInfo=InjectionTag;return in;
}
void sendTarget(int target) {
    if(!ps::validOutput(target)) { PostMessageW(mainWindow,ErrorMessage,1,0);return; }
    std::vector<INPUT> input;
    for(const auto& stroke:ps::outputStrokes(target)) {
        INPUT i{};i.type=INPUT_KEYBOARD;i.ki.wVk=static_cast<WORD>(stroke.key);i.ki.dwExtraInfo=InjectionTag;
        i.ki.dwFlags=(stroke.down ? 0 : KEYEVENTF_KEYUP)|(stroke.extended ? KEYEVENTF_EXTENDEDKEY : 0);
        input.push_back(i);
    }
    auto sent=SendInput(static_cast<UINT>(input.size()),input.data(),sizeof(INPUT));
    if(sent!=input.size()) {
        // Best effort: release only output keys that may have been inserted.
        std::vector<INPUT> up;
        for(UINT i=0;i<sent;++i) if(!(input[i].ki.dwFlags&KEYEVENTF_KEYUP)) {
            auto release=input[i];release.ki.dwFlags|=KEYEVENTF_KEYUP;up.push_back(release);
        }
        if(!up.empty()) SendInput(static_cast<UINT>(up.size()),up.data(),sizeof(INPUT));
        PostMessageW(mainWindow,ErrorMessage,1,0);
    } else PostMessageW(mainWindow,WM_APP+4,static_cast<WPARAM>(target),0);
}
LRESULT CALLBACK keyboardHook(int code,WPARAM w,LPARAM l) {
    if(code<0) return CallNextHookEx(hook,code,w,l);
    auto& k=*reinterpret_cast<KBDLLHOOKSTRUCT*>(l);
    if(k.flags&LLKHF_INJECTED) return CallNextHookEx(hook,code,w,l);
    bool down=w==WM_KEYDOWN || w==WM_SYSKEYDOWN;
    if(w!=WM_KEYDOWN && w!=WM_SYSKEYDOWN && w!=WM_KEYUP && w!=WM_SYSKEYUP) return CallNextHookEx(hook,code,w,l);
    bool extended=(k.flags&LLKHF_EXTENDED)!=0;
    unsigned vk=keyTracker.event(k.vkCode,k.scanCode,extended,down);
    if(!down) {
        // Remove snapshots of keys held when the hook was installed/resynchronized.
        if(snapshotKeys.erase(k.vkCode)) engine.physical.erase(k.vkCode);
        unsigned normalized=ps::virtualKey(vk);
        if(snapshotKeys.erase(normalized)) engine.physical.erase(normalized);
    }
    if(recording) {
        if(down) engine.physical.insert(vk);else engine.physical.erase(vk);
        if(recordAwaitRelease) {
            if(engine.physical.empty()) recordAwaitRelease=false;
            return CallNextHookEx(hook,code,w,l);
        }
        if(ps::virtualKey(vk)==VK_ESCAPE && down) { PostMessageW(mainWindow,RecordMessage,1,0);return 1; }
        if(down) { inspectedKey=vk;inspectedVirtual=k.vkCode;PostMessageW(mainWindow,RecordMessage,2,0); }
        if(auto chord=recorder.event(vk,down,exactModifiers,physicalPositions)) {
            if(recorded.size()<16) recorded.push_back(*chord);
            PostMessageW(mainWindow,RecordMessage,0,0);
        }
        return 1;
    }
    cancelIfFocusChanged();
    // Let the user edit even a single-key binding without triggering that binding.
    if(editorWindow && GetForegroundWindow()==editorWindow) {
        engine.reset();
        if(down) engine.physical.insert(vk);else engine.physical.erase(vk);
        return CallNextHookEx(hook,code,w,l);
    }
    auto decision=engine.event(vk,down,GetTickCount64());
    if(decision.kind==ps::Kind::Pass) return CallNextHookEx(hook,code,w,l);
    if(buffered.empty()) bufferedForeground=GetForegroundWindow();
    buffered.push_back(keyboardInput(k,down));
    if(buffered.size()>512) { engine.reset();replay();return 1; }
    if(decision.kind==ps::Kind::Replay) replay();
    if(decision.kind==ps::Kind::Fire) { buffered.clear();sendTarget(decision.target); }
    return 1;
}
void syncPhysical() {
    engine.physical.clear();keyTracker.reset();snapshotKeys.clear();
    for(unsigned k=8;k<=254;++k) {
        if(k==VK_SHIFT || k==VK_CONTROL || k==VK_MENU) continue;
        if(GetAsyncKeyState(static_cast<int>(k))&0x8000) engine.physical.insert(k);
    }
    snapshotKeys=engine.physical;
}
void fonts() {
    HFONT oldBody=bodyFont,oldTitle=titleFont,oldSmall=smallFont;
    bodyFont=CreateFontW(-D(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    titleFont=CreateFontW(-D(30),0,0,0,FW_SEMIBOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    smallFont=CreateFontW(-D(13),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    for(HWND parent:{mainWindow}) if(parent) EnumChildWindows(parent,[](HWND child,LPARAM font)->BOOL {
        SendMessageW(child,WM_SETFONT,static_cast<WPARAM>(font),TRUE);return TRUE;
    },reinterpret_cast<LPARAM>(bodyFont));
    if(listWindow) SendMessageW(ListView_GetHeader(listWindow),WM_SETFONT,reinterpret_cast<WPARAM>(bodyFont),TRUE);
    if(oldBody) DeleteObject(oldBody);
    if(oldTitle) DeleteObject(oldTitle);
    if(oldSmall) DeleteObject(oldSmall);
}
HWND control(HWND parent,const wchar_t* cls,const wchar_t* label,DWORD style,int id,int x,int y,int width,int height,DWORD ex=0) {
    HWND h=CreateWindowExW(ex,cls,label,WS_CHILD|WS_VISIBLE|style,D(x),D(y),D(width),D(height),parent,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),instance,nullptr);
    SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(bodyFont),TRUE);return h;
}
void place(HWND parent,int id,int x,int y,int width,int height) {
    MoveWindow(GetDlgItem(parent,id),D(x),D(y),D(width),D(height),TRUE);
}
void refreshList() {
    refreshing=true;
    int selection=ListView_GetNextItem(listWindow,-1,LVNI_SELECTED);
    ListView_DeleteAllItems(listWindow);
    for(size_t i=0;i<engine.rules.size();++i) {
        auto& r=engine.rules[i];auto shortcut=wide(ps::format(r.sequence));
        LVITEMW item{};item.mask=LVIF_TEXT;item.iItem=static_cast<int>(i);item.pszText=shortcut.data();
        ListView_InsertItem(listWindow,&item);
        ListView_SetItemText(listWindow,static_cast<int>(i),1,const_cast<LPWSTR>(ps::Outputs[static_cast<unsigned>(r.target)].title));
        std::wstring steps=std::to_wstring(r.sequence.size());
        ListView_SetItemText(listWindow,static_cast<int>(i),2,steps.data());
        ListView_SetCheckState(listWindow,static_cast<int>(i),r.enabled);
    }
    if(selection>=0 && selection<static_cast<int>(engine.rules.size())) ListView_SetItemState(listWindow,selection,LVIS_SELECTED|LVIS_FOCUSED,LVIS_SELECTED|LVIS_FOCUSED);
    CheckDlgButton(mainWindow,GlobalEnable,engine.enabled ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(mainWindow,Startup,startupEnabled() ? BST_CHECKED : BST_UNCHECKED);
    CheckDlgButton(mainWindow,StartHidden,startHidden ? BST_CHECKED : BST_UNCHECKED);
    refreshing=false;updateTray();InvalidateRect(mainWindow,nullptr,FALSE);
    bool selected=ListView_GetNextItem(listWindow,-1,LVNI_SELECTED)>=0;
    EnableWindow(GetDlgItem(mainWindow,Edit),selected);EnableWindow(GetDlgItem(mainWindow,Remove),selected);
}
void layoutMain() {
    RECT r;GetClientRect(mainWindow,&r);int width=MulDiv(r.right,96,dpi),height=MulDiv(r.bottom,96,dpi);
    place(mainWindow,GlobalEnable,28,114,width-56,30);
    place(mainWindow,List,28,188,width-56,height-510);
    place(mainWindow,Add,28,height-308,100,34);place(mainWindow,Edit,138,height-308,100,34);place(mainWindow,Remove,248,height-308,100,34);
    place(mainWindow,Startup,28,height-256,320,28);place(mainWindow,StartHidden,28,height-222,320,28);
    place(mainWindow,Timeout,width-208,height-238,180,170);
    place(mainWindow,WindowsCapture,28,height-182,width-56,30);
    place(mainWindow,WindowsCaptureInfo,28,height-147,width-56,26);
    place(mainWindow,WindowsKeyboardSettings,28,height-112,218,30);
    place(mainWindow,Hide,width-158,height-62,130,34);
    if(statusWindow) MoveWindow(statusWindow,D(28),D(height-61),D(width-210),D(44),TRUE);
    int available=D(width-56);
    ListView_SetColumnWidth(listWindow,0,available-D(280));ListView_SetColumnWidth(listWindow,1,D(205));ListView_SetColumnWidth(listWindow,2,D(65));
}
void paintMain(HWND hwnd) {
    PAINTSTRUCT p;HDC dc=BeginPaint(hwnd,&p);RECT r;GetClientRect(hwnd,&r);FillRect(dc,&r,background);
    RECT header{0,0,r.right,D(92)};HBRUSH b=CreateSolidBrush(RGB(23,35,53));FillRect(dc,&header,b);DeleteObject(b);
    SetBkMode(dc,TRANSPARENT);SetTextColor(dc,RGB(255,255,255));auto old=SelectObject(dc,titleFont);
    RECT title{D(28),D(17),r.right-D(120),D(55)};DrawTextW(dc,L"PrintShift",-1,&title,DT_LEFT|DT_SINGLELINE);
    SelectObject(dc,smallFont);SetTextColor(dc,RGB(183,200,220));
    RECT sub{D(29),D(59),r.right-D(28),D(81)};DrawTextW(dc,L"A native home for your missing Print Screen key",-1,&sub,DT_LEFT|DT_SINGLELINE);
    RECT version{r.right-D(110),D(30),r.right-D(28),D(54)};DrawTextW(dc,L"v" PRINTSHIFT_VERSION_W,-1,&version,DT_RIGHT|DT_SINGLELINE);
    SelectObject(dc,bodyFont);SetTextColor(dc,RGB(57,72,93));
    RECT label{D(28),D(157),r.right-D(28),D(182)};
    std::wstring heading=L"YOUR SHORTCUTS    "+std::to_wstring(engine.rules.size())+L" / 64";
    DrawTextW(dc,heading.c_str(),-1,&label,DT_LEFT|DT_SINGLELINE);
    RECT timeoutLabel{r.right-D(208),r.bottom-D(262),r.right-D(28),r.bottom-D(239)};
    DrawTextW(dc,L"Time between steps",-1,&timeoutLabel,DT_LEFT|DT_SINGLELINE);
    SelectObject(dc,old);EndPaint(hwnd,&p);
}
bool conflict(const ps::Rule& rule,int skip) {
    if(!rule.enabled) return false;
    for(size_t i=0;i<engine.rules.size();++i) if(static_cast<int>(i)!=skip && engine.rules[i].enabled && ps::ambiguous(rule.sequence,engine.rules[i].sequence)) return true;
    return false;
}
void stopRecord() {
    recording=false;recordAwaitRelease=false;recorder.reset();engine.reset();
    if(editorWindow) {
        SetWindowTextW(GetDlgItem(editorWindow,Record),L"Record keys");
        EnableWindow(GetDlgItem(editorWindow,ExactModifiers),TRUE);
        EnableWindow(GetDlgItem(editorWindow,PhysicalPositions),TRUE);
        SetWindowTextW(GetDlgItem(editorWindow,EditorHelp),L"Use + for keys together; use a comma for the next step.\r\nNamed: RAlt+P    |    Physical: SCE038+SC019\r\nEnter = either key. MainEnter / NumEnter select one key.");
    }
}
void layoutEditor(HWND hwnd) {
    auto scale=[](int n) { return MulDiv(n,static_cast<int>(editorDpi),96); };
    struct Position { int id,x,y,w,h; };
    const Position positions[]={
        {401,26,22,560,25},{Shortcut,26,53,560,34},{Record,26,99,140,34},
        {ExactModifiers,184,99,402,34},{PhysicalPositions,26,136,560,28},
        {EditorHelp,26,176,560,66},{KeyInfo,26,249,560,30},{402,26,290,560,24},{Output,26,321,560,180},
        {RuleEnable,26,370,560,28},{Cancel,360,424,100,34},{Save,470,424,116,34}};
    for(auto p:positions) MoveWindow(GetDlgItem(hwnd,p.id),scale(p.x),scale(p.y),scale(p.w),scale(p.h),TRUE);
    HFONT old=editorFont;
    editorFont=CreateFontW(-scale(15),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,OUT_DEFAULT_PRECIS,CLIP_DEFAULT_PRECIS,CLEARTYPE_QUALITY,DEFAULT_PITCH,L"Segoe UI");
    EnumChildWindows(hwnd,[](HWND child,LPARAM font)->BOOL { SendMessageW(child,WM_SETFONT,static_cast<WPARAM>(font),TRUE);return TRUE; },reinterpret_cast<LPARAM>(editorFont));
    if(old) DeleteObject(old);
}
void closeEditor() {
    stopRecord();EnableWindow(mainWindow,TRUE);
    if(editorWindow) DestroyWindow(editorWindow);
    editorWindow=nullptr;
    if(editorFont) { DeleteObject(editorFont);editorFont=nullptr; }
    SetForegroundWindow(mainWindow);
}
void saveEditor() {
    if(recording) { error(L"Stop recording before saving.",editorWindow);return; }
    try {
        auto seq=ps::parse(ascii(text(GetDlgItem(editorWindow,Shortcut))));
        int target=static_cast<int>(SendDlgItemMessageW(editorWindow,Output,CB_GETCURSEL,0,0));
        if(!ps::validOutput(target)) throw std::runtime_error("Choose an output key.");
        ps::Rule rule{seq,target,IsDlgButtonChecked(editorWindow,RuleEnable)==BST_CHECKED};
        if(conflict(rule,editing)) throw std::runtime_error("This shortcut overlaps another enabled shortcut. Change it or disable the other shortcut first.");
        resetGesture();
        if(editing<0) engine.rules.push_back(rule);else engine.rules[editing]=rule;
        if(saveConfig()) status(L"Shortcut saved. Output is sent after you release the gesture keys.");
        closeEditor();refreshList();
    } catch(const std::exception& e) { error(wide(e.what()),editorWindow); }
}
LRESULT CALLBACK editorProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    switch(msg) {
    case WM_CREATE: {
        control(hwnd,L"STATIC",L"Shortcut",0,401,26,22,560,25);
        control(hwnd,L"EDIT",L"",WS_TABSTOP|ES_AUTOHSCROLL,Shortcut,26,53,560,34,WS_EX_CLIENTEDGE);
        control(hwnd,L"BUTTON",L"Record keys",WS_TABSTOP|BS_PUSHBUTTON,Record,26,99,140,34);
        control(hwnd,L"BUTTON",L"Distinguish left/right modifiers",WS_TABSTOP|BS_AUTOCHECKBOX,ExactModifiers,184,99,402,34);
        CheckDlgButton(hwnd,ExactModifiers,exactModifiers ? BST_CHECKED : BST_UNCHECKED);
        control(hwnd,L"BUTTON",L"Bind physical key positions (scan codes)",WS_TABSTOP|BS_AUTOCHECKBOX,PhysicalPositions,26,136,560,28);
        CheckDlgButton(hwnd,PhysicalPositions,physicalPositions ? BST_CHECKED : BST_UNCHECKED);
        control(hwnd,L"STATIC",L"Key inspector: start recording to inspect a key.",0,KeyInfo,26,249,560,30);
        control(hwnd,L"STATIC",L"",0,EditorHelp,26,149,560,82);
        control(hwnd,L"STATIC",L"Send to Windows",0,402,26,243,560,24);
        HWND combo=control(hwnd,L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Output,26,274,560,180);
        for(const auto& target:ps::Outputs) SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(target.title));
        control(hwnd,L"BUTTON",L"Enable this shortcut",WS_TABSTOP|BS_AUTOCHECKBOX,RuleEnable,26,323,560,28);
        control(hwnd,L"BUTTON",L"Cancel",WS_TABSTOP,Cancel,360,378,100,34);
        control(hwnd,L"BUTTON",L"Save shortcut",WS_TABSTOP|BS_DEFPUSHBUTTON,Save,470,378,116,34);
        if(editing>=0) {
            auto& rule=engine.rules[editing];SetDlgItemTextW(hwnd,Shortcut,wide(ps::format(rule.sequence)).c_str());
            SendMessageW(combo,CB_SETCURSEL,rule.target,0);CheckDlgButton(hwnd,RuleEnable,rule.enabled ? BST_CHECKED : BST_UNCHECKED);
        } else { SendMessageW(combo,CB_SETCURSEL,0,0);CheckDlgButton(hwnd,RuleEnable,BST_CHECKED); }
        editorDpi=GetDpiForWindow(hwnd);layoutEditor(hwnd);break;
    }
    case WM_COMMAND:
        switch(LOWORD(w)) {
        case IDOK: case Save:saveEditor();return 0;
        case IDCANCEL: case Cancel:closeEditor();return 0;
        case Record:
            if(recording) stopRecord();else {
                resetGesture();recorded.clear();recorder.reset();syncPhysical();
                recordAwaitRelease=!engine.physical.empty();recording=true;
                EnableWindow(GetDlgItem(hwnd,ExactModifiers),FALSE);
                EnableWindow(GetDlgItem(hwnd,PhysicalPositions),FALSE);
                SetWindowTextW(GetDlgItem(hwnd,Record),L"Stop recording");
                SetWindowTextW(GetDlgItem(hwnd,EditorHelp),L"Recording globally. Press a chord, then release a key to finish it.\r\nYou may keep shared modifiers held for the next step.\r\nClick Stop recording when finished. Esc cancels recording.");
            }
            return 0;
        case ExactModifiers:
            exactModifiers=IsDlgButtonChecked(hwnd,ExactModifiers)==BST_CHECKED;saveConfig();return 0;
        case PhysicalPositions:
            physicalPositions=IsDlgButtonChecked(hwnd,PhysicalPositions)==BST_CHECKED;saveConfig();return 0;
        }
        break;
    case WM_CLOSE:closeEditor();return 0;
    case WM_DPICHANGED: {
        editorDpi=HIWORD(w);auto r=reinterpret_cast<RECT*>(l);
        SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
        layoutEditor(hwnd);return 0;
    }
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN: {
        HDC dc=reinterpret_cast<HDC>(w);SetBkColor(dc,RGB(245,247,251));SetTextColor(dc,RGB(40,54,75));return reinterpret_cast<LRESULT>(background);
    }
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
void openEditor(int selected) {
    if(editorWindow) { SetForegroundWindow(editorWindow);return; }
    if(selected<0 && engine.rules.size()>=64) { error(L"You can define up to 64 shortcuts.");return; }
    resetGesture();editing=selected;
    RECT r;GetWindowRect(mainWindow,&r);RECT size{0,0,D(612),D(484)};
    AdjustWindowRectEx(&size,WS_CAPTION|WS_SYSMENU,FALSE,WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT);
    editorWindow=CreateWindowExW(WS_EX_DLGMODALFRAME|WS_EX_CONTROLPARENT,EditorClass,
        selected<0 ? L"Add shortcut - PrintShift" : L"Edit shortcut - PrintShift",WS_CAPTION|WS_SYSMENU,
        r.left+(r.right-r.left-(size.right-size.left))/2,r.top+D(60),size.right-size.left,size.bottom-size.top,
        mainWindow,nullptr,instance,nullptr);
    if(!editorWindow) { error(L"Could not create the shortcut editor.");return; }
    stopRecord();EnableWindow(mainWindow,FALSE);ShowWindow(editorWindow,SW_SHOW);
    SetFocus(GetDlgItem(editorWindow,Shortcut));
}
void toggleGlobal() {
    if(recording) stopRecord();
    resetGesture();engine.enabled=!engine.enabled;
    if(saveConfig()) status(engine.enabled ? L"Shortcuts enabled." : L"Shortcuts disabled. Your keyboard works normally.");
    refreshList();
}
void trayMenu() {
    HMENU menu=CreatePopupMenu();
    AppendMenuW(menu,MF_STRING|(engine.enabled ? MF_CHECKED : MF_UNCHECKED),TrayEnable,L"Enabled");
    AppendMenuW(menu,MF_STRING,TraySettings,L"Settings...");AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
    AppendMenuW(menu,MF_STRING,TrayExit,L"Exit PrintShift");
    POINT p;GetCursorPos(&p);SetForegroundWindow(editorWindow ? editorWindow : mainWindow);
    UINT command=TrackPopupMenu(menu,TPM_RETURNCMD|TPM_RIGHTBUTTON,p.x,p.y,0,mainWindow,nullptr);
    DestroyMenu(menu);PostMessageW(mainWindow,WM_NULL,0,0);
    if(command) PostMessageW(mainWindow,WM_COMMAND,command,0);
}
LRESULT CALLBACK mainProc(HWND hwnd,UINT msg,WPARAM w,LPARAM l) {
    if(msg==showMessage) { showSettings();return 0; }
    if(msg==taskbarCreated) { trayReady=false;updateTray(true);if(!trayReady) showSettings();return 0; }
    switch(msg) {
    case WM_CREATE: {
        mainWindow=hwnd;dpi=GetDpiForWindow(hwnd);fonts();
        control(hwnd,L"BUTTON",L"Enable keyboard shortcuts",WS_TABSTOP|BS_AUTOCHECKBOX,GlobalEnable,28,114,600,30);
        listWindow=control(hwnd,WC_LISTVIEWW,L"",WS_TABSTOP|LVS_REPORT|LVS_SINGLESEL|LVS_SHOWSELALWAYS,List,28,188,780,270,WS_EX_CLIENTEDGE);
        SetWindowTheme(listWindow,L"Explorer",nullptr);
        ListView_SetExtendedListViewStyle(listWindow,LVS_EX_FULLROWSELECT|LVS_EX_DOUBLEBUFFER|LVS_EX_CHECKBOXES|LVS_EX_LABELTIP);
        ListView_SetBkColor(listWindow,RGB(255,255,255));ListView_SetTextBkColor(listWindow,RGB(255,255,255));
        ListView_SetTextColor(listWindow,RGB(27,43,62));
        const wchar_t* columns[]={L"Shortcut",L"Sends",L"Steps"};
        for(int i=0;i<3;++i) { LVCOLUMNW col{};col.mask=LVCF_TEXT|LVCF_WIDTH;col.pszText=const_cast<LPWSTR>(columns[i]);col.cx=D(i==0 ? 450 : 180);ListView_InsertColumn(listWindow,i,&col); }
        control(hwnd,L"BUTTON",L"Add shortcut",WS_TABSTOP,Add,28,470,100,34);
        control(hwnd,L"BUTTON",L"Edit",WS_TABSTOP,Edit,138,470,100,34);
        control(hwnd,L"BUTTON",L"Remove",WS_TABSTOP,Remove,248,470,100,34);
        control(hwnd,L"BUTTON",L"Run when I sign in to Windows",WS_TABSTOP|BS_AUTOCHECKBOX,Startup,28,520,320,28);
        control(hwnd,L"BUTTON",L"Open in the notification area",WS_TABSTOP|BS_AUTOCHECKBOX,StartHidden,28,554,320,28);
        HWND timeout=control(hwnd,L"COMBOBOX",L"",WS_TABSTOP|CBS_DROPDOWNLIST|WS_VSCROLL,Timeout,630,540,180,170);
        const unsigned choices[]={500,800,1200,1800,2500,4000,5000};int selected=0;
        for(unsigned choice:choices) {
            std::wstring label=std::to_wstring(choice)+L" ms";int index=static_cast<int>(SendMessageW(timeout,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str())));
            SendMessageW(timeout,CB_SETITEMDATA,index,choice);if(choice==engine.timeout) selected=index;
        }
        bool found=false;for(unsigned c:choices) if(c==engine.timeout) found=true;
        if(!found) {
            auto label=std::to_wstring(engine.timeout)+L" ms";selected=static_cast<int>(SendMessageW(timeout,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label.c_str())));
            SendMessageW(timeout,CB_SETITEMDATA,selected,engine.timeout);
        }
        SendMessageW(timeout,CB_SETCURSEL,selected,0);
        statusWindow=control(hwnd,L"STATIC",lastStatus.c_str(),0,-1,28,620,600,44);
        control(hwnd,L"BUTTON",L"Hide to tray",WS_TABSTOP,Hide,680,620,130,34);
        control(hwnd,L"BUTTON",L"Use the Print screen key to open screen capture",WS_TABSTOP|BS_CHECKBOX,WindowsCapture,28,608,780,30);
        control(hwnd,L"STATIC",L"",0,WindowsCaptureInfo,28,643,780,26);
        control(hwnd,L"BUTTON",L"Windows keyboard settings",WS_TABSTOP,WindowsKeyboardSettings,28,678,218,30);
        refreshWindowsCaptureSetting();
        SetTimer(hwnd,1,25,nullptr);SetTimer(hwnd,2,1000,nullptr);
        WTSRegisterSessionNotification(hwnd,NOTIFY_FOR_THIS_SESSION);layoutMain();return 0;
    }
    case WM_PAINT:paintMain(hwnd);return 0;
    case WM_ERASEBKGND:return 1;
    case WM_SIZE:
        if(w==SIZE_MINIMIZED) hideToTray();else if(listWindow) layoutMain();return 0;
    case WM_GETMINMAXINFO: {
        auto info=reinterpret_cast<MINMAXINFO*>(l);info->ptMinTrackSize={D(780),D(700)};
        MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
        if(GetMonitorInfoW(MonitorFromWindow(hwnd,MONITOR_DEFAULTTONEAREST),&monitor)) {
            info->ptMinTrackSize.x=std::min(info->ptMinTrackSize.x,monitor.rcWork.right-monitor.rcWork.left);
            info->ptMinTrackSize.y=std::min(info->ptMinTrackSize.y,monitor.rcWork.bottom-monitor.rcWork.top);
        }
        return 0;
    }
    case WM_DPICHANGED: {
        dpi=HIWORD(w);fonts();auto r=reinterpret_cast<RECT*>(l);
        SetWindowPos(hwnd,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);
        layoutMain();InvalidateRect(hwnd,nullptr,TRUE);return 0;
    }
    case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN: {
        HDC dc=reinterpret_cast<HDC>(w);SetTextColor(dc,RGB(40,54,75));SetBkColor(dc,RGB(245,247,251));return reinterpret_cast<LRESULT>(background);
    }
    case WM_COMMAND:
        switch(LOWORD(w)) {
        case Add:openEditor(-1);return 0;
        case Edit: { int i=ListView_GetNextItem(listWindow,-1,LVNI_SELECTED);if(i>=0) openEditor(i);return 0; }
        case Remove: {
            int i=ListView_GetNextItem(listWindow,-1,LVNI_SELECTED);
            if(i>=0) { resetGesture();engine.rules.erase(engine.rules.begin()+i);if(saveConfig()) status(L"Shortcut removed.");refreshList(); }
            return 0;
        }
        case GlobalEnable:case TrayEnable:toggleGlobal();return 0;
        case Startup: {
            bool on=IsDlgButtonChecked(hwnd,Startup)==BST_CHECKED;
            if(!setStartup(on)) error(L"Could not update startup. Keep the executable in a stable folder with a path under 240 characters.");
            CheckDlgButton(hwnd,Startup,startupEnabled() ? BST_CHECKED : BST_UNCHECKED);return 0;
        }
        case StartHidden:startHidden=IsDlgButtonChecked(hwnd,StartHidden)==BST_CHECKED;saveConfig();return 0;
        case WindowsCapture:toggleWindowsCaptureSetting();return 0;
        case WindowsKeyboardSettings: {
            const auto opened=reinterpret_cast<INT_PTR>(ShellExecuteW(hwnd,L"open",L"ms-settings:easeofaccess-keyboard",nullptr,nullptr,SW_SHOWNORMAL));
            if(opened<=32) error(L"Windows keyboard settings could not be opened. Search Windows Settings for Print screen.");
            return 0;
        }
        case Timeout:
            if(HIWORD(w)==CBN_SELCHANGE) {
                resetGesture();int i=static_cast<int>(SendDlgItemMessageW(hwnd,Timeout,CB_GETCURSEL,0,0));
                engine.timeout=static_cast<unsigned>(SendDlgItemMessageW(hwnd,Timeout,CB_GETITEMDATA,i,0));saveConfig();
            }
            return 0;
        case Hide:hideToTray();return 0;
        case TraySettings:showSettings();return 0;
        case TrayExit:if(editorWindow) closeEditor();DestroyWindow(hwnd);return 0;
        }
        break;
    case WM_NOTIFY: {
        auto header=reinterpret_cast<NMHDR*>(l);
        if(header->idFrom==List) {
            if(header->code==NM_DBLCLK) { int i=ListView_GetNextItem(listWindow,-1,LVNI_SELECTED);if(i>=0) openEditor(i); }
            if(header->code==LVN_ITEMCHANGED && !refreshing) {
                auto change=reinterpret_cast<NMLISTVIEW*>(l);int i=change->iItem;
                bool selected=ListView_GetNextItem(listWindow,-1,LVNI_SELECTED)>=0;
                EnableWindow(GetDlgItem(hwnd,Edit),selected);EnableWindow(GetDlgItem(hwnd,Remove),selected);
                if(i>=0 && i<static_cast<int>(engine.rules.size()) && (change->uChanged&LVIF_STATE) &&
                    ((change->uOldState^change->uNewState)&LVIS_STATEIMAGEMASK)) {
                    auto rule=engine.rules[i];rule.enabled=ListView_GetCheckState(listWindow,i)!=FALSE;
                    if(conflict(rule,i)) { error(L"This shortcut overlaps another enabled shortcut.");refreshList(); }
                    else { resetGesture();engine.rules[i]=rule;saveConfig(); }
                }
            }
        }
        return 0;
    }
    case TrayMessage:
        if(LOWORD(l)==WM_CONTEXTMENU) trayMenu();
        else if(LOWORD(l)==NIN_SELECT || LOWORD(l)==NIN_KEYSELECT || LOWORD(l)==WM_LBUTTONDBLCLK) showSettings();
        return 0;
    case RecordMessage:
        if(editorWindow) {
            if(w==2) {
                auto name=wide(ps::keyName(ps::virtualKey(inspectedKey)));
                auto scan=ps::scanCode(inspectedKey) ? wide(ps::keyName(ps::scanKey(ps::scanCode(inspectedKey),ps::extendedKey(inspectedKey)))) : L"none";
                wchar_t info[160]{};swprintf_s(info,L"Key: %s   |   VK %02X   |   %s",name.c_str(),inspectedVirtual,scan.c_str());
                SetDlgItemTextW(editorWindow,KeyInfo,info);return 0;
            }
            if(!recorded.empty()) SetDlgItemTextW(editorWindow,Shortcut,wide(ps::format(recorded)).c_str());
            if(w==1) stopRecord();
        }
        return 0;
    case ErrorMessage:
        if(w==0) { saveConfig();refreshList(); }
        status(w==0 ? L"Input replay failed. Shortcuts disabled. Release any held keys." : L"Windows blocked the output. Check the foreground app's privilege level.");
        return 0;
    case WM_APP+4:
        if(w<ps::Outputs.size()) status(std::wstring(L"Sent ")+ps::Outputs[w].title+L". Behavior follows your Windows screenshot settings.");
        return 0;
    case WM_TIMER:
        if(w==2) { if(IsWindowVisible(hwnd)) refreshWindowsCaptureSetting();return 0; }
        cancelIfFocusChanged();
        if(!recording && engine.tick(GetTickCount64()).kind==ps::Kind::Replay) replay();
        return 0;
    case WM_SETTINGCHANGE:refreshWindowsCaptureSetting();return 0;
    case WM_ACTIVATEAPP:if(w) refreshWindowsCaptureSetting();return 0;
    case WM_WTSSESSION_CHANGE:
        if(w==WTS_SESSION_LOCK || w==WTS_SESSION_UNLOCK || w==WTS_CONSOLE_CONNECT || w==WTS_REMOTE_CONNECT) {
            if(recording) stopRecord();
            buffered.clear();engine.reset();syncPhysical();
        }
        return 0;
    case WM_POWERBROADCAST:
        if(w==PBT_APMRESUMEAUTOMATIC || w==PBT_APMRESUMESUSPEND) {
            if(recording) stopRecord();
            buffered.clear();engine.reset();syncPhysical();
        }
        return TRUE;
    case WM_QUERYENDSESSION:return TRUE;
    case WM_ENDSESSION:if(w) { resetGesture();saveConfig(); }return 0;
    case WM_CLOSE:hideToTray();return 0;
    case WM_DESTROY: {
        KillTimer(hwnd,1);KillTimer(hwnd,2);recording=false;resetGesture();
        WTSUnRegisterSessionNotification(hwnd);
        if(hook) { UnhookWindowsHookEx(hook);hook=nullptr; }
        NOTIFYICONDATAW n{};n.cbSize=sizeof(n);n.hWnd=hwnd;n.uID=1;Shell_NotifyIconW(NIM_DELETE,&n);
        PostQuitMessage(0);return 0;
    }
    }
    return DefWindowProcW(hwnd,msg,w,l);
}
}
int nativeSmokeTest(HINSTANCE app) {
    if(!FindResourceW(app,MAKEINTRESOURCEW(1),RT_GROUP_ICON) ||
       !FindResourceW(app,MAKEINTRESOURCEW(1),RT_MANIFEST) ||
       !FindResourceW(app,MAKEINTRESOURCEW(1),RT_VERSION)) return 1;
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};
    if(!InitCommonControlsEx(&controls)) return 2;
    HWND hidden=CreateWindowExW(WS_EX_TOOLWINDOW,L"STATIC",L"PrintShift self-test",WS_POPUP,
        0,0,32,32,nullptr,nullptr,app,nullptr);
    if(!hidden) return 3;
    HICON icon=static_cast<HICON>(LoadImageW(app,MAKEINTRESOURCEW(1),IMAGE_ICON,32,32,LR_DEFAULTCOLOR));
    bool ok=icon && GetDpiForWindow(hidden)>0;
    if(icon) DestroyIcon(icon);
    DestroyWindow(hidden);return ok ? 0 : 4;
}
int WINAPI wWinMain(HINSTANCE app,HINSTANCE,PWSTR command,int show) {
    if(command && std::wstring(command)==L"--self-test") return nativeSmokeTest(app);
    instance=app;showMessage=RegisterWindowMessageW(L"PrintShift.ShowSettings.1");
    singleton=CreateMutexW(nullptr,FALSE,L"Local\\PrintShift.Singleton.1");
    if(!singleton) { error(L"Could not create the application instance.");return 1; }
    if(GetLastError()==ERROR_ALREADY_EXISTS) {
        if(HWND other=FindWindowW(ClassName,nullptr)) { AllowSetForegroundWindow(ASFW_ANY);PostMessageW(other,showMessage,0,0); }
        CloseHandle(singleton);return 0;
    }
    CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_LISTVIEW_CLASSES|ICC_STANDARD_CLASSES};InitCommonControlsEx(&controls);
    loadConfig();activeIcon=loadAppIcon();inactiveIcon=loadAppIcon(true);
    taskbarCreated=RegisterWindowMessageW(L"TaskbarCreated");
    WNDCLASSEXW wc{};wc.cbSize=sizeof(wc);wc.hInstance=app;wc.lpfnWndProc=mainProc;
    wc.lpszClassName=ClassName;wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=activeIcon;wc.hIconSm=activeIcon;
    RegisterClassExW(&wc);wc.lpszClassName=EditorClass;wc.lpfnWndProc=editorProc;wc.hbrBackground=background;RegisterClassExW(&wc);
    mainWindow=CreateWindowExW(WS_EX_CONTROLPARENT,ClassName,L"PrintShift - Keyboard shortcuts",WS_OVERLAPPEDWINDOW,
        CW_USEDEFAULT,CW_USEDEFAULT,920,860,nullptr,nullptr,app,nullptr);
    if(!mainWindow) { error(L"Could not create the settings window.");CloseHandle(singleton);return 1; }
    // Size the initial client area in logical pixels on high-DPI monitors.
    RECT size{0,0,D(860),D(790)};AdjustWindowRectExForDpi(&size,WS_OVERLAPPEDWINDOW,FALSE,WS_EX_CONTROLPARENT,dpi);
    MONITORINFO monitor{};monitor.cbSize=sizeof(monitor);
    if(GetMonitorInfoW(MonitorFromWindow(mainWindow,MONITOR_DEFAULTTONEAREST),&monitor)) {
        LONG width=std::min(size.right-size.left,monitor.rcWork.right-monitor.rcWork.left);
        LONG height=std::min(size.bottom-size.top,monitor.rcWork.bottom-monitor.rcWork.top);
        SetWindowPos(mainWindow,nullptr,monitor.rcWork.left+(monitor.rcWork.right-monitor.rcWork.left-width)/2,
            monitor.rcWork.top+(monitor.rcWork.bottom-monitor.rcWork.top-height)/2,width,height,SWP_NOZORDER);
    } else SetWindowPos(mainWindow,nullptr,0,0,size.right-size.left,size.bottom-size.top,SWP_NOMOVE|SWP_NOZORDER);
    syncPhysical();hook=SetWindowsHookExW(WH_KEYBOARD_LL,keyboardHook,app,0);
    if(!hook) { error(L"Windows could not install the keyboard hook. PrintShift cannot run.");DestroyWindow(mainWindow);CloseHandle(singleton);return 1; }
    updateTray(true);refreshList();
    bool trayLaunch=command && wcsstr(command,L"--tray");
    ShowWindow(mainWindow,(trayReady && (trayLaunch || startHidden)) ? SW_HIDE : show);UpdateWindow(mainWindow);
    MSG msg{};BOOL result;
    while((result=GetMessageW(&msg,nullptr,0,0))>0) {
        if(editorWindow && IsDialogMessageW(editorWindow,&msg)) continue;
        if(IsWindowEnabled(mainWindow) && IsDialogMessageW(mainWindow,&msg)) continue;
        TranslateMessage(&msg);DispatchMessageW(&msg);
    }
    if(bodyFont) DeleteObject(bodyFont);
    if(titleFont) DeleteObject(titleFont);
    if(smallFont) DeleteObject(smallFont);
    DeleteObject(background);DestroyIcon(activeIcon);DestroyIcon(inactiveIcon);
    CloseHandle(singleton);CoUninitialize();return result==-1 ? 1 : static_cast<int>(msg.wParam);
}
