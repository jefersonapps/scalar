#pragma once
#include <cstdint>
#include <string>
#include <vector>
#include <span>
namespace scalar {
using Bytes=std::vector<std::uint8_t>;
struct ArchiveResult { Bytes json; std::string error; explicit operator bool() const {return error.empty();} };
// The inspected payload borrows its storage from the supplied archive.
struct ArchiveView { std::span<const std::uint8_t> json; std::string error; explicit operator bool() const {return error.empty();} };
// Restricted valid ZIP profile: one stored project.json; refuses encryption/compression.
Bytes packBoard(std::span<const std::uint8_t> json);
ArchiveResult unpackBoard(std::span<const std::uint8_t> zip);
ArchiveView inspectBoard(std::span<const std::uint8_t> zip);
}
