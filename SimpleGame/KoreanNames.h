#pragma once
#include <cstdint>
#include <string>

inline std::wstring KoreanPersonalName(std::uint64_t identity)
{
    static const wchar_t* surnames =
        L"김이박최정강조윤장임한오서신권황안송전홍유고문양손배백허남심노하곽성차주우구민진지엄채원천방공현함변염여추도소석선설마길연위표명기반왕금옥육인맹제모탁국어은편용예봉경사부복태목형피두감음빈동온호범좌";
    static const wchar_t* first =
        L"도서지민하예시수유태은준나소연재다채승건아우현성정진혜해보담솔가";
    static const wchar_t* last =
        L"윤준연우현아빈진영원호희온율민서경수린솔하람안재담나림선채결봄설";
    const std::uint64_t a = std::char_traits<wchar_t>::length(surnames);
    const std::uint64_t b = std::char_traits<wchar_t>::length(first);
    const std::uint64_t c = std::char_traits<wchar_t>::length(last);
    std::uint64_t value = identity ? identity - 1 : 0;
    const std::uint64_t capacity = a * b * c;
    std::uint64_t generation = value / capacity;
    value = (value % capacity * 7919) % capacity;
    std::wstring name;
    name += surnames[value % a];
    name += first[(value / a) % b];
    name += last[(value / (a * b)) % c];
    if (generation)
    {
        while (generation)
        {
            name += wchar_t(0xAC00 + (generation - 1) % 11172);
            generation = (generation - 1) / 11172;
        }
    }
    return name;
}
