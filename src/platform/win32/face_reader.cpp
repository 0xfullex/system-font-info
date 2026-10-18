#include "face_reader.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <cmath>
#include <iomanip>
#include <sstream>
#include <stdexcept>

#include "../../core/platform.h"

namespace sfi::platform::win32 {
namespace {

uint32_t directWriteTag(uint32_t tag) {
  return (tag >> 24) | ((tag >> 8) & 0x0000ff00) | ((tag << 8) & 0x00ff0000) | (tag << 24);
}

std::optional<std::vector<uint8_t>> copyTable(IDWriteFontFace* face, uint32_t tag) {
  const void* data = nullptr;
  void* context = nullptr;
  UINT32 size = 0;
  BOOL exists = FALSE;
  const HRESULT status = face->TryGetFontTable(directWriteTag(tag), &data, &size, &context, &exists);
  // The directory establishes presence. A failed fetch is handled as that table's parse failure by the reader,
  // and only selected entries expose it in opentype.errors.
  if (FAILED(status)) return std::nullopt;
  if (!exists) return std::nullopt;
  const auto* bytes = static_cast<const uint8_t*>(data);
  std::vector<uint8_t> result(bytes, bytes + size);
  face->ReleaseFontTable(context);
  return result;
}

std::string lowerAscii(std::string text) {
  for (char& character : text) {
    if (character >= 'A' && character <= 'Z') character = static_cast<char>(character - 'A' + 'a');
  }
  return text;
}

Value additionalStrings(IDWriteFont* font, const sfnt::FontTables& tables) {
  Value result = Value::object();
  for (int id = 1; id <= DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG; ++id) {
    LocalizedStrings additional;
    for (const auto& item : informationalStrings(font, static_cast<DWRITE_INFORMATIONAL_STRING_ID>(id))) {
      const bool reproduced = std::any_of(tables.names.begin(), tables.names.end(), [&](const sfnt::NameRecord& name) {
        return name.value && *name.value == item.value && lowerAscii(name.language) == lowerAscii(item.language);
      });
      if (!reproduced) additional.push_back(item);
    }
    if (!additional.empty()) result.set(std::to_string(id), localizedValue(additional));
  }
  return result;
}

std::vector<std::string> supportedLanguages(IDWriteFont* font) {
  std::vector<std::string> result;
  for (const auto& item : informationalStrings(font, DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG)) {
    std::string separated = item.value;
    for (char& character : separated) {
      if (character == ';' || character == ',') character = ' ';
    }
    std::istringstream words(separated);
    std::string tag;
    while (words >> tag) {
      if (tag.size() == 4 && std::all_of(tag.begin(), tag.end(), [](unsigned char c) { return std::isalpha(c); }))
        tag = "und-" + tag;
      result.push_back(std::move(tag));
    }
  }
  return result;
}

Format formatOf(const sfnt::FontTables& tables, DWRITE_FONT_FACE_TYPE type) {
  const auto has = [&](uint32_t tag) { return std::binary_search(tables.tags.begin(), tables.tags.end(), tag); };
  if (has(sfnt::makeTag("CFF2"))) return Format::Cff2;
  if (has(sfnt::makeTag("CFF "))) return Format::Cff;
  if (has(sfnt::makeTag("glyf"))) return Format::TrueType;
  if (type == DWRITE_FONT_FACE_TYPE_TYPE1) return Format::Type1;
  if (type == DWRITE_FONT_FACE_TYPE_BITMAP) return Format::Bitmap;
  return Format::Unknown;
}

std::optional<uint32_t> namedInstance(const Face& face, const sfnt::FontTables& tables) {
  if (!tables.fvar || face.coordinates.empty()) return std::nullopt;
  for (size_t index = 0; index < tables.fvar->instances.size(); ++index) {
    const auto& instance = tables.fvar->instances[index];
    bool matches = instance.coordinates.size() == face.coordinates.size();
    for (size_t axis = 0; matches && axis < face.coordinates.size(); ++axis)
      matches = std::abs(instance.coordinates[axis] - face.coordinates[axis].second) <= 1.0 / 65536;
    if (matches) return static_cast<uint32_t>(index);
  }
  return std::nullopt;
}

void appendString(std::ostream& output, const std::string& text) { output << text.size() << ':' << text; }

// These are all the per-member inputs of public B fields. For an identical physical resource, coordinates, and
// simulations, C/D/E inputs are shared; any different member metadata must fail instead of being silently dropped.
std::string memberMetadata(IDWriteFont* font) {
  std::ostringstream result;
  result << font->GetWeight() << ',' << font->GetStretch() << ',' << font->GetStyle() << ',' << font->IsSymbolFont();
  ComPtr<IDWriteFont1> font1;
  check(font->QueryInterface(IID_PPV_ARGS(&font1)), "IDWriteFont1");
  result << ',' << font1->IsMonospacedFont();
  ComPtr<IDWriteFont2> font2;
  check(font->QueryInterface(IID_PPV_ARGS(&font2)), "IDWriteFont2");
  result << ',' << font2->IsColorFont();
  const auto appendNames = [&](const LocalizedStrings& strings) {
    result << '[' << strings.size() << ']';
    for (const auto& item : strings) {
      appendString(result, item.language);
      appendString(result, item.value);
    }
  };
  ComPtr<IDWriteLocalizedStrings> names;
  check(font->GetFaceNames(&names), "GetFaceNames");
  appendNames(localizedStrings(names.Get()));
  for (int id = 1; id <= DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG; ++id)
    appendNames(informationalStrings(font, static_cast<DWRITE_INFORMATIONAL_STRING_ID>(id)));
  const Reference reference = fontReference(font);
  for (const auto& value : axisValues(reference.reference.Get()))
    result << ',' << static_cast<uint32_t>(value.axisTag) << '=' << std::hexfloat << value.value;
  return result.str();
}

}  // namespace

FaceReader::FaceReader(const Request& request, WorkLog& work, Collection& collection)
    : request_(request), work_(work), collection_(collection) {}

FaceReader::Resource& FaceReader::resource(const Reference& reference) { return resources_[reference.key]; }

IDWriteFontFace3* FaceReader::openFace(Resource& resource, const Reference& reference) {
  if (!resource.face) {
    check(reference.reference->CreateFontFace(&resource.face), "CreateFontFace");
    ++work_.fontsOpened;
  }
  return resource.face.Get();
}

const sfnt::FontTables& FaceReader::tables(Resource& resource, const Reference& reference) {
  if (resource.tables) return *resource.tables;
  const auto face = openFace(resource, reference);
  std::vector<uint32_t> tags;
  if (!reference.path.empty()) {
    const auto [entry, inserted] = files_.try_emplace(reference.path);
    if (inserted) {
      entry->second = sfnt::FontFile::open(reference.path);
      ++work_.fontsOpened;
    }
    if (entry->second) tags = entry->second->tableTags(reference.key.second);
  }
  const sfnt::TableFetcher fetch = [face](uint32_t tag) { return copyTable(face, tag); };
  resource.tables = sfnt::readFontTables(tags, fetch, decodeLegacyText);
  if (request_.wantsAnyOpenType() && !tags.empty()) {
    resource.opentype = sfnt::openTypeValue(*resource.tables, request_.openTypeSelection, fetch);
    work_.tableEntries += static_cast<uint32_t>(request_.openTypeSelection.entries.count());
  }
  return *resource.tables;
}

SharedAxes FaceReader::axes(Resource& resource, const Reference& reference) {
  const auto& parsed = tables(resource, reference);
  if (!resource.axes && parsed.fvar && !parsed.fvar->axes.empty()) {
    auto axes = std::make_shared<std::vector<VariationAxis>>();
    for (const auto& stored : parsed.fvar->axes) {
      axes->push_back({sfnt::tagToString(stored.tag), sfnt::localizedName(parsed.names, stored.nameId), stored.min,
                       stored.defaultValue, stored.max, bool(stored.flags & 1)});
    }
    resource.axes = axes;
  }
  return resource.axes;
}

const Value& FaceReader::resourceAxes(Resource& resource, const Reference& reference) {
  if (resource.resourceAxes) return *resource.resourceAxes;
  check(collection_.factory()->CreateFontResource(reference.file.Get(), reference.key.second, &resource.resource),
        "CreateFontResource");
  ++work_.fontsOpened;
  const UINT32 count = resource.resource->GetFontAxisCount();
  std::vector<DWRITE_FONT_AXIS_RANGE> ranges(count);
  std::vector<DWRITE_FONT_AXIS_VALUE> defaults(count);
  check(resource.resource->GetFontAxisRanges(ranges.data(), count), "GetFontAxisRanges(resource)");
  check(resource.resource->GetDefaultFontAxisValues(defaults.data(), count), "GetDefaultFontAxisValues");
  Value result = Value::array();
  for (UINT32 index = 0; index < count; ++index) {
    ComPtr<IDWriteLocalizedStrings> names;
    check(resource.resource->GetAxisNames(index, &names), "GetAxisNames");
    result.push(Value::object()
                    .set("tag", axisTag(ranges[index].axisTag))
                    .set("min", ranges[index].minValue)
                    .set("max", ranges[index].maxValue)
                    .set("default", defaults[index].value)
                    .set("attributes", static_cast<uint32_t>(resource.resource->GetFontAxisAttributes(index)))
                    .set("names", localizedValue(localizedStrings(names.Get()))));
  }
  resource.resourceAxes = std::move(result);
  return *resource.resourceAxes;
}

ReadFace FaceReader::read(IDWriteFont* font, const LocalizedStrings& familyNames, bool inactive) {
  ReadFace read;
  Face& face = read.face;
  const Reference reference = fontReference(font);
  Resource& shared = resource(reference);
  const auto wanted = [&](Field field) { return request_.wants(field); };
  const auto simulations = font->GetSimulations();
  face.simulatedBold = (simulations & DWRITE_FONT_SIMULATIONS_BOLD) != 0;
  face.simulatedOblique = (simulations & DWRITE_FONT_SIMULATIONS_OBLIQUE) != 0;
  if (wanted(Field::FacesFile) && !reference.path.empty())
    face.file = FileLocation{reference.path, reference.key.second};
  if (wanted(Field::FacesFamilyNames)) face.familyNames = familyNames;
  if (wanted(Field::FacesPostscriptName))
    face.postscriptName = preferredString(informationalStrings(font, DWRITE_INFORMATIONAL_STRING_POSTSCRIPT_NAME));
  if (wanted(Field::FacesFullNames)) face.fullNames = informationalStrings(font, DWRITE_INFORMATIONAL_STRING_FULL_NAME);
  if (wanted(Field::FacesStyleNames)) {
    ComPtr<IDWriteLocalizedStrings> names;
    check(font->GetFaceNames(&names), "GetFaceNames");
    face.styleNames = localizedStrings(names.Get());
  }
  if (wanted(Field::FacesWeight)) face.weight = font->GetWeight();
  if (wanted(Field::FacesWidth)) {
    constexpr std::array<double, 10> widths = {100, 50, 62.5, 75, 87.5, 100, 112.5, 125, 150, 200};
    face.width = widths.at(static_cast<size_t>(font->GetStretch()));
  }
  if (wanted(Field::FacesStyle)) {
    const auto style = font->GetStyle();
    face.style = style == DWRITE_FONT_STYLE_ITALIC    ? Style::Italic
                 : style == DWRITE_FONT_STYLE_OBLIQUE ? Style::Oblique
                                                      : Style::Normal;
  }
  if (wanted(Field::FacesMonospace)) {
    ComPtr<IDWriteFont1> font1;
    check(font->QueryInterface(IID_PPV_ARGS(&font1)), "IDWriteFont1");
    face.monospace = font1->IsMonospacedFont() != FALSE;
  }
  if (wanted(Field::FacesColor)) {
    ComPtr<IDWriteFont2> font2;
    check(font->QueryInterface(IID_PPV_ARGS(&font2)), "IDWriteFont2");
    face.color = font2->IsColorFont() != FALSE;
  }
  if (wanted(Field::FacesSymbol)) face.symbol = font->IsSymbolFont() != FALSE;
  if (wanted(Field::FacesSupportedLanguages)) face.supportedLanguages = supportedLanguages(font);

  Source source;
  if (wanted(Field::FacesScope) || wanted(Field::FacesPlatformSourceType) || wanted(Field::FacesPlatformSourceName)) {
    source = collection_.source(reference.key);
    if (collection_.processFont(reference.path))
      face.scope = Scope::Process;
    else if (source.type == DWRITE_FONT_SOURCE_TYPE_PER_MACHINE)
      face.scope = Scope::System;
    else if (source.type == DWRITE_FONT_SOURCE_TYPE_PER_USER)
      face.scope = Scope::User;
  }

  const bool needsVariation = wanted(Field::FacesVariable) || wanted(Field::FacesAxes) ||
                              wanted(Field::FacesCoordinates) || wanted(Field::FacesNamedInstanceIndex);
  const bool variable = needsVariation && collection_.variable(reference.key);
  face.variable = variable;
  std::vector<DWRITE_FONT_AXIS_VALUE> values;
  if (variable || wanted(Field::FacesPlatformAxisValues)) values = axisValues(reference.reference.Get());
  if (variable &&
      (wanted(Field::FacesAxes) || wanted(Field::FacesCoordinates) || wanted(Field::FacesNamedInstanceIndex))) {
    face.axes = axes(shared, reference);
    if (!face.axes)
      throw std::runtime_error("system-font-info: variable resource has no readable fvar axes: " + reference.path);
    for (const auto& axis : *face.axes) {
      const auto found = std::find_if(values.begin(), values.end(),
                                      [&](const auto& value) { return axisTag(value.axisTag) == axis.tag; });
      face.coordinates.emplace_back(axis.tag, found == values.end() ? axis.defaultValue : double(found->value));
    }
    if (wanted(Field::FacesNamedInstanceIndex))
      face.namedInstanceIndex = namedInstance(face, tables(shared, reference));
  }

  if (wanted(Field::FacesFormat))
    face.format = formatOf(tables(shared, reference), openFace(shared, reference)->GetType());
  if (request_.wantsAnyOpenType()) {
    tables(shared, reference);
    face.opentype = shared.opentype;
  }
  if (wanted(Field::FacesCoverage)) {
    face.coverage = coverageOf(font);
    ++work_.characterSets;
  }

  if (request_.platformObject) {
    Value platform = Value::object().set("os", "win32");
    if (wanted(Field::FacesPlatformWeight)) platform.set("weight", static_cast<uint32_t>(font->GetWeight()));
    if (wanted(Field::FacesPlatformWidth)) platform.set("width", static_cast<uint32_t>(font->GetStretch()));
    if (wanted(Field::FacesPlatformSlant)) platform.set("slant", static_cast<uint32_t>(font->GetStyle()));
    if (wanted(Field::FacesPlatformFormat))
      platform.set("format", static_cast<uint32_t>(openFace(shared, reference)->GetType()));
    const std::array<std::pair<Field, DWRITE_INFORMATIONAL_STRING_ID>, 4> infos = {
        {{Field::FacesPlatformTypographicFamilyNames, DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_FAMILY_NAMES},
         {Field::FacesPlatformTypographicStyleNames, DWRITE_INFORMATIONAL_STRING_TYPOGRAPHIC_SUBFAMILY_NAMES},
         {Field::FacesPlatformDesignLanguages, DWRITE_INFORMATIONAL_STRING_DESIGN_SCRIPT_LANGUAGE_TAG},
         {Field::FacesPlatformSupportedLanguages, DWRITE_INFORMATIONAL_STRING_SUPPORTED_SCRIPT_LANGUAGE_TAG}}};
    constexpr std::array<const char*, 4> keys = {"typographicFamilyNames", "typographicStyleNames", "designLanguages",
                                                 "supportedLanguages"};
    for (size_t i = 0; i < infos.size(); ++i)
      if (wanted(infos[i].first)) platform.set(keys[i], localizedValue(informationalStrings(font, infos[i].second)));
    if (wanted(Field::FacesPlatformInactiveFamilyPreference)) platform.set("inactiveFamilyPreference", inactive);
    if (wanted(Field::FacesPlatformSourceType) && source.type) platform.set("sourceType", *source.type);
    if (wanted(Field::FacesPlatformSourceName) && source.name) platform.set("sourceName", *source.name);
    if (wanted(Field::FacesPlatformAxisValues)) {
      Value axes = Value::array();
      for (const auto& value : values)
        axes.push(Value::object().set("tag", axisTag(value.axisTag)).set("value", value.value));
      platform.set("axisValues", std::move(axes));
    }
    if (wanted(Field::FacesPlatformResourceAxes)) platform.set("resourceAxes", resourceAxes(shared, reference));
    if (wanted(Field::FacesPlatformAdditionalInformationalStrings))
      platform.set("additionalInformationalStrings", additionalStrings(font, tables(shared, reference)));
    face.platform = std::move(platform);
  }

  std::ostringstream identity;
  appendString(identity, reference.key.first);
  identity << ':' << reference.key.second << ':' << static_cast<uint32_t>(simulations);
  for (const auto& [tag, value] : face.coordinates) identity << ':' << tag << '=' << std::hexfloat << value;
  // A derived family field can need members without requesting identity. Its hidden intermediate faces still keep
  // distinct instances without opening fvar just to identify them.
  if (!wanted(Field::FacesCoordinates))
    for (const auto& value : axisValues(reference.reference.Get()))
      identity << ':' << axisTag(value.axisTag) << '=' << std::hexfloat << value.value;
  read.identity = identity.str();
  return read;
}

bool FaceReader::sameMember(IDWriteFont* left, IDWriteFont* right) {
  return memberMetadata(left) == memberMetadata(right);
}

}  // namespace sfi::platform::win32
