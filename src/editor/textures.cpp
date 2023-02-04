#include "common.hpp"
#include "editor.hpp"

namespace Textures {
static bool showAtlasSettingsPopup = false;
bool showAtlas = false, showObjects = false, showInspector = false;
vec2i selected = 0;
vector<Atlas*> atlases;
vector<ObjectClass*> objects;
Atlas* atlas;
ObjectClass* object;

Atlas* atlasByName(const std::string& name) {
  for (const auto atlas : atlases) {
    if (atlas->name == name) {
      return atlas;
    }
  }
  return atlases.empty() ? nullptr : atlases[0];
}

void chooseAtlas(const std::string& label, Atlas*& atlas, int tilesetness) {
  if (!atlas) atlas = atlases[0];
  ImGui::TextUnformatted(label.c_str());
  ImGui::SameLine();
  if (ImGui::BeginCombo("##AtlasInput", atlas->name.c_str())) {
    for (const auto item : atlases) {
      if (tilesetness != -1 && !item->tileset == tilesetness) continue;
      if (ImGui::Selectable(item->name.c_str(), item == atlas)) atlas = item;
      if (item == atlas) ImGui::SetItemDefaultFocus();
    }
    ImGui::EndCombo();
  }
}

void importAtlas(const std::string& filename) {
  if (filename.empty()) return;
  atlas = new Atlas(filename, 16);
  atlases.push_back(atlas);
  showAtlasSettingsPopup = true;
  selected = 0;
}

void atlasSettings() {
  showAtlasSettingsPopup = true;
  selected = 0;
}

static void atlasWindow() {
  if (!ImGui::Begin("Texture Atlas", &showAtlas)) return ImGui::End();

  if (atlas) {
    if (ImGui::BeginCombo("##AtlasSelector", atlas->name.c_str())) {
      for (int i = 0; i < atlases.size(); i++) {
        if (ImGui::Selectable(atlases[i]->name.c_str(), atlases[i] == atlas)) atlas = atlases[i];
        if (atlases[i] == atlas) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }

    vec2i viewportPos = oreVec(ImGui::GetCursorScreenPos());
    float scale = min(ImGui::GetContentRegionAvail().x / atlas->image.width, ImGui::GetContentRegionAvail().y / atlas->image.height);
    ImGui::Image(imID(atlas->image), imVec(atlas->image.size() * scale));
    if (atlas->tileset) {
      for (auto& patch : atlas->tileset->patches) {
        vec2i start = viewportPos + patch * atlas->tilesize * scale;
        ImGui::GetWindowDrawList()->AddRect(imVec(start), imVec(start + atlas->tilesize * vec2i(4, 1) * scale), MvColor::red.value, 0.f, 0, 2.f);
      }
      if (atlas->tileset->colliders) {
        for (int i = 0; i < atlas->width() * atlas->height(); i++) {
          if (atlas->tileset->colliders[i]) {
            ImGui::GetWindowDrawList()->AddCircleFilled(imVec(viewportPos + (vec2f(i % atlas->width(), i / atlas->width()) + 0.5f) * atlas->tilesize * scale), 5, MvColor::red.value);
          }
        }
      }
    }

    vec2i tileOnMouse = (vec2f)(oreVec(ImGui::GetMousePos()) - viewportPos) / atlas->tilesize / scale;
    if (ImGui::IsItemHovered() && (Mova::isMouseButtonHeld(MOUSE_LEFT) || Mova::isMouseButtonHeld(MOUSE_RIGHT))) {
      if (atlas->tileset && atlas->tileset->colliders) {
        if (Mova::isKeyHeld(MvKey::Ctrl)) {
          atlas->tileset->colliders[atlas->toIndex(tileOnMouse)] = Mova::isMouseButtonHeld(MOUSE_LEFT);
        }
      }
    }

    if (ImGui::IsItemClicked() || ImGui::IsItemHovered() && Mova::isMouseButtonPressed(MOUSE_RIGHT)) {
      selected = tileOnMouse;
      if (atlas->tileset) {
        if (atlas->tileset->inPatch(selected)) selected = atlas->tileset->patch(selected);
      }
      object = nullptr;
    }
    if (!Mova::isKeyHeld(MvKey::Ctrl) && ImGui::BeginPopupContextWindow()) {
      if (!atlas->tileset) {
        if (ImGui::MenuItem("Enable tileset")) atlas->tileset = new Atlas::Tileset();
      } else {
        if (ImGui::MenuItem("Disable tileset")) delete atlas->tileset, atlas->tileset = nullptr;
      }
      if (atlas->tileset) {
        if (!atlas->tileset->inPatch(selected)) {
          if (ImGui::MenuItem("Add patch")) atlas->tileset->patches.push_back(selected);
        } else {
          if (ImGui::MenuItem("Remove patch")) atlas->tileset->removePatch(selected);
        }
        if (!atlas->tileset->colliders) {
          if (ImGui::MenuItem("Enable colliders")) atlas->tileset->colliders = new bool[atlas->width() * atlas->height()], memset(atlas->tileset->colliders, 0, atlas->width() * atlas->height());
        } else {
          if (ImGui::MenuItem("Disable colliders")) delete[] atlas->tileset->colliders, atlas->tileset->colliders = nullptr;
        }
      }
      ImGui::EndPopup();
    }
    {
      vec2i start = viewportPos + selected * atlas->tilesize * scale;
      ImGui::GetWindowDrawList()->AddRect(imVec(start), imVec(start + atlas->tilesize * scale), MvColor::red.value, 0.f, 0, 3.f);
    }
  } else ImGui::TextUnformatted("\"File->Import texture atlas\" to import texture atlas!");
  ImGui::End();
}

static void objectsWindow() {
  static fs::path base;
  static fs::path path;
  static char tmpFolder[256];
  static bool creatingFolder = false;

  if (!ImGui::Begin("Objects", &showObjects)) return ImGui::End();
  if (projectSaveDirectory.empty()) {
    ImGui::TextUnformatted("Open Project to see the objects");
    return ImGui::End();
  }

  if (base.empty()) path = base = projectSaveDirectory + "objects";
  if (path != base) {
    if (ImGui::Button("..")) path = path.parent_path();
  }
  for (const auto& p : fs::directory_iterator(path)) {
    if (!p.is_directory()) continue;
    if (ImGui::Button(p.path().stem().string().c_str())) path = p.path();
  }
  for (const auto& obj : objects) {
    if (fs::path(obj->path).parent_path() != path) continue;
    if (ImGui::Button(obj->name.c_str())) object = obj, TiledLevel::object = nullptr;
  }

  if (ImGui::BeginPopupModal("Create Object", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    static Atlas* atlas;
    static char name[256];
    chooseAtlas("Atlas: ", atlas, 0);
    UI::formField("Object name: ", name, sizeof(name));
    if (ImGui::Button("Ok")) {
      object = new ObjectClass(name, (path / (std::string(name) + ".obj")).string(), atlas);
      object->save();
      uint32_t index = 0;
      for (int i = 0; i < objects.size(); i++) {
        if (fs::absolute(objects[i]->path).string().compare(fs::absolute(object->path.string()).string()) >= 0) {
          index = i;
          break;
        }
      }
      objects.insert(objects.begin() + index, object);
      std::sort(objects.begin(), objects.end(), [](ObjectClass* a, ObjectClass* b) { return strcmp(a->name, b->name); });
      ImGui::CloseCurrentPopup();
    }
    ImGui::SameLine();
    if (ImGui::Button("Cancel")) ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  if (creatingFolder) {
    if (ImGui::InputText("##Filename Input", tmpFolder, sizeof(tmpFolder), ImGuiInputTextFlags_EnterReturnsTrue)) {
      fs::create_directory(path / tmpFolder);
      creatingFolder = false;
    }
  }
  if (ImGui::Button("+")) {
    ImGui::OpenPopup("Create Object");
  }
  if (ImGui::Button("+ Folder")) creatingFolder = true;

  ImGui::End();
}

static void inspectorWindow() {
  if (!ImGui::Begin("Inspector", &showInspector)) return ImGui::End();
  if (object && !TiledLevel::object) {
    ImGui::TextUnformatted("Object Class");
    ImGui::TextUnformatted(object->name.c_str());
    chooseAtlas("Atlas: ", object->atlas, 0);
    ImGui::Separator();

    ImGui::TextUnformatted("Object Properties:");
    int propertyToMove = -1, direction = 0;
    for (int i = 0; i < object->properties.size(); i++) {
      char buffer[256];
      strcpy(buffer, object->properties[i].name.c_str());
      float width = (ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("-").x - 10 - ImGui::GetStyle().FramePadding.x * 10) / 5;
      ImGui::SetNextItemWidth(width * 2);
      if (ImGui::InputTextEx(("##PropertyNameInput" + std::to_string(i)).c_str(), "Property name", buffer, sizeof(buffer), imVec(vec2i(0)), 0)) {
        object->properties[i].name = buffer;
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(width * 2);
      if (ImGui::BeginCombo(("##PropertyType" + std::to_string(i)).c_str(), propertyTypes[(int)object->properties[i].type].c_str())) {
        for (int j = 0; j < IM_ARRAYSIZE(propertyTypes); j++) {
          if (ImGui::Selectable(propertyTypes[j].c_str(), j == (int)object->properties[i].type)) object->properties[i].type = (PropertyType)j;
          if (j == (int)object->properties[i].type) ImGui::SetItemDefaultFocus();
        }
        ImGui::EndCombo();
      }
      ImGui::SameLine();
      ImGui::SetNextItemWidth(width);
      strcpy(buffer, object->properties[i].defaultValue.c_str());
      if (ImGui::InputTextEx(("##PropertyDefaultValueInput" + std::to_string(i)).c_str(), "Default value", buffer, sizeof(buffer), imVec(vec2i(0)), 0)) {
        object->properties[i].defaultValue = buffer;
      }
      ImGui::SameLine();
      if (ImGui::Button("-")) {
        object->properties.erase(object->properties.begin() + i);
        for (auto child : object->children) child->properties.erase(child->properties.begin() + i);
      }
      ImGui::SameLine();
      vec2i widgetPos = oreVec(ImGui::GetCursorScreenPos()) + oreVec(ImGui::GetStyle().FramePadding);
      int arrowHeight = ImGui::GetTextLineHeight() / 2 - 1;
      ImGui::GetWindowDrawList()->AddTriangleFilled(imVec(widgetPos + vec2i(5, 0)), imVec(widgetPos + vec2i(0, arrowHeight)), imVec(widgetPos + vec2i(10, arrowHeight)), MvColor::white.value);
      if (inRangeW(window->getMousePos(), widgetPos, vec2i(10, arrowHeight)) && Mova::isMouseButtonPressed(MOUSE_LEFT)) propertyToMove = i, direction = -1;
      widgetPos.y += arrowHeight + 2;
      ImGui::GetWindowDrawList()->AddTriangleFilled(imVec(widgetPos + vec2i(0, 0)), imVec(widgetPos + vec2i(10, 0)), imVec(widgetPos + vec2i(5, arrowHeight)), MvColor::white.value);
      if (inRangeW(window->getMousePos(), widgetPos, vec2i(10, arrowHeight)) && Mova::isMouseButtonPressed(MOUSE_LEFT)) propertyToMove = i, direction = 1;
      ImGui::NewLine();
    }
    if (ImGui::Button("+")) {
      object->properties.emplace_back();
      for (auto child : object->children) child->properties.emplace_back();
    }
    if (propertyToMove != -1 && inRange<int>(propertyToMove + direction, 0, object->properties.size())) {
      std::iter_swap(object->properties.begin() + propertyToMove, object->properties.begin() + propertyToMove + direction);
      for (auto child : object->children) std::iter_swap(child->properties.begin() + propertyToMove, child->properties.begin() + propertyToMove + direction);
    }
  } else if (TiledLevel::object) {
    ImGui::TextUnformatted("Object");
    ImGui::Text("Base Class: %s", TiledLevel::object->parent->name.c_str());
    ImGui::SameLine();
    if (ImGui::Button("Select")) return object = TiledLevel::object->parent, TiledLevel::object = nullptr, ImGui::End();
    ImGui::Separator();
    ImGui::TextUnformatted("Object Properties:");
    for (int i = 0; i < TiledLevel::object->properties.size(); i++) {
      ImGui::Text("%s: ", TiledLevel::object->parent->properties[i].name.c_str());
      ImGui::SameLine();

      char buffer[256];
      strcpy(buffer, TiledLevel::object->properties[i].c_str());
      ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::GetStyle().FramePadding.x * 2);
      if (ImGui::InputTextEx(("##PropertyValueInput" + std::to_string(i)).c_str(), "Property Value", buffer, sizeof(buffer), imVec(vec2i(0)), 0)) {
        TiledLevel::object->properties[i] = buffer;
      }
    }
  }
  ImGui::End();
}

void save() {
  for (const auto atlas : atlases) atlas->save();
  for (const auto object : objects) object->save();
}

void load() {
  {  // Atlases
    for (const auto atlas : atlases) delete atlas;
    atlases.clear();
    for (const auto& entry : fs::directory_iterator(projectSaveDirectory + "atlases/")) {
      if (entry.path().extension() == ".png") atlases.push_back(new Atlas(entry.path().stem().string()));
    }
    std::sort(atlases.begin(), atlases.end(), [](Atlas* a, Atlas* b) { return strcmp(a->name, b->name); });
    atlas = atlases.empty() ? nullptr : atlases[0];
  }

  {  // Objects
    for (const auto object : objects) delete object;
    objects.clear();
    for (const auto& p : fs::recursive_directory_iterator(projectSaveDirectory + "objects/")) {
      if (p.is_directory()) continue;
      objects.push_back(new ObjectClass(p.path().string()));
    }
  }
}

void exportData() {
  std::string data = "const uint8_t PROGMEM textures[] = {\n  ";

  data += itobytes(atlases.size(), 2);
  for (auto atlas : atlases) {
    int nTiles = atlas->width() * atlas->height();
    data += format("%d, %d, %d, ", nTiles, atlas->tilesize.x, atlas->tilesize.y);
    for (int i = 0; i < nTiles; i++) {
      for (int y = 0; y < atlas->tilesize.y; y++) {
        for (int x = 0; x < atlas->tilesize.x; x++) {
          uint16_t pixel = rgb565(atlas->image.getPixel(i * atlas->tilesize.x % atlas->image.width + x, i * atlas->tilesize.x / atlas->image.width * atlas->tilesize.y + y));
          data += format("%d, %d, ", pixel >> 8, pixel & 0xff);
        }
      }
    }
    data += format("%d, ", atlas->tileset != nullptr);
    if (atlas->tileset) {
      data += format("%d, ", atlas->tileset->patches.size());
      for (const auto& patch : atlas->tileset->patches) {
        data += format("%d, ", patch.x + patch.y * atlas->width() + 1);
      }
      data += format("%d, ", atlas->tileset->colliders != nullptr);
      if (atlas->tileset->colliders) {
        int tilesTotal = atlas->width() * atlas->height();
        for (int i = 0; i < tilesTotal / 8 + (tilesTotal % 8 != 0); i++) {
          uint8_t byte = 0;
          for (int j = 0; j < 8; j++) {
            if (i * 8 + j < tilesTotal) byte |= atlas->tileset->colliders[i * 8 + j] << j;
          }
          data += format("%d, ", byte);
        }
      }
    }
  }
  data += itobytes(objects.size(), 2);
  for (const auto object : objects) {
    data += itobytes(std::find(atlases.begin(), atlases.end(), object->atlas) - atlases.begin(), 2);
  }
  for (int i = 0; i < 2; i++) data.pop_back();
  data += "\n};";
  Mova::copyToClipboard(data);
}

void windows() {
  if (showAtlasSettingsPopup) ImGui::OpenPopup("Atlas settings"), showAtlasSettingsPopup = false;
  if (ImGui::BeginPopupModal("Atlas settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    static char buffer[256] = {'\1'};
    if (buffer[0] == '\1') strncpy(buffer, atlas->name.c_str(), sizeof(buffer) - 1);
    atlas->tilesize = max(atlas->tilesize, vec2(1));
    UI::formField("Atlas name: ", buffer, sizeof(buffer));
    UI::formField("Tile size: ", atlas->tilesize);
    vec2i tileCount = atlas->image.size() / atlas->tilesize;
    if (UI::formField("Tile count: ", tileCount)) atlas->tilesize = atlas->image.size() / tileCount;
    if (ImGui::Button("Ok")) atlas->name = buffer, buffer[0] = '\1', std::sort(atlases.begin(), atlases.end(), [](Atlas* a, Atlas* b) { return strcmp(a->name, b->name); }), ImGui::CloseCurrentPopup();
    ImGui::EndPopup();
  }

  if (showAtlas) atlasWindow();
  if (showInspector) inspectorWindow();
  if (showObjects) objectsWindow();
}
}  // namespace Textures
