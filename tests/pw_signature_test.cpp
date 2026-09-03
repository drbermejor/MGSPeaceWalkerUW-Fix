#include "pw_patch_targets.h"

#include <array>
#include <cstdint>
#include <cstring>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace {

int failures = 0;

void expect(bool condition, const char* message) {
    if (condition) return;
    ++failures;
    std::cerr << "FAIL: " << message << '\n';
}

struct RawSection {
    const std::uint8_t* bytes{};
    std::size_t size{};
    std::uintptr_t rva{};
};

bool raw_section(const std::vector<std::uint8_t>& file, const char* name,
                 RawSection* out) {
    if (!out || file.size() < sizeof(IMAGE_DOS_HEADER)) return false;
    const auto* dos = reinterpret_cast<const IMAGE_DOS_HEADER*>(file.data());
    if (dos->e_magic != IMAGE_DOS_SIGNATURE || dos->e_lfanew <= 0 ||
        static_cast<std::size_t>(dos->e_lfanew) +
                sizeof(IMAGE_NT_HEADERS64) >
            file.size()) {
        return false;
    }
    const auto* nt = reinterpret_cast<const IMAGE_NT_HEADERS64*>(
        file.data() + dos->e_lfanew);
    if (nt->Signature != IMAGE_NT_SIGNATURE ||
        nt->OptionalHeader.Magic != IMAGE_NT_OPTIONAL_HDR64_MAGIC)
        return false;

    const auto* section = IMAGE_FIRST_SECTION(nt);
    for (unsigned index = 0; index < nt->FileHeader.NumberOfSections;
         ++index, ++section) {
        if (!pw::signature::section_name_equals(*section, name)) continue;
        const std::size_t offset = section->PointerToRawData;
        const std::size_t size = section->SizeOfRawData;
        if (offset > file.size() || size > file.size() - offset) return false;
        *out = {file.data() + offset, size, section->VirtualAddress};
        return true;
    }
    return false;
}

std::int32_t read_i32(const std::uint8_t* bytes) {
    std::int32_t value{};
    std::memcpy(&value, bytes, sizeof(value));
    return value;
}

bool audit_unpacked_image(const char* path) {
    std::ifstream stream(path, std::ios::binary | std::ios::ate);
    if (!stream) {
        std::cerr << "Could not open signature-audit image: " << path << '\n';
        return false;
    }
    const std::streamsize file_size = stream.tellg();
    if (file_size <= 0) return false;
    std::vector<std::uint8_t> file(static_cast<std::size_t>(file_size));
    stream.seekg(0);
    if (!stream.read(reinterpret_cast<char*>(file.data()), file_size))
        return false;

    RawSection text{}, rdata{};
    if (!raw_section(file, ".text", &text) ||
        !raw_section(file, ".rdata", &rdata)) {
        return false;
    }
    const pw::signature::Region text_region{text.bytes, text.size,
                                             text.rva};
    const pw::signature::Region rdata_region{rdata.bytes, rdata.size,
                                              rdata.rva};
    const auto table = pw::signature::find_unique(
        rdata_region, pw::targets::kResolutionTable);
    const auto projection = pw::signature::find_unique(
        text_region, pw::targets::kProjectionTail);
    const auto viewport = pw::signature::find_unique(
        text_region, pw::targets::kViewportHook);
    const auto frame_start = pw::signature::find_unique(
        text_region, pw::targets::kFrameStart);
    const auto frame_marker = pw::signature::find_unique(
        text_region, pw::targets::kFrameMarker);
    const auto display = pw::signature::find_unique(
        text_region, pw::targets::kDisplayContext);
    const auto frustum = pw::signature::find_unique(
        text_region, pw::targets::kFrustumSeed);
    const std::array<std::pair<const char*, pw::signature::MatchResult>, 7>
        results{{
            {"resolution table", table},
            {"projection tail", projection},
            {"viewport hook", viewport},
            {"frame start", frame_start},
            {"frame marker", frame_marker},
            {"display context", display},
            {"visibility frustum", frustum},
        }};
    bool valid = true;
    for (const auto& entry : results) {
        const bool unique =
            entry.second.state == pw::signature::MatchState::unique;
        std::cout << path << ": " << entry.first << ": matches="
                  << entry.second.count;
        if (unique)
            std::cout << ", RVA=0x" << std::hex << entry.second.address
                      << std::dec;
        std::cout << '\n';
        valid &= unique;
    }
    if (!valid) return false;

    const auto call_target = [&](std::uintptr_t pattern_rva,
                                 std::size_t call_offset) {
        const std::size_t offset = pattern_rva - text.rva + call_offset;
        return pattern_rva + call_offset + 5 +
            read_i32(text.bytes + offset + 1);
    };
    const std::uintptr_t viewport_function =
        viewport.address - 0x13;
    return call_target(frame_start.address,
                       pw::targets::kFrameStartCallOffset) ==
               viewport_function &&
           call_target(frame_marker.address,
                       pw::targets::kFrameMarkerCallOffset) ==
               viewport_function;
}

}  // namespace

int main(int argc, char** argv) {
    using namespace pw::signature;

    const std::array<std::uint8_t, 12> sample{
        0x90, 0x48, 0x8b, 0x01, 0xe8, 0x11,
        0x22, 0x33, 0x44, 0xc3, 0x90, 0x90};
    const auto base = reinterpret_cast<std::uintptr_t>(sample.data());
    const Region region{sample.data(), sample.size(), base};

    const std::array<std::uint8_t, 4> exact_bytes{0x48, 0x8b, 0x01, 0xe8};
    const BytePattern exact{exact_bytes.data(), nullptr, exact_bytes.size()};
    const auto one = find_unique(region, exact);
    expect(one.state == MatchState::unique && one.address == base + 1,
           "an exact unique pattern must resolve its address");

    const std::array<std::uint8_t, 5> call_bytes{0xe8, 0, 0, 0, 0};
    const std::array<std::uint8_t, 5> call_mask{1, 0, 0, 0, 0};
    const BytePattern call{call_bytes.data(), call_mask.data(),
                           call_bytes.size()};
    expect(find_unique(region, call).state == MatchState::unique,
           "masked relocation bytes must not prevent a unique match");

    const std::array<std::uint8_t, 1> nop_bytes{0x90};
    const BytePattern nop{nop_bytes.data(), nullptr, nop_bytes.size()};
    const auto many = find_unique(region, nop);
    expect(many.state == MatchState::ambiguous && many.address == 0,
           "an ambiguous pattern must never select its first match");

    const std::array<std::uint8_t, 2> absent_bytes{0xcc, 0xcc};
    const BytePattern absent{absent_bytes.data(), nullptr,
                             absent_bytes.size()};
    expect(find_unique(region, absent).state == MatchState::none,
           "an absent pattern must remain unresolved");

    std::array<std::uint8_t, 8> relative{};
    relative[0] = 0xe8;
    const std::int32_t displacement = 0x1234;
    std::memcpy(relative.data() + 1, &displacement, sizeof(displacement));
    std::uintptr_t target = 0;
    const auto instruction =
        reinterpret_cast<std::uintptr_t>(relative.data());
    expect(decode_relative_target(instruction, 1, 5, &target) &&
               target == instruction + 5 + displacement,
           "relative targets must use the end of the instruction");

    expect(mapped_image_section(
               reinterpret_cast<std::uintptr_t>(GetModuleHandleW(nullptr)),
               ".text").valid(),
           "the resolver must locate the current process .text section");

    // Optional maintainer mode. SteamStub encrypts the retail .text on disk,
    // so this accepts only an unpacked copy obtained from the maintainer's own
    // installation. Such executables are never committed or distributed.
    for (int index = 1; index < argc; ++index)
        expect(audit_unpacked_image(argv[index]),
               "every locator must be unique and relationally consistent");

    if (failures) return 1;
    std::cout << "signature resolver checks passed\n";
    return 0;
}
