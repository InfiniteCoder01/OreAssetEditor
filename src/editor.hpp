#include "common.hpp"
#include <shellapi.h>

extern std::string status, projectSaveDirectory;

namespace TiledLevel {
struct Object;
}

namespace Textures {
struct Atlas {
  vec2i tilesize;
  std::string name;
  MvImage image;
  struct Tileset {
    vector<vec2i> patches;
    bool* colliders = nullptr;

    vec2i patch(vec2i tile) {
      for (const auto& patch : patches) {
        if (inRangeW(tile.x, patch.x, 4) && tile.y == patch.y) return patch;
      }
      return -1;
    }

    bool inPatch(vec2i tile) { return patch(tile) != -1; }

    void removePatch(vec2i tile) {
      for (int i = 0; i < patches.size(); i++) {
        if (inRangeW(tile.x, patches[i].x, 4) && tile.y == patches[i].y) {
          patches.erase(patches.begin() + i);
          break;
        }
      }
    }
  }* tileset = nullptr;

  Atlas(const std::string& filename, vec2i tilesize) : tilesize(tilesize), image(filename) {}
  Atlas(const std::string& name) : name(name), image(projectSaveDirectory + "atlases/" + name + ".png") {
    File file(projectSaveDirectory + "atlases/" + name + ".atl", "rb");
    fread((void*)&tilesize, sizeof(tilesize), 1, file());
    if (fgetn<bool>(file())) {
      tileset = new Tileset();
      int nPatches = fgetn<uint16_t>(file());
      tileset->patches.resize(nPatches);
      if (nPatches) fread((void*)&tileset->patches[0], sizeof(tileset->patches[0]), nPatches, file());
      if (fgetn<bool>(file())) {
        tileset->colliders = new bool[width() * height()];
        uint32_t w, h;
        readMetadata(file(), "%32i %32i", &w, &h);
        bool* readColliders = new bool[w * h];
        fread((void*)readColliders, sizeof(tileset->colliders[0]), w * h, file());
        for (int x = 0; x < min(w, width()); x++) {
          for (int y = 0; y < min(h, height()); y++) {
            tileset->colliders[x + y * width()] = readColliders[x + y * w];
          }
        }
        delete[] readColliders;
      }
    }
  }

  ~Atlas() {
    if (tileset) {
    if (tileset->colliders) delete[] tileset->colliders;
      delete tileset;
    }
  }

  void save() {
    image.save(projectSaveDirectory + "atlases/" + name + ".png");
    File file(projectSaveDirectory + "atlases/" + name + ".atl", "wb+");
    fwrite((void*)&tilesize, sizeof(tilesize), 1, file());
    fputn<bool>(file(), tileset != nullptr);
    if (tileset) {
      fputn<uint16_t>(file(), tileset->patches.size());
      if (!tileset->patches.empty()) fwrite((void*)&tileset->patches[0], sizeof(tileset->patches[0]), tileset->patches.size(), file());
      fputn<bool>(file(), tileset->colliders != nullptr);
      if (tileset->colliders) {
        writeMetadata(file(), "%32i %32i", width(), height());
        fwrite((void*)tileset->colliders, sizeof(tileset->colliders[0]), width() * height(), file());
      }
    }
  }

  int width() const { return image.width / tilesize.x; }
  int height() const { return image.height / tilesize.y; }
  vec2i size() const { return vec2i(width(), height()); }
  int toIndex(vec2i tile) const { return tile.x + tile.y * width(); }
};

extern bool showAtlas, showObjects, showInspector;
extern vec2i selected;
extern vector<Atlas*> atlases;
extern Atlas* atlas;

Atlas* atlasByName(const std::string& name);
void chooseAtlas(const std::string& label, Atlas*& atlas, int tilesetness = -1);
void importAtlas(const std::string& filename);
void atlasSettings();

enum class PropertyType : uint8_t { INT, FLOAT, STRING };
const std::string propertyTypes[] = {"int", "float", "String"};

struct ObjectClass {
  Textures::Atlas* atlas;
  std::string name;
  fs::path path;

  struct Property {
    std::string name, defaultValue;
    PropertyType type = PropertyType::INT;
  };

  std::vector<Property> properties;
  std::vector<TiledLevel::Object*> children;

  ObjectClass(const std::string& name, const fs::path& path, Atlas* atlas) : name(name), path(path), atlas(atlas) {}
  ObjectClass(const std::string& path) : name(path.substr(path.find_last_of("/\\") + 1, path.size() - path.find_last_of("/\\") - 5)), path(path) {
    File file(path, "rb");
    std::string atlasName = freadstr(file());
    atlas = atlasByName(atlasName);
    uint32_t nProps = fgetn<uint32_t>(file());
    properties.resize(nProps);
    for (auto& property : properties) {
      readMetadata(file(), "%s, %s, %8i", &property.name, &property.defaultValue, &property.type);
    }
  }

  ~ObjectClass() { save(); }
  void save() {  //
    File file(path.string(), "wb+");
    fwritestr(file(), atlas->name.c_str());
    fputn<uint32_t>(file(), properties.size());
    for (const auto& property : properties) {
      writeMetadata(file(), "%s, %s, %8i", property.name.c_str(), property.defaultValue.c_str(), property.type);
    }
  }
};
extern std::vector<ObjectClass*> objects;
extern ObjectClass* object;

void save();
void load();
void exportData();
void windows();
}  // namespace Textures

namespace TiledLevel {
struct Object {
  vec2i pos;
  Textures::ObjectClass* parent;
  std::vector<std::string> properties;

  Object(){};
  Object(Textures::ObjectClass* parent, vec2i pos) : pos(pos), parent(parent) {
    parent->children.push_back(this);
    properties.reserve(parent->properties.size());
    for (const auto& prop : parent->properties) {
      properties.push_back(prop.defaultValue);
    }
  }

  ~Object() { parent->children.erase(std::remove(parent->children.begin(), parent->children.end(), this), parent->children.end()); }
};

struct Level {
  Textures::Atlas* tileset;
  uint32_t width, height;
  std::string name;
  vec2i* data;
  std::vector<Object> objects;

  Level(const std::string& name) : name(name) {
    File file(projectSaveDirectory + "levels/" + name + ".lvl", "rb");
    std::string tilesetName;
    readMetadata(file(), "%32i, %32i, %s", &width, &height, &tilesetName);
    tileset = Textures::atlasByName(tilesetName);
    data = new vec2i[width * height];
    fread(data, sizeof(vec2i) * width * height, 1, file());
    uint16_t nObjects = fgetn<uint16_t>(file());
    objects.resize(nObjects);
    for (auto& object : objects) {
      std::string parentName;
      uint16_t nProperties;
      readMetadata(file(), "%32i %32i %s %16i", &object.pos.x, &object.pos.y, &parentName, &nProperties);
      for (auto& parent : Textures::objects) {
        if (parent->name == parentName) {
          object.parent = parent;
          parent->children.push_back(&object);
          break;
        }
      }
      object.properties.resize(nProperties);
      for (auto& property : object.properties) {
        property = freadstr(file());
      }
    }
  }

  Level(const std::string& name, Textures::Atlas* tileset, uint32_t width, uint32_t height) : tileset(tileset), width(width), height(height), name(name) {
    data = new vec2i[width * height];
    std::fill(data, data + width * height, -1);
  }

  ~Level() {
    if (data) delete[] data;
  }

  void resize(vec2i size) {
    if (this->size() == size) return;
    vec2i* data_ = new vec2i[size.x * size.y];
    std::fill(data_, data_ + size.x * size.y, -1);

    for (int x = 0; x < min(width, size.x); x++) {
      for (int y = 0; y < min(height, size.y); y++) {
        data_[x + (size.y - y - 1) * size.x] = getTile(vec2i(x, height - y - 1));
      }
    }

    delete[] data;
    data = data_;
    width = size.x, height = size.y;
  }

  vec2i getTile(vec2i pos) { return data[pos.x + pos.y * width]; }
  void setTile(vec2i pos, vec2i tile) { data[pos.x + pos.y * width] = tile; }
  vec2i size() { return vec2i(width, height); }

  void save() {  //
    File file(projectSaveDirectory + "levels/" + name + ".lvl", "wb+");
    writeMetadata(file(), "%32i %32i %s %b %16i", width, height, tileset->name.c_str(), data, sizeof(vec2i) * width * height, objects.size());
    for (const auto& object : objects) {
      writeMetadata(file(), "%32i %32i %s %16i", object.pos.x, object.pos.y, object.parent->name.c_str(), object.properties.size());
      for (const auto& property : object.properties) {
        fwritestr(file(), property);
      }
    }
  }
};

extern bool showEditor;
extern vector<Level*> levels;
extern Level* level;
extern Object* object;

void newLevel();
void levelSettings();
void save();
void load();
void exportData();
void windows();
}  // namespace TiledLevel

static void loadProject(const std::string& folder) {
  projectSaveDirectory = folder;
  Textures::load();
  TiledLevel::load();
}

static void saveProject(const std::string& folder = "") {
  if (!folder.empty()) projectSaveDirectory = folder;
  Textures::save();
  TiledLevel::save();
}

static void openProjectsFolder() { ShellExecute(NULL, NULL, projectSaveDirectory.c_str(), NULL, NULL, SW_SHOWNORMAL); }
