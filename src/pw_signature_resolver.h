#pragma once

#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace pw::signature {

struct BytePattern {
    const std::uint8_t* bytes{};
    const std::uint8_t* significant{};  // nullptr means every byte is exact.
    std::size_t size{};
};

struct Region {
    const std::uint8_t* bytes{};
    std::size_t size{};
    std::uintptr_t address{};

    constexpr bool valid() const { return bytes && size && address; }
};

enum class MatchState : std::uint8_t {
    none,
    unique,
    ambiguous,
};

struct MatchResult {
    MatchState state{MatchState::none};
    std::uintptr_t address{};
    std::size_t count{};
};

inline bool valid_pattern(const BytePattern& pattern) {
    return pattern.bytes && pattern.size;
}

inline bool matches(const std::uint8_t* candidate, std::size_t available,
                    const BytePattern& pattern) {
    if (!candidate || !valid_pattern(pattern) || available < pattern.size)
        return false;
    for (std::size_t index = 0; index < pattern.size; ++index) {
        if ((!pattern.significant || pattern.significant[index]) &&
            candidate[index] != pattern.bytes[index]) {
            return false;
        }
    }
    return true;
}

// A locator is accepted only when it has exactly one match. Returning the
// first result from an ambiguous pattern could redirect execution into an
// unrelated function after a game update.
inline MatchResult find_unique(const Region& region,
                               const BytePattern& pattern) {
    if (!region.valid() || !valid_pattern(pattern) ||
        pattern.size > region.size) {
        return {};
    }

    MatchResult result{};
    const std::size_t last = region.size - pattern.size;
    for (std::size_t offset = 0; offset <= last; ++offset) {
        if (!matches(region.bytes + offset, region.size - offset, pattern))
            continue;
        ++result.count;
        if (result.count == 1) {
            result.address = region.address + offset;
        } else {
            result.state = MatchState::ambiguous;
            result.address = 0;
            return result;
        }
    }
    if (result.count == 1) result.state = MatchState::unique;
    return result;
}

inline bool section_name_equals(const IMAGE_SECTION_HEADER& section,
                                const char* name) {
    if (!name) return false;
    char actual[IMAGE_SIZEOF_SHORT_NAME + 1]{};
    std::memcpy(actual, section.Name, IMAGE_SIZEOF_SHORT_NAME);
    return std::strncmp(actual, name, IMAGE_SIZEOF_SHORT_NAME) == 0;
}

// Searches are restricted to one mapped PE section. This avoids unrelated
// modules and keeps every memory access inside the executable image.
inline Region mapped_image_section(std::uintptr_t base, const char* name) {
    if (!base) return {};
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(base);
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0)
        return {};
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        base + static_cast<std::uintptr_t>(dos->e_lfanew));
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC ||
        nt->FileHeader.NumberOfSections == 0 ||
        nt->FileHeader.NumberOfSections > 96) {
        return {};
    }

    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections;
         ++index, ++section) {
        if (!section_name_equals(*section, name)) continue;
        const std::size_t remaining =
            nt->OptionalHeader.SizeOfImage > section->VirtualAddress
                ? nt->OptionalHeader.SizeOfImage - section->VirtualAddress
                : 0;
        const std::size_t size =
            (std::min)(static_cast<std::size_t>(section->Misc.VirtualSize),
                       remaining);
        if (!size) return {};
        const std::uintptr_t address = base + section->VirtualAddress;
        return {reinterpret_cast<const std::uint8_t*>(address), size, address};
    }
    return {};
}

inline bool contains(const Region& region, std::uintptr_t address,
                     std::size_t size = 1) {
    if (!region.valid() || address < region.address) return false;
    const std::size_t offset = address - region.address;
    return offset <= region.size && size <= region.size - offset;
}

// Decode an x86-64 rel32 call or RIP-relative memory operand. The displacement
// is relative to the end of the complete instruction.
inline bool decode_relative_target(std::uintptr_t instruction,
                                   std::size_t displacement_offset,
                                   std::size_t instruction_size,
                                   std::uintptr_t* target) {
    if (!instruction || !target || instruction_size < sizeof(std::int32_t) ||
        displacement_offset > instruction_size - sizeof(std::int32_t)) {
        return false;
    }
    std::int32_t displacement{};
    std::memcpy(&displacement,
                reinterpret_cast<const void*>(instruction +
                                              displacement_offset),
                sizeof(displacement));
    const std::int64_t next = static_cast<std::int64_t>(instruction) +
        static_cast<std::int64_t>(instruction_size);
    const std::int64_t resolved = next + displacement;
    if (resolved < 0 ||
        static_cast<std::uint64_t>(resolved) >
            (std::numeric_limits<std::uintptr_t>::max)()) {
        return false;
    }
    *target = static_cast<std::uintptr_t>(resolved);
    return true;
}

}  // namespace pw::signature
