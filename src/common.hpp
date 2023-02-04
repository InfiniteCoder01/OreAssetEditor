#pragma once
#include <vector>
#include <sstream>
#include <utility>
#include <algorithm>
#include <filesystem>
#include <mova.h>
#include <lib/logassert.h>
#include <imgui_internal.h>
#include <nfd.h>
#include <glUtil.hpp>
#include <nvwa/debug_new.h>

using namespace Math;
using namespace VectorMath;
using std::vector;
namespace fs = std::filesystem;

inline vec2f oreVec(ImVec2 vec) { return vec2f(vec.x, vec.y); }
inline ImVec2 imVec(vec2f vec) { return ImVec2(vec.x, vec.y); }
inline ImVec4 imVec(MvColor color) { return ImVec4(color.r / 255.f, color.g / 255.f, color.b / 255.f, color.a / 255.f); }
inline MvColor mvColor(ImVec4 color) { return MvColor(color.x * 255, color.y * 255, color.z * 255, color.w * 255); }
inline ImTextureID imID(MvImage& image) { return (ImTextureID)(intptr_t)image.asTexture(MvRendererType::OpenGL); }

inline vec2i availableRegion() { return oreVec(ImGui::GetContentRegionAvail()); }
inline vec2i viewportPos() { return oreVec(ImGui::GetCursorStartPos()) + oreVec(ImGui::GetWindowPos()); }
inline vec2i mousePos() { return oreVec(ImGui::GetMousePos()) - viewportPos(); }
inline const std::string& keepOldIfEmpty(const std::string& old, const std::string& news) { return news.empty() ? old : news; }
inline uint16_t rgb565(MvColor color) {
  if (color.a < 128) return 0xf81f;
  return (color.r >> 3 << 11) | (color.g >> 2 << 5) | color.b >> 3;
}

inline MvColor rgb565(uint16_t color) {
  if (color == 0xf81f) return MvColor::alpha;
  return MvColor((color & 0xf800) >> 8, (color & 0x07e0) >> 3, (color & 0x001f) << 3);
}

inline bool strcmp(const std::string& a, const std::string& b) {
  if (isdigit(a[0])) return isdigit(b[0]) ? (std::stoi(a.substr(0, a.find_first_not_of("0123456789"))) < std::stoi(b.substr(0, b.find_first_not_of("0123456789")))) : true;
  return a < b;
}

inline std::string openFile() {
  nfdchar_t* outPath = NULL;
  nfdresult_t result = NFD_OpenDialog(NULL, NULL, &outPath);

  if (result == NFD_OKAY) {
    std::string path = outPath;
    free(outPath);
    return path;
  } else if (result != NFD_CANCEL) MV_ERR("%s", NFD_GetError());
  return "";
}

inline std::string openDir() {
  nfdchar_t* outPath = NULL;
  nfdresult_t result = NFD_PickFolder(NULL, &outPath);

  if (result == NFD_OKAY) {
    std::string path = outPath;
    free(outPath);
    return path;
  } else if (result != NFD_CANCEL) MV_ERR("%s", NFD_GetError());
  return "";
}

struct File {
  FILE* ptr;
  File(const std::string& path, const std::string& mode) : ptr(fopen(path.c_str(), mode.c_str())) {}
  ~File() { fclose(ptr); }
  FILE* operator()() { return ptr; }
};

template <typename T> inline void fputn(FILE* file, T value) { fwrite((void*)&value, sizeof(value), 1, file); }
template <typename T> inline T fgetn(FILE* file) {
  T value;
  fread((void*)&value, sizeof(value), 1, file);
  return value;
}

static std::string freadstr(FILE* file) {
  std::string str;
  while (true) {
    char c = fgetc(file);
    if (c == EOF || c == '\0') return str;
    str += c;
  }
  return str;
}

static void fwritestr(FILE* file, const std::string& str) { fwrite(str.c_str(), str.size() + 1, 1, file); }

static void writeMetadata(FILE* file, const char* format, ...) {
  va_list argp;
  va_start(argp, format);
  while (*format != '\0') {
    if (strncmp(format, "%8i", 3) == 0) fputn<uint8_t>(file, va_arg(argp, int)), format += 3;
    else if (strncmp(format, "%16i", 4) == 0) fputn<uint16_t>(file, va_arg(argp, int)), format += 4;
    else if (strncmp(format, "%32i", 4) == 0) fputn<uint32_t>(file, va_arg(argp, int)), format += 4;
    else if (strncmp(format, "%64i", 4) == 0) fputn<uint64_t>(file, va_arg(argp, int)), format += 4;
    else if (strncmp(format, "%s", 2) == 0) fwritestr(file, va_arg(argp, const char*)), format += 2;
    else if (strncmp(format, "%b", 2) == 0) {
      uint8_t* data = va_arg(argp, uint8_t*);
      uint32_t size = va_arg(argp, uint32_t);
      fwrite(data, size, 1, file);
      format += 2;
    } else format++;
  }
  va_end(argp);
}

static void readMetadata(FILE* file, const char* format, ...) {
  va_list argp;
  va_start(argp, format);
  while (*format != '\0') {
    if (strncmp(format, "%8i", 3) == 0) *va_arg(argp, uint8_t*) = fgetn<uint8_t>(file), format += 3;
    else if (strncmp(format, "%16i", 4) == 0) *va_arg(argp, uint16_t*) = fgetn<uint16_t>(file), format += 4;
    else if (strncmp(format, "%32i", 4) == 0) *va_arg(argp, uint32_t*) = fgetn<uint32_t>(file), format += 4;
    else if (strncmp(format, "%64i", 4) == 0) *va_arg(argp, uint64_t*) = fgetn<uint64_t>(file), format += 4;
    else if (strncmp(format, "%s", 2) == 0) *va_arg(argp, std::string*) = freadstr(file), format += 2;
    else if (strncmp(format, "%b", 2) == 0) {
      uint8_t* data = va_arg(argp, uint8_t*);
      uint32_t size = va_arg(argp, uint32_t);
      fread(data, size, 1, file);
      format += 2;
    } else format++;
  }
  va_end(argp);
}

template <typename... Args> inline void writeMetadata(const std::string& filename, const char* format, Args... args) {
  File file(filename, "wb+");
  writeMetadata(file(), format, args...);
}

template <typename... Args> inline void readMetadata(const std::string& filename, const char* format, Args... args) {
  File file(filename, "rb");
  readMetadata(file(), format, args...);
}

template <typename... Args> static std::string format(const std::string& format, Args... args) {
  int size_s = std::snprintf(nullptr, 0, format.c_str(), args...) + 1;  // Extra space for '\0'
  if (size_s <= 0) {
    throw std::runtime_error("Error during formatting.");
  }
  auto size = static_cast<size_t>(size_s);
  std::unique_ptr<char[]> buf(new char[size]);
  std::snprintf(buf.get(), size, format.c_str(), args...);
  return std::string(buf.get(), buf.get() + size - 1);  // We don't want the '\0' inside
}

inline std::string itobytes(int n, int bytes) {
  std::string str;
  for (int i = 0; i < bytes; i++) str += format("%d, ", (n >> (i * 8)) & 0xff);
  return str;
}

namespace UI {
extern MvImage outline, transparentGrid, cursor;
namespace Tool {
extern MvImage brush, fillBucket, line, rect, colorPicker;
};

static bool formField(const std::string& label, char* buffer, size_t size) {
  ImGui::TextUnformatted(label.c_str());
  ImGui::SameLine();
  return ImGui::InputText(("##" + label).c_str(), buffer, size);
}

static bool formField(const std::string& label, std::string& str) {
  char buffer[256];
  strcpy(buffer, str.c_str());
  if (formField(label, buffer, sizeof(buffer))) {
    str = buffer;
    return true;
  }
  return false;
}

static bool formField(const std::string& label, vec2i& vec) {
  ImGui::TextUnformatted(label.c_str());
  ImGui::SameLine();
  return ImGui::InputInt2(("##" + label).c_str(), &vec.x);
}
};  // namespace UI

extern MvWindow* window;
