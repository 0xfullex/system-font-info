#include <filesystem>
#include <fstream>

#include "reader.h"
#include "sfnt.h"

namespace sfi::sfnt {

namespace {

constexpr uint32_t kCollectionTag = makeTag("ttcf");
constexpr size_t kDirectoryHeaderSize = 12;
constexpr size_t kTableRecordSize = 16;

// `path` is UTF-8; going through char8_t makes std::filesystem convert it to
// the native encoding (UTF-16 on Windows).
std::filesystem::path pathFromUtf8(const std::string& path) {
  return std::filesystem::path(std::u8string(reinterpret_cast<const char8_t*>(path.data()), path.size()));
}

bool isSfntVersion(uint32_t version) {
  return version == 0x00010000 || version == makeTag("OTTO") || version == makeTag("true") || version == makeTag("typ1");
}

std::optional<std::vector<uint8_t>> readRange(std::ifstream& file, uint64_t offset, uint64_t length) {
  std::vector<uint8_t> bytes(length);
  file.clear();
  file.seekg(std::streamoff(offset));
  if (length > 0 && !file.read(reinterpret_cast<char*>(bytes.data()), std::streamsize(length))) return std::nullopt;
  return bytes;
}

}  // namespace

std::optional<FontFile> FontFile::open(const std::string& path) {
  std::error_code ec;
  const std::filesystem::path fsPath = pathFromUtf8(path);
  const uint64_t fileSize = std::filesystem::file_size(fsPath, ec);
  if (ec) return std::nullopt;
  std::ifstream file(fsPath, std::ios::binary);
  if (!file) return std::nullopt;

  // Reads the table directory of the face whose offset table starts at `offset`.
  const auto readFace = [&](uint64_t offset) -> std::optional<std::vector<TableRecord>> {
    const auto header = readRange(file, offset, kDirectoryHeaderSize);
    if (!header) return std::nullopt;
    const Reader headerReader(*header);
    if (!isSfntVersion(headerReader.u32(0))) return std::nullopt;
    const size_t tableCount = headerReader.u16(4);
    const auto directory = readRange(file, offset + kDirectoryHeaderSize, tableCount * kTableRecordSize);
    if (!directory) return std::nullopt;
    const Reader reader(*directory);
    std::vector<TableRecord> records;
    for (size_t i = 0; i < tableCount; ++i) {
      const size_t at = i * kTableRecordSize;
      records.push_back({reader.u32(at), reader.u32(at + 8), reader.u32(at + 12)});
    }
    return records;
  };

  const auto start = readRange(file, 0, 12);
  if (!start) return std::nullopt;
  const Reader startReader(*start);

  FontFile font;
  font.path_ = path;
  if (startReader.u32(0) == kCollectionTag) {
    const uint64_t faceCount = startReader.u32(8);
    if (faceCount == 0 || faceCount > fileSize / 4) return std::nullopt;
    const auto offsets = readRange(file, 12, faceCount * 4);
    if (!offsets) return std::nullopt;
    const Reader offsetReader(*offsets);
    for (uint64_t i = 0; i < faceCount; ++i) {
      // A face that cannot be read keeps its index, with no tables.
      font.faces_.push_back(readFace(offsetReader.u32(i * 4)).value_or(std::vector<TableRecord>()));
    }
  } else {
    auto face = readFace(0);
    if (!face) return std::nullopt;
    font.faces_.push_back(std::move(*face));
  }
  return font;
}

size_t FontFile::faceCount() const { return faces_.size(); }

std::vector<uint32_t> FontFile::tableTags(size_t faceIndex) const {
  std::vector<uint32_t> tags;
  if (faceIndex >= faces_.size()) return tags;
  for (const TableRecord& record : faces_[faceIndex]) tags.push_back(record.tag);
  return tags;
}

std::optional<std::vector<uint8_t>> FontFile::table(size_t faceIndex, uint32_t tag) const {
  if (faceIndex >= faces_.size()) return std::nullopt;
  for (const TableRecord& record : faces_[faceIndex]) {
    if (record.tag != tag) continue;
    std::ifstream file(pathFromUtf8(path_), std::ios::binary);
    if (!file) return std::nullopt;
    file.seekg(0, std::ios::end);
    const uint64_t fileSize = uint64_t(file.tellg());
    if (uint64_t(record.offset) + record.length > fileSize) return std::nullopt;
    return readRange(file, record.offset, record.length);
  }
  return std::nullopt;
}

}  // namespace sfi::sfnt
