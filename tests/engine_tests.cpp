#include "../src/engine.hpp"
#include "../src/capture_defaults.hpp"
#include "../src/outputs.hpp"
#include <cassert>
#include <iostream>
using namespace ps;
int main() {
    assert(format(parse(" control + shift + p, F24 "))=="Shift+Ctrl+P, F24");
    assert(parse("A+B")[0].size()==2);
    assert(parse("LCtrl+RCtrl+Num0")[0].size()==3);
    assert(parse("Plus+Comma")[0].size()==2);
    for(const auto& bad:{"", "A+", "A,,B", "Ctrl+LCtrl+P", "Ctrl+Alt+Delete", "Win+L", "Fn", "F25"}) {
        bool failed=false;try { parse(bad); } catch(...) { failed=true; }assert(failed);
    }
    assert(ambiguous(parse("A"),parse("A+B")));
    assert(ambiguous(parse("Ctrl+K"),parse("Ctrl+K, Ctrl+C")));
    assert(ambiguous(parse("Ctrl+P"),parse("LCtrl+P")));
    assert(!ambiguous(parse("Ctrl+K, Ctrl+C"),parse("Ctrl+K, Ctrl+P")));
    assert(!ambiguous(parse("LCtrl+P"),parse("RCtrl+P")));
    Engine e;e.rules={{parse("Ctrl+K, Ctrl+P"),2,true},{parse("Ctrl+K, Ctrl+C"),1,true}};
    uint64_t t=0;
    auto step=[&](unsigned k,bool d,Kind want) {assert(e.event(k,d,++t).kind==want);};
    step(0xA2,true,Kind::Hold);step('K',true,Kind::Hold);step('K',false,Kind::Hold);
    step('P',true,Kind::Hold);step('P',true,Kind::Hold);step('P',false,Kind::Hold);
    auto fire=e.event(0xA2,false,++t);assert(fire.kind==Kind::Fire && fire.target==2);
    step(0xA3,true,Kind::Hold);step('K',true,Kind::Hold);step('K',false,Kind::Hold);
    step(0xA3,false,Kind::Hold);step(0xA3,true,Kind::Hold);step('C',true,Kind::Hold);
    step('C',false,Kind::Hold);assert(e.event(0xA3,false,++t).target==1);
    step(0xA2,true,Kind::Hold);step('X',true,Kind::Replay);step('X',false,Kind::Pass);
    step(0xA2,false,Kind::Pass);
    step(0xA2,true,Kind::Hold);assert(e.tick(t+1201).kind==Kind::Replay);step(0xA2,false,Kind::Pass);
    e.rules={{parse("A+B"),0,true}};
    step('A',true,Kind::Hold);step('B',true,Kind::Hold);step('A',false,Kind::Hold);step('B',false,Kind::Fire);
    step('A',true,Kind::Hold);step('A',false,Kind::Replay);
    e.rules={{parse("P, P"),0,true}};
    step('P',true,Kind::Hold);step('P',true,Kind::Hold);step('P',false,Kind::Hold);
    step('P',true,Kind::Hold);step('P',false,Kind::Fire);
    e.rules={{parse("A"),0,true}};
    step('A',true,Kind::Hold);step('Z',true,Kind::Replay);step('A',false,Kind::Pass);step('Z',false,Kind::Pass);
    e.enabled=false;step('A',true,Kind::Pass);step('A',false,Kind::Pass);
    // Never swallow an up event if its down event was passed before recognition.
    e.enabled=true;e.rules={{parse("Ctrl+P"),0,true}};
    step('Z',true,Kind::Pass);step(0xA2,true,Kind::Pass);step('Z',false,Kind::Pass);
    step('P',true,Kind::Pass);step('P',false,Kind::Pass);step(0xA2,false,Kind::Pass);
    // A plain physical modifier never equals the synthetic "Win" alias.
    assert(matches({0x5B,'P'},parse("Win+P")[0]));
    // The recorder preserves physical sides by default, with an explicit generic mode.
    for(auto pair:std::vector<std::pair<unsigned,unsigned>>{{0xA0,0xA1},{0xA2,0xA3},{0xA4,0xA5},{0x5B,0x5C}}) {
        auto left=pair.first,right=pair.second;
        assert(recordingKey(left,true)==left && recordingKey(right,true)==right);
        assert(recordingKey(left,false)==recordingKey(right,false));
        assert(!matches({left,'P'},{right,'P'}));
        assert(matches({left,'P'},{generic(left),'P'}));
        assert(matches({right,'P'},{generic(right),'P'}));
        assert(!ambiguous({{left,'P'}},{{right,'P'}}));
        assert(ambiguous({{generic(left),'P'}},{{right,'P'}}));
        Engine sided;sided.rules={{{{left,'P'}},1,true},{{{right,'P'}},2,true}};
        for(auto key:{left,right}) {
            assert(sided.event(key,true,0).kind==Kind::Hold);
            assert(sided.event('P',true,1).kind==Kind::Hold);
            assert(sided.event('P',false,2).kind==Kind::Hold);
            auto result=sided.event(key,false,3);
            assert(result.kind==Kind::Fire && result.target==(key==left ? 1 : 2));
        }
    }
    assert(normalizeKey(0x10,0x2A,false)==0xA0);
    assert(normalizeKey(0x10,0x36,false)==0xA1);
    assert(normalizeKey(0x11,0x1D,false)==0xA2);
    assert(normalizeKey(0x11,0x1D,true)==0xA3);
    assert(normalizeKey(0x12,0x38,false)==0xA4);
    assert(normalizeKey(0x12,0x38,true)==0xA5);
    assert(normalizeKey(13,0x1C,false)==MainEnter);
    assert(normalizeKey(13,0x1C,true)==NumEnter);
    assert(parse("MainEnter")[0]==Keys{MainEnter});
    assert(parse("NumpadEnter")[0]==Keys{NumEnter});
    assert(matches({MainEnter},parse("Enter")[0]));
    assert(matches({NumEnter},parse("Enter")[0]));
    assert(!matches({MainEnter},parse("NumEnter")[0]));
    assert(!matches({NumEnter},parse("MainEnter")[0]));
    assert(!ambiguous(parse("MainEnter"),parse("NumEnter")));
    assert(ambiguous(parse("Enter"),parse("NumEnter")));
    assert(normalizeKey(0x67,0x47,false)==103); // keypad 7, Num Lock on
    assert(normalizeKey(0x24,0x47,false)==103); // keypad 7/Home, Num Lock off
    assert(normalizeKey(0x24,0x47,true)==0x24); // dedicated Home
    assert(normalizeKey(0x6E,0x53,false)==110);
    assert(normalizeKey(0x2E,0x53,false)==110); // keypad decimal/Delete
    assert(normalizeKey(0x2E,0x53,true)==0x2E); // dedicated Delete
    assert(normalizeKey('7',0x08,false)=='7'); // top-row 7
    assert(normalizeKey('P',0x19,false)=='P');
    Recorder recorder;
    assert(!recorder.event(0xA5,true,true));
    assert(!recorder.event('K',true,true));
    assert(!recorder.event('K',true,true)); // auto-repeat must not split the chord
    assert((recorder.event('K',false,true)==std::optional<Keys>{{0xA5,'K'}}));
    assert(!recorder.event('P',true,true)); // retain Right Alt across sequence steps
    assert((recorder.event('P',false,true)==std::optional<Keys>{{0xA5,'P'}}));
    assert(!recorder.event(0xA5,false,true)); // no spurious trailing modifier step
    assert(!recorder.event(0xA4,true,false));
    assert(!recorder.event('P',true,false));
    assert((recorder.event('P',false,false)==std::optional<Keys>{{Alt,'P'}}));
    assert(!recorder.event(0xA4,false,false));
    recorder.reset();
    assert(!recorder.event(MainEnter,true,true));
    assert(recorder.event(MainEnter,false,true)==std::optional<Keys>{{MainEnter}});
    // Physical scan codes remain distinct even when Windows assigns the same VK.
    for(bool extended:{false,true}) for(unsigned scan:{1u,0x1Cu,0x38u,0x56u,0xFFu,0x100u,0xFFFFu}) {
        unsigned actual=physicalKey('A',scan,extended),binding=scanKey(scan,extended);
        assert(scanCode(actual)==scan && extendedKey(actual)==extended);
        assert(matches({actual},{binding}));
        assert(!matches({physicalKey('A',scan,!extended)},{binding}));
        assert(parse(keyName(binding))[0]==Keys{binding});
    }
    unsigned iso=physicalKey(226,0x56,false),oem=physicalKey(226,0x73,false);
    assert(iso!=oem);
    assert(matches({iso},parse("SC056")[0]));
    assert(!matches({oem},parse("SC056")[0]));
    Engine physical;physical.rules={{parse("SC056"),0,true},{parse("SC073"),2,true}};
    for(auto key:{iso,oem}) {
        assert(physical.event(key,true,10).kind==Kind::Hold);
        auto result=physical.event(key,false,11);
        assert(result.kind==Kind::Fire && result.target==(key==iso ? 0 : 2));
    }
    unsigned lc=physicalKey(0x11,0x1D,false),rc=physicalKey(0x11,0x1D,true);
    assert(matches({lc},parse("LCtrl")[0]) && !matches({rc},parse("LCtrl")[0]));
    assert(matches({rc},parse("RCtrl")[0]) && !matches({lc},parse("RCtrl")[0]));
    assert(!matches({lc},parse("Ctrl+SC01D")[0])); // one event cannot count as two keys
    assert(matches({lc,rc},parse("Ctrl+SCE01D")[0]));
    assert(recordingKey(rc,true,true)==scanKey(0x1D,true));
    assert(recordingKey(rc,false,true)==Ctrl);
    assert(matches({physicalKey(13,0x1C,false)},parse("MainEnter")[0]));
    assert(matches({physicalKey(13,0x1C,true)},parse("NumEnter")[0]));
    assert(!matches({physicalKey(13,0x1C,false)},parse("SCE01C")[0]));
    assert(physicalKey(0x24,0x47,false)==physicalKey(103,0x47,false));
    assert(physicalKey(0x24,0x47,true)!=physicalKey(103,0x47,false));
    assert(!ambiguous(parse("Ctrl+Shift+SC019"),parse("SCE038+SC025")));
    assert(!ambiguous(parse("Ctrl+SC019"),parse("Ctrl+SC025")));
    assert(ambiguous(parse("SC01C"),parse("Enter")));
    assert(!ambiguous(parse("SC01C"),parse("NumEnter")));
    assert(hardwareIdentity(19,0x45,false)!=hardwareIdentity(144,0x45,false));
    for(const auto& entry:names()) assert(parse(entry.first)[0]==Keys{entry.second});
    for(unsigned vk=8;vk<=254;++vk) assert(parse(keyName(vk))[0]==Keys{vk});
    for(const auto& bad:{"SC000", "SCE000", "SC1FFFF", "SCXYZ", "SCE0"}) {
        bool rejected=false;try { parse(bad); } catch(...) { rejected=true; }assert(rejected);
    }
    recorder.reset();
    unsigned ra=physicalKey(0x12,0x38,true),kp=physicalKey('K',0x25,false),pp=physicalKey('P',0x19,false);
    assert(!recorder.event(ra,true,true,true));
    assert(!recorder.event(kp,true,true,true));
    auto first=recorder.event(kp,false,true,true);
    assert(first && *first==parse("SCE038+SC025")[0]);
    assert(!recorder.event(pp,true,true,true));
    auto second=recorder.event(pp,false,true,true);
    assert(second && *second==parse("SCE038+SC019")[0]);
    assert(!recorder.event(ra,false,true,true));
    Engine recordedEngine;recordedEngine.rules={{{*first,*second},1,true}};
    for(auto event:std::vector<std::pair<unsigned,bool>>{{ra,true},{kp,true},{kp,false},{pp,true},{pp,false}})
        assert(recordedEngine.event(event.first,event.second,20).kind==Kind::Hold);
    assert(recordedEngine.event(ra,false,21).kind==Kind::Fire);
    KeyTracker tracker;
    unsigned firstVendor=tracker.event(180,0x100,false,true);
    unsigned secondVendor=tracker.event(181,0x100,false,true);
    assert(firstVendor!=secondVendor);
    assert(tracker.event(180,0x100,false,false)==firstVendor);
    assert(tracker.event(181,0x100,false,false)==secondVendor);
    unsigned beforeLayoutChange=tracker.event('A',0x1E,false,true);
    assert(tracker.event('Q',0x1E,false,false)==beforeLayoutChange);
    assert(normalizeKey(175,0x47,false)==175); // vendor/media VK must not become keypad 7
    // An unset preference resolves to On/Off only for a known Windows default.
    assert(windowsCaptureDefault(10,0,19045,std::nullopt,true)==std::optional<bool>{false});
    assert(windowsCaptureDefault(10,0,22000,std::nullopt,true)==std::optional<bool>{false});
    assert(windowsCaptureDefault(10,0,22621,1545,true)==std::optional<bool>{false});
    assert(!windowsCaptureDefault(10,0,22621,1546,true).has_value());
    assert(!windowsCaptureDefault(10,0,22621,1927,true).has_value());
    assert(windowsCaptureDefault(10,0,22621,1928,true)==std::optional<bool>{true});
    assert(windowsCaptureDefault(10,0,22631,std::nullopt,true)==std::optional<bool>{true});
    assert(windowsCaptureDefault(10,0,26100,std::nullopt,true)==std::optional<bool>{true});
    assert(!windowsCaptureDefault(10,0,22621,std::nullopt,true).has_value());
    assert(!windowsCaptureDefault(10,0,22624,1546,true).has_value());
    assert(!windowsCaptureDefault(10,0,26100,std::nullopt,false).has_value());
    assert(!windowsCaptureDefault(11,0,30000,std::nullopt,true).has_value());
    // Exercise the output plans used by SendInput, including balanced modifier releases.
    const unsigned modifiers[]={0,0xA4,0x5B,0xA0,0xA2};
    assert(Outputs.size()==5);
    assert(std::wstring(Outputs[3].title)==L"Shift + Print Screen");
    assert(std::wstring(Outputs[4].title)==L"Ctrl + Print Screen");
    for(int target=0;target<5;++target) {
        assert(validOutput(target));
        const auto strokes=outputStrokes(target);
        const size_t offset=target==0 ? 0u : 1u;
        assert(strokes.size()==(target==0 ? 2u : 4u));
        assert(strokes[offset].key==0x2C && strokes[offset].down && strokes[offset].extended);
        assert(strokes[offset+1].key==0x2C && !strokes[offset+1].down && strokes[offset+1].extended);
        if(target!=0) {
            assert(strokes.front().key==modifiers[target] && strokes.front().down);
            assert(strokes.back().key==modifiers[target] && !strokes.back().down);
            assert(strokes.front().extended==(target==2));
            assert(strokes.back().extended==(target==2));
        }
        Keys heldOutput;
        for(const auto& stroke:strokes) {
            if(stroke.down) assert(heldOutput.insert(stroke.key).second);
            else assert(heldOutput.erase(stroke.key)==1);
        }
        assert(heldOutput.empty());
        Engine dispatch;dispatch.rules={{parse("F10"),target,true}};
        assert(dispatch.event(121,true,1).kind==Kind::Hold);
        const auto emitted=dispatch.event(121,false,2);
        assert(emitted.kind==Kind::Fire && emitted.target==target);
    }
    for(int invalid:{-1,5,100}) {
        assert(!validOutput(invalid));
        bool rejected=false;try { outputStrokes(invalid); } catch(const std::invalid_argument&) { rejected=true; }
        assert(rejected);
    }
    std::cout << "All key identity, parser, recorder, sequence, Windows-default and output-plan tests passed.\n";
}
