#pragma once
#include <array>
#include <stdexcept>
#include <vector>

namespace ps {
struct OutputSpec {
    const wchar_t* title;
    unsigned modifier;
    bool extended;
};
// IDs 0-2 remain unchanged so existing saved mappings keep their original output.
inline constexpr std::array<OutputSpec,5> Outputs{{
    {L"Print Screen",0,false},
    {L"Alt + Print Screen",0xA4,false},
    {L"Win + Print Screen",0x5B,true},
    {L"Shift + Print Screen",0xA0,false},
    {L"Ctrl + Print Screen",0xA2,false}
}};
inline bool validOutput(int target) {
    return target>=0 && static_cast<unsigned>(target)<Outputs.size();
}
struct OutputStroke {
    unsigned key;
    bool down;
    bool extended;
};
inline std::vector<OutputStroke> outputStrokes(int target) {
    if(!validOutput(target)) throw std::invalid_argument("Invalid Print Screen output");
    const auto& output=Outputs[static_cast<unsigned>(target)];
    std::vector<OutputStroke> result;
    if(output.modifier) result.push_back({output.modifier,true,output.extended});
    result.push_back({0x2C,true,true});
    result.push_back({0x2C,false,true});
    if(output.modifier) result.push_back({output.modifier,false,output.extended});
    return result;
}
}
