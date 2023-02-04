#include "common.hpp"
#include "editor.hpp"

namespace TiledLevel {
static bool showLevelSettingsPopup = false;
bool showEditor = false;
vector<Level*> levels;
Level* level;
Object* object;

void newLevel() { level = nullptr, showLevelSettingsPopup = true; }
void levelSettings() { showLevelSettingsPopup = true; }

static bool concatX(vec2i tile, vec2i pos, int dir) { return inRange(pos.x + dir, 0, (int)level->width) && level->getTile(pos + vec2i(dir, 0)) == level->tileset->tileset->patch(tile); }
static bool concatY(vec2i tile, vec2i pos, int dir) { return inRange(pos.y + dir, 0, (int)level->height) && level->getTile(pos + vec2i(0, dir)) == level->tileset->tileset->patch(tile); }

static void drawQuater(MvDrawTarget& viewport, vec2i pos, vec2i tile, vec2f screen, vec2f tileScreenSize, vec2i quater) {
  vec2i delta = quater * 2 - 1;
  if (!concatX(tile, pos, delta.x)) tile.x += concatY(tile, pos, delta.y) ? 3 : 1;
  else if (!concatY(tile, pos, delta.y)) tile.x += 2;
  viewport.drawImage(level->tileset->image, floor(screen + quater * tileScreenSize / 2), ceil(tileScreenSize / 2), (tile * 2 + quater) * level->tileset->tilesize / 2, level->tileset->tilesize / 2);
}

static void drawPatch(MvDrawTarget& viewport, vec2i pos, vec2i tile, vec2i screen, vec2i tileScreenSize) {
  drawQuater(viewport, pos, tile, screen, tileScreenSize, vec2i(0, 0));
  drawQuater(viewport, pos, tile, screen, tileScreenSize, vec2i(1, 0));
  drawQuater(viewport, pos, tile, screen, tileScreenSize, vec2i(0, 1));
  drawQuater(viewport, pos, tile, screen, tileScreenSize, vec2i(1, 1));
}

void editor() {
  static std::unique_ptr<MvImage> viewport;
  static vec2f camera = 0;
  static float scale = 3;
  static int menuObject;
  if (!ImGui::Begin("Tiled Level Editor", &showEditor)) return ImGui::End();
  if (!levels.empty()) {
    if (!level) level = levels[0];
    if (ImGui::BeginCombo("##LevelSelect", level->name.c_str())) {
      for (const auto item : levels) {
        if (ImGui::Selectable(item->name.c_str(), item == level)) level = item;
        if (item == level) ImGui::SetItemDefaultFocus();
      }
      ImGui::EndCombo();
    }
  }
  vec2i viewportSize = availableRegion();
  vec2i viewportPos = oreVec(ImGui::GetCursorScreenPos());
  vec2f mouse = oreVec(ImGui::GetMousePos()) - viewportPos;
  if (!viewport || viewport->width != viewportSize.x || viewport->height != viewportSize.y) viewport = std::unique_ptr<MvImage>(new MvImage(max(viewportSize, vec2i(1)), nullptr));

  if (level && level->tileset) {
    viewport->clear(MvColor::black);
    vec2f tileScreenSize = level->tileset->tilesize * scale;
    viewport->fillRect(-camera, tileScreenSize * level->size(), MvColor(135, 206, 235));
    for (int x = max(camera.x / tileScreenSize.x, 0); x <= min((camera.x + viewport->width) / tileScreenSize.x, level->width - 1); x++) {
      for (int y = max(camera.y / tileScreenSize.y, 0); y <= min((camera.y + viewport->height) / tileScreenSize.y, level->height - 1); y++) {
        vec2i tile = level->getTile(vec2i(x, y));
        vec2f screen = vec2f(x, y) * tileScreenSize - camera;
        if (tile != -1) {
          if (level->tileset->tileset->inPatch(tile)) drawPatch(*viewport, vec2i(x, y), level->tileset->tileset->patch(tile), screen, tileScreenSize);
          else viewport->drawImage(level->tileset->image, screen, tileScreenSize, tile * level->tileset->tilesize, level->tileset->tilesize);
        }
      }
    }
    for (const auto& object : level->objects) {
      viewport->drawImage(object.parent->atlas->image, (vec2f)object.pos / level->tileset->tilesize * tileScreenSize - camera, object.parent->atlas->tilesize * scale, 0, object.parent->atlas->tilesize);
    }
    ImGui::Image(imID(*viewport), imVec(viewportSize));

    if (ImGui::IsItemHovered()) {
      vec2i selected = (mouse + camera) / tileScreenSize;
      if (selected.x >= 0 && selected.x < level->width && selected.y >= 0 && selected.y < level->height || Textures::object) {
        if (Mova::isMouseButtonHeld(MOUSE_LEFT) && !Mova::isKeyHeld(MvKey::Ctrl)) {
          bool found = false;
          for (auto& object : level->objects) {
            if (inRangeW<vec2i>(mouse, (vec2f)object.pos / level->tileset->tilesize * tileScreenSize - camera, object.parent->atlas->tilesize * scale)) {
              TiledLevel::object = &object;
              found = true;
              break;
            }
          }
          if (found) {
          } else if (Textures::object) level->objects.push_back(Object(Textures::object, Mova::isKeyHeld(MvKey::Alt) ? vec2i((mouse + camera) / scale) : selected * level->tileset->tilesize));
          else if (Textures::atlas == level->tileset) level->setTile(selected, Textures::selected);
        } else if (Mova::isMouseButtonHeld(MOUSE_RIGHT) && !Mova::isKeyHeld(MvKey::Ctrl)) {
          bool found = false;
          for (int i = 0; i < level->objects.size(); i++) {
            if (inRangeW<vec2i>((mouse + camera) / scale, level->objects[i].pos, level->objects[i].parent->atlas->tilesize)) {
              ImGui::OpenPopup("Object Settings");
              menuObject = i;
              found = true;
              break;
            }
          }
          if (!found) level->setTile(selected, -1);
        }
        vec2i selectedScreen = viewportPos + selected * tileScreenSize - camera;
        ImGui::GetWindowDrawList()->AddRect(imVec(selectedScreen), imVec(selectedScreen + tileScreenSize), MvColor::red.value);
      }
      status = format("Tile at %d, %d", selected.x, selected.y);

      if (ImGui::IsWindowHovered()) {
        float lastScale = scale;
        scale = max(scale + Mova::getScrollY() * 0.4f, min(viewportSize.x / float(level->width * level->tileset->tilesize.x), viewportSize.y / float(level->height * level->tileset->tilesize.y)));
        if (Mova::getScrollY()) camera = (mouse + camera) * scale / lastScale - mouse;
        if (Mova::isMouseButtonHeld(MOUSE_LEFT) && Mova::isKeyHeld(MvKey::Ctrl) || Mova::isMouseButtonHeld(MOUSE_MIDDLE)) camera -= Mova::getMouseDelta();
      }
    }
    camera = max(vec2i(0), min(level->size() * level->tileset->tilesize * scale - viewportSize, camera));
  } else ImGui::TextUnformatted("\"File->New level\" to create new level!");

  if (ImGui::BeginPopup("Object Settings")) {
    if (ImGui::MenuItem("Remove Object")) {
      if (&level->objects[menuObject] == TiledLevel::object) TiledLevel::object = nullptr;
      level->objects.erase(level->objects.begin() + menuObject);
    }
    ImGui::EndPopup();
  }

  ImGui::End();
}

void save() {
  for (const auto level : levels) {
    level->save();
  }
}

void load() {
  for (const auto& entry : fs::directory_iterator(projectSaveDirectory + "levels/")) {
    if (entry.path().extension() == ".lvl") levels.push_back(new Level(entry.path().stem().string()));
  }
  level = levels.empty() ? nullptr : levels[0];
}

void exportData() {
  if (!level) return;
  std::string data = "const uint8_t PROGMEM levels[] = {\n  ";
  data += itobytes(levels.size(), 2);
  for (const auto level : levels) {
    data += itobytes(level->width, 2);
    data += itobytes(level->height, 2);
    for (int y = 0; y < level->height; y++) {
      for (int x = 0; x < level->width; x++) {
        vec2i tile = level->getTile(vec2i(x, y));
        if (tile == -1) data += "0, ";
        else data += itobytes(tile.x + tile.y * level->tileset->image.width / level->tileset->tilesize.x + 1, 1);
      }
    }
    uint32_t objectsDataSizeInsert = data.size();
    data += itobytes(level->objects.size(), 2);
    for (const auto& object : level->objects) {
      data += itobytes(object.pos.x, 2) + itobytes(object.pos.y, 2);
      data += itobytes(std::find(Textures::objects.begin(), Textures::objects.end(), object.parent) - Textures::objects.begin(), 2);
      for (int i = 0; i < object.properties.size(); i++) {
        auto type = object.parent->properties[i].type;
        if (type == Textures::PropertyType::INT) data += itobytes(std::stoi(object.properties[i]), 4);
        else if (type == Textures::PropertyType::STRING) {
          for (auto character : object.properties[i]) data += format("%d, ", (uint8_t)character);
          data += "0, ";
        }
      }
    }
    data.insert(objectsDataSizeInsert, itobytes(std::count(data.begin() + objectsDataSizeInsert, data.end(), ','), 4));
  }
  for (int i = 0; i < 2; i++) data.pop_back();
  data += "\n};";
  Mova::copyToClipboard(data);
}

void windows() {
  if (showLevelSettingsPopup) ImGui::OpenPopup("Level settings");
  if (ImGui::BeginPopupModal("Level settings", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
    static vec2i levelSize;
    static char levelName[256];
    static Textures::Atlas* tileset;
    if (level && showLevelSettingsPopup) {
      levelSize = level->size();
      strncpy(levelName, level->name.c_str(), sizeof(levelName));
      tileset = level->tileset;
    }

    levelSize = max(levelSize, vec2i(1));
    UI::formField("Level size: ", levelSize);
    if (!level) UI::formField("Level name: ", levelName, sizeof(levelName));
    Textures::chooseAtlas("Level tileset: ", tileset, 1);

    bool disabled = false;
    if (!level) {
      disabled = levelName[0] == '\0';
      if (!disabled) {
        for (const auto level : levels) {
          if (level->name == levelName) {
            disabled = true;
            break;
          }
        }
      }
    }

    ImGui::BeginDisabled(disabled);
    if (ImGui::Button("Ok")) {
      if (!level) levels.push_back(level = new Level(levelName, tileset, levelSize.x, levelSize.y));
      else {
        level->resize(levelSize);
        level->tileset = tileset;
      }
      ImGui::CloseCurrentPopup();
    }
    ImGui::EndDisabled();
    ImGui::EndPopup();
  }
  if (showLevelSettingsPopup) showLevelSettingsPopup = false;

  if (showEditor) editor();
}
}  // namespace TiledLevel
