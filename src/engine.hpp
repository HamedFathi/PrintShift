#pragma once
#include <algorithm>
#include <cctype>
#include <cstdint>
#include <optional>
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace ps {
using Keys = std::set<unsigned>;
constexpr unsigned Ctrl=0x11, Shift=0x10, Alt=0x12, Win=0x100;
constexpr unsigned MainEnter=0x200,NumEnter=0x201;
constexpr unsigned ActualFlag=0x80000000u,ScanFlag=0x40000000u;
inline bool isActual(unsigned key) { return (key&ActualFlag)!=0; }
inline bool isScan(unsigned key) { return (key&ScanFlag)!=0 && !isActual(key); }
inline unsigned virtualKey(unsigned key) { return isActual(key) ? key&0x3ffu : key; }
inline unsigned scanCode(unsigned key) { return isActual(key) ? (key>>10)&0xffffu : key&0xffffu; }
inline bool extendedKey(unsigned key) { return (key&(isActual(key) ? 0x04000000u : 0x10000u))!=0; }
inline unsigned scanKey(unsigned scan,bool extended) { return ScanFlag|(scan&0xffffu)|(extended ? 0x10000u : 0u); }
inline unsigned normalizeKey(unsigned vk,unsigned scan,bool extended);
inline unsigned physicalKey(unsigned vk,unsigned scan,bool extended) {
    return ActualFlag|normalizeKey(vk,scan,extended)|((scan&0xffffu)<<10)|(extended ? 0x04000000u : 0u);
}
inline unsigned hardwareIdentity(unsigned vk,unsigned scan,bool extended) {
    // Retain VK as well: vendor keys and Pause/Num Lock can share scan codes.
    return (scan&0xffffu)|(extended ? 0x10000u : 0u)|(normalizeKey(vk,scan,extended)<<18);
}
class KeyTracker {
    std::map<unsigned,unsigned> held_;
public:
    void reset() { held_.clear(); }
    unsigned event(unsigned vk,unsigned scan,bool extended,bool down) {
        unsigned id=hardwareIdentity(vk,scan,extended),key=physicalKey(vk,scan,extended);
        auto found=held_.find(id);
        if(!down && found==held_.end() && scan!=0) {
            auto candidate=held_.end();
            for(auto it=held_.begin();it!=held_.end();++it) if(scanCode(it->second)==scan && extendedKey(it->second)==extended) {
                if(candidate!=held_.end()) { candidate=held_.end();break; }
                candidate=it;
            }
            found=candidate;
        }
        if(found!=held_.end()) key=found->second;
        if(down) held_[id]=key;
        else if(found!=held_.end()) held_.erase(found);
        return key;
    }
};
inline bool covers(unsigned want,unsigned actual) {
    if(isScan(want)) return isActual(actual) && scanCode(actual)!=0 &&
        scanCode(want)==scanCode(actual) && extendedKey(want)==extendedKey(actual);
    actual=virtualKey(actual);
    return want==actual || (want==13 && (actual==MainEnter || actual==NumEnter)) ||
        (want==Ctrl && (actual==0xA2 || actual==0xA3)) ||
        (want==Shift && (actual==0xA0 || actual==0xA1)) ||
        (want==Alt && (actual==0xA4 || actual==0xA5)) ||
        (want==Win && (actual==0x5B || actual==0x5C));
}
inline bool subset(const Keys& actual,const Keys& wanted) {
    for(auto a:actual) {
        bool found=false;
        for(auto w:wanted) if(covers(w,a)) { found=true; break; }
        if(!found) return false;
    }
    return true;
}
inline bool matches(const Keys& actual,const Keys& wanted) {
    if(actual.size()<wanted.size() || !subset(actual,wanted)) return false;
    std::vector<unsigned> have(actual.begin(),actual.end()),need(wanted.begin(),wanted.end());
    std::vector<int> assigned(have.size(),-1);
    auto assign=[&](auto&& self,size_t index,std::vector<bool>& seen)->bool {
        for(size_t i=0;i<have.size();++i) if(!seen[i] && covers(need[index],have[i])) {
            seen[i]=true;
            if(assigned[i]<0 || self(self,static_cast<size_t>(assigned[i]),seen)) {
                assigned[i]=static_cast<int>(index);return true;
            }
        }
        return false;
    };
    for(size_t i=0;i<need.size();++i) {
        std::vector<bool> seen(have.size(),false);
        if(!assign(assign,i,seen)) return false;
    }
    return true;
}
inline unsigned generic(unsigned k) {
    k=virtualKey(k);
    if(k==0xA2 || k==0xA3) return Ctrl;
    if(k==0xA0 || k==0xA1) return Shift;
    if(k==0xA4 || k==0xA5) return Alt;
    if(k==0x5B || k==0x5C) return Win;
    return k;
}
// Normalize physical keyboard events without collapsing left/right modifiers.
// MainEnter/NumEnter remain distinct; the Enter binding intentionally accepts both.
inline unsigned normalizeKey(unsigned vk,unsigned scan,bool extended) {
    if(vk==0x10) return scan==0x36 ? 0xA1 : 0xA0;
    if(vk==0x11) return extended ? 0xA3 : 0xA2;
    if(vk==0x12) return extended ? 0xA5 : 0xA4;
    if(vk==13) return extended ? NumEnter : MainEnter;
    if(!extended && (vk==0 || (vk>=96 && vk<=105) || (vk>=33 && vk<=40) || vk==12 || vk==45 || vk==46 || vk==110)) {
        // Keep keypad identities stable with Num Lock either on or off.
        switch(scan) {
        case 0x47:return 103;case 0x48:return 104;case 0x49:return 105;
        case 0x4B:return 100;case 0x4C:return 101;case 0x4D:return 102;
        case 0x4F:return 97;case 0x50:return 98;case 0x51:return 99;
        case 0x52:return 96;case 0x53:return 110;
        }
    }
    return vk;
}
inline unsigned recordingKey(unsigned key,bool exactModifiers,bool physicalPositions=false) {
    unsigned vk=virtualKey(key),g=generic(vk);
    bool modifier=g==Ctrl || g==Shift || g==Alt || g==Win;
    // Pause/PrintScreen have special make sequences; their named identities are safer.
    if(physicalPositions && (exactModifiers || !modifier) && isActual(key) && scanCode(key)!=0 && vk!=19 && vk!=44)
        return scanKey(scanCode(key),extendedKey(key));
    if(modifier) return exactModifiers ? vk : g;
    return vk;
}
class Recorder {
    Keys held_,peak_;
public:
    void reset() { held_.clear();peak_.clear(); }
    std::optional<Keys> event(unsigned key,bool down,bool exactModifiers,bool physicalPositions=false) {
        bool repeat=down && held_.count(key);
        if(down) held_.insert(key);else held_.erase(key);
        if(down && !repeat) {
            peak_.clear();
            for(auto k:held_) peak_.insert(recordingKey(k,exactModifiers,physicalPositions));
        }
        if(!down && !peak_.empty()) {
            auto chord=std::move(peak_);peak_.clear();return chord;
        }
        return std::nullopt;
    }
};
inline std::string trim(std::string s) {
    auto a=s.find_first_not_of(" \t\r\n"),b=s.find_last_not_of(" \t\r\n");
    return a==std::string::npos ? "" : s.substr(a,b-a+1);
}
inline const std::vector<std::pair<std::string,unsigned>>& names() {
    static const std::vector<std::pair<std::string,unsigned>> n={
        {"Ctrl",Ctrl},{"Shift",Shift},{"Alt",Alt},{"Win",Win},
        {"LCtrl",0xA2},{"RCtrl",0xA3},{"LShift",0xA0},{"RShift",0xA1},
        {"LAlt",0xA4},{"RAlt",0xA5},{"LWin",0x5B},{"RWin",0x5C},
        {"Cancel",3},{"Backspace",8},{"Tab",9},{"Clear",12},{"Enter",13},{"MainEnter",MainEnter},{"NumEnter",NumEnter},{"Pause",19},{"CapsLock",20},
        {"IMEKana",21},{"IMEJunja",23},{"IMEFinal",24},{"IMEKanji",25},
        {"IMEConvert",28},{"IMENonConvert",29},{"IMEAccept",30},{"IMEModeChange",31},
        {"Esc",27},{"Space",32},{"PageUp",33},{"PageDown",34},{"End",35},
        {"Home",36},{"Left",37},{"Up",38},{"Right",39},{"Down",40},
        {"Select",41},{"Print",42},{"Execute",43},{"PrintScreen",44},{"Insert",45},{"Delete",46},{"Help",47},{"Menu",93},{"Sleep",95},
        {"NumMultiply",106},{"NumAdd",107},{"NumSubtract",109},{"NumDecimal",110},
        {"NumDivide",111},{"NumLock",144},{"ScrollLock",145},
        {"VolumeMute",173},{"VolumeDown",174},{"VolumeUp",175},
        {"MediaNext",176},{"MediaPrevious",177},{"MediaStop",178},{"MediaPlay",179},
        {"BrowserBack",166},{"BrowserForward",167},{"BrowserRefresh",168},{"BrowserStop",169},
        {"BrowserSearch",170},{"BrowserFavorites",171},{"BrowserHome",172},
        {"LaunchMail",180},{"LaunchMedia",181},{"LaunchApp1",182},{"LaunchApp2",183},
        {"Semicolon",186},{"Plus",187},{"Comma",188},{"Minus",189},
        {"Period",190},{"Slash",191},{"Backtick",192},{"LBracket",219},
        {"Backslash",220},{"RBracket",221},{"Quote",222},{"OEM102",226},
        {"ProcessKey",229},{"Packet",231},{"Attn",246},{"CrSel",247},{"ExSel",248},
        {"EraseEOF",249},{"Play",250},{"Zoom",251},{"NoName",252},{"PA1",253},{"OEMClear",254}};
    return n;
}
inline std::string upper(std::string s) {
    for(auto& c:s) c=static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    return s;
}
inline unsigned parseKey(std::string s) {
    s=upper(trim(s));
    if(s.size()==1 && ((s[0]>='A' && s[0]<='Z') || (s[0]>='0' && s[0]<='9'))) return s[0];
    if(s=="CONTROL") s="CTRL";
    if(s=="ESCAPE") s="ESC";
    if(s=="NUMPADENTER") s="NUMENTER";
    if(s=="PRTSC" || s=="PRTSCN") s="PRINTSCREEN";
    for(const auto& n:names()) if(upper(n.first)==s) return n.second;
    if(s.rfind("SC",0)==0) {
        bool extended=s.rfind("SCE0",0)==0;
        auto digits=s.substr(extended ? 4 : 2);
        if(digits.size()>=2 && digits.size()<=4 && digits.find_first_not_of("0123456789ABCDEF")==std::string::npos) {
            auto scan=static_cast<unsigned>(std::stoul(digits,nullptr,16));
            if(scan>0) return scanKey(scan,extended);
        }
        throw std::runtime_error("Invalid scan code. Use SC01C or SCE01C.");
    }
    if(s.size()>1 && s[0]=='F') {
        auto t=s.substr(1);
        if(t.find_first_not_of("0123456789")==std::string::npos && t.size()<=2) {
            int v=std::stoi(t); if(v>=1 && v<=24) return 111+v;
        }
    }
    if(s.size()==4 && s.substr(0,3)=="NUM" && s[3]>='0' && s[3]<='9') return 96+s[3]-'0';
    if(s.size()==4 && s.substr(0,2)=="VK" && s.find_first_not_of("0123456789ABCDEF",2)==std::string::npos) {
        auto k=static_cast<unsigned>(std::stoul(s.substr(2),nullptr,16));
        if(k>=8 && k<=254) return k;
    }
    throw std::runtime_error("Unknown key: "+s+". Use a key name or VKxx (hex).");
}
inline std::string keyName(unsigned k) {
    if(isActual(k)) k=virtualKey(k);
    if(isScan(k)) {
        const char* hex="0123456789ABCDEF";unsigned scan=scanCode(k);
        std::string digits;
        do { digits=hex[scan&15]+digits;scan>>=4; } while(scan);
        while(digits.size()<(extendedKey(k) ? 2u : 3u)) digits="0"+digits;
        return std::string(extendedKey(k) ? "SCE0" : "SC")+digits;
    }
    for(const auto& n:names()) if(n.second==k) return n.first;
    if((k>='A' && k<='Z') || (k>='0' && k<='9')) return std::string(1,static_cast<char>(k));
    if(k>=112 && k<=135) return "F"+std::to_string(k-111);
    if(k>=96 && k<=105) return "Num"+std::to_string(k-96);
    const char* hex="0123456789ABCDEF";
    return std::string("VK")+hex[(k>>4)&15]+hex[k&15];
}
inline std::string chordName(const Keys& keys) {
    std::string s;
    for(bool modifiers:{true,false}) for(auto k:keys) {
        auto g=generic(k); bool m=g==Ctrl || g==Shift || g==Alt || g==Win;
        if(m!=modifiers) continue;
        if(!s.empty()) s+="+";
        s+=keyName(k);
    }
    return s;
}
using Sequence=std::vector<Keys>;
inline Sequence parse(const std::string& text) {
    Sequence seq;
    size_t pos=0;
    do {
        auto end=text.find(',',pos);
        auto chord=trim(text.substr(pos,end==std::string::npos ? end : end-pos));
        if(chord.empty()) throw std::runtime_error("Empty shortcut step.");
        Keys keys;
        size_t p=0;
        do {
            auto e=chord.find('+',p);
            auto token=trim(chord.substr(p,e==std::string::npos ? e : e-p));
            if(token.empty()) throw std::runtime_error("Empty key. Use Plus for the + key.");
            unsigned k=parseKey(token);
            for(auto previous:keys) if(covers(previous,k) || covers(k,previous))
                throw std::runtime_error("Duplicate or overlapping modifier in one step.");
            keys.insert(k);
            if(e==std::string::npos) break;
            p=e+1;
        } while(true);
        if(keys.size()>12) throw std::runtime_error("Maximum 12 keys per step.");
        Keys genericKeys; for(auto k:keys) genericKeys.insert(generic(k));
        if((genericKeys.count(Ctrl) && genericKeys.count(Alt) && keys.count(46)) ||
            (genericKeys.count(Win) && keys.count('L')))
            throw std::runtime_error("This shortcut is reserved by Windows.");
        seq.push_back(keys);
        if(seq.size()>16) throw std::runtime_error("Maximum 16 steps per shortcut.");
        if(end==std::string::npos) break;
        pos=end+1;
    } while(true);
    return seq;
}
inline std::string format(const Sequence& s) {
    std::string result;
    for(const auto& k:s) { if(!result.empty()) result+=", "; result+=chordName(k); }
    return result;
}
struct Rule { Sequence sequence; int target=0; bool enabled=true; };
inline bool keyPredicatesOverlap(unsigned a,unsigned b) {
    if(isScan(a) && isScan(b)) return a==b;
    // Layouts/drivers can map a physical scan code to different virtual keys.
    // Conservatively reject potentially overlapping physical/virtual definitions.
    if(isScan(a) || isScan(b)) {
        unsigned sc=isScan(a) ? a : b,vk=isScan(a) ? b : a;
        unsigned code=scanCode(sc),known=normalizeKey(0,code,extendedKey(sc));
        if(code==0x1D) known=extendedKey(sc) ? 0xA3 : 0xA2;
        if(code==0x38) known=extendedKey(sc) ? 0xA5 : 0xA4;
        if(code==0x2A && !extendedKey(sc)) known=0xA0;
        if(code==0x36 && !extendedKey(sc)) known=0xA1;
        if(code==0x1C) known=extendedKey(sc) ? NumEnter : MainEnter;
        if(code==0x5B && extendedKey(sc)) known=0x5B;
        if(code==0x5C && extendedKey(sc)) known=0x5C;
        if(known) return covers(vk,known);
        unsigned g=generic(vk);
        if(g==Ctrl || g==Shift || g==Alt || g==Win || vk==MainEnter || vk==NumEnter || vk==13) return false;
        return true;
    }
    return covers(a,b) || covers(b,a);
}
// Reject competing completed chords and sequence prefixes before they become active.
inline bool ambiguous(const Sequence& a,const Sequence& b) {
    for(size_t i=0;i<std::min(a.size(),b.size());++i) {
        Keys left=a[i],right=b[i];
        for(auto key:a[i]) if(b[i].count(key)) { left.erase(key);right.erase(key); }
        auto canBePrefix=[](const Keys& x,const Keys& y) {
            for(auto k:x) {
                bool possible=false;
                for(auto other:y) if(keyPredicatesOverlap(k,other)) { possible=true;break; }
                if(!possible) return false;
            }
            return true;
        };
        bool overlap=canBePrefix(left,right) || canBePrefix(right,left);
        if(!overlap) return false;
        if(a[i]!=b[i]) return true;
    }
    return true;
}
enum class Kind { Pass, Hold, Replay, Fire };
struct Decision { Kind kind=Kind::Pass; int target=0; };
class Engine {
    struct Candidate { size_t rule,step=0; };
    std::vector<Candidate> candidates_;
    bool active_=false,finished_=false;
    uint64_t deadline_=0;
    int target_=0;
public:
    std::vector<Rule> rules;
    Keys physical;
    bool enabled=true;
    unsigned timeout=1200;
    bool active() const { return active_; }
    void reset() { active_=finished_=false; candidates_.clear(); }
    Decision tick(uint64_t now) {
        if(active_ && now>=deadline_) { reset();return {Kind::Replay}; }
        return {};
    }
    Decision event(unsigned key,bool down,uint64_t now) {
        bool wasEmpty=physical.empty(),repeat=down && physical.count(key);
        if(down) physical.insert(key);else physical.erase(key);
        if(!enabled) return {};
        bool expired=active_ && now>=deadline_;
        if(expired) { reset();return {Kind::Replay}; }
        if(!active_) {
            if(!down || !wasEmpty) return {};
            candidates_.clear();
            for(size_t i=0;i<rules.size();++i) if(rules[i].enabled && !rules[i].sequence.empty() &&
                subset(physical,rules[i].sequence[0])) candidates_.push_back({i,0});
            if(candidates_.empty()) return {};
            active_=true; deadline_=now+timeout;
        }
        if(finished_) {
            if(down && !repeat) { reset();return {Kind::Replay}; }
            if(physical.empty()) { int t=target_;reset();return {Kind::Fire,t}; }
            return {Kind::Hold};
        }
        if(down && !repeat) {
            std::vector<Candidate> next;
            for(auto c:candidates_) {
                auto& rule=rules[c.rule];
                if(matches(physical,rule.sequence[c.step])) {
                    ++c.step; deadline_=now+timeout;
                    if(c.step==rule.sequence.size()) {
                        finished_=true;target_=rule.target;deadline_=now+5000;return {Kind::Hold};
                    }
                    next.push_back(c);
                } else if(subset(physical,rule.sequence[c.step])) next.push_back(c);
            }
            candidates_=std::move(next);
            if(candidates_.empty()) { reset();return {Kind::Replay}; }
        }
        if(!down && physical.empty()) {
            bool anyProgress=false;
            for(auto c:candidates_) if(c.step>0) anyProgress=true;
            if(!anyProgress) { reset();return {Kind::Replay}; }
        }
        return {Kind::Hold};
    }
};
}
