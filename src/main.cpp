#include "common.hpp"
#include "editor.hpp"
#include "style.hpp"
#include <GL/gl.h>

void renderViewport() {
  Textures::windows();
  TiledLevel::windows();
}

namespace UI {
MvImage outline = MvImage("Assets/res/image/outline.png");
MvImage transparentGrid = MvImage("Assets/res/image/transparent.png");
MvImage cursor = MvImage("Assets/res/image/ui/cursor.png");
};  // namespace UI

namespace UI::Tool {
MvImage brush = MvImage("Assets/res/image/ui/tool/brush.png");
MvImage fillBucket = MvImage("Assets/res/image/ui/tool/bucket.png");
MvImage line = MvImage("Assets/res/image/ui/tool/line.png");
MvImage rect = MvImage("Assets/res/image/ui/tool/rect.png");
MvImage colorPicker = MvImage("Assets/res/image/ui/tool/colorPicker.png");
};  // namespace UI::Tool

ImGuiID dockspaceID;
MvWindow* window;
std::string status, projectSaveDirectory;

enum class Layout {
  PIXEL_TILE,
};

void setLayout(Layout layout) {
  Textures::showAtlas = Textures::showInspector = Textures::showObjects = false;
  TiledLevel::showEditor = false;
  ImGui::DockBuilderRemoveNode(dockspaceID);
  ImGui::DockBuilderAddNode(dockspaceID);
  ImGui::DockBuilderSetNodeSize(dockspaceID, ImGui::GetMainViewport()->Size);
  ImGuiID leftPanel = ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Left, 0.2f, nullptr, &dockspaceID);
  ImGuiID bottomPanel = ImGui::DockBuilderSplitNode(dockspaceID, ImGuiDir_Down, 0.3f, nullptr, &dockspaceID);
  if (layout == Layout::PIXEL_TILE) {
    Textures::showAtlas = Textures::showInspector = Textures::showObjects = true;
    TiledLevel::showEditor = true;
    ImGui::DockBuilderDockWindow("Tiled Level Editor", dockspaceID);
    ImGui::DockBuilderDockWindow("Texture Atlas", bottomPanel);
    ImGui::DockBuilderDockWindow("Inspector", leftPanel);
    ImGui::DockBuilderDockWindow("Objects", bottomPanel);
  }
}

int main(int argc, const char** argv) {
  window = new MvWindow("Ore Asset Editor", MvRendererType::OpenGL);
  Mova::ImGui_Init(*window);

  SetupImGuiStyle();
  ImGui::GetIO().ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_DockingEnable;
  ImGui::GetIO().FontDefault = ImGui::GetIO().Fonts->AddFontFromFileTTF("Assets/res/Consolas.ttf", 12.0);
  ImGui::GetIO().ConfigWindowsMoveFromTitleBarOnly = true;

  while (window->isOpen) {
    Mova::ImGui_NewFrame();
    status = "";
    if (!dockspaceID) {
      dockspaceID = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());
      setLayout(Layout::PIXEL_TILE);                                      // TODO: Default layout?
      loadProject("D:/Dev/Arduino/Projects/GameBoy/res/Mario/Project/");  // TODO: load last project?
    } else dockspaceID = ImGui::DockSpaceOverViewport(ImGui::GetMainViewport());

    ImGui::BeginMainMenuBar();
    if (ImGui::BeginMenu("File")) {
      if (ImGui::MenuItem("Open project", "CTRL+O")) loadProject(projectSaveDirectory = openDir());
      if (ImGui::MenuItem("Save project", "CTRL+S")) saveProject();
      if (ImGui::MenuItem("Save project as", "CTRL+SHIFT+S")) saveProject(projectSaveDirectory = openDir());
      if (ImGui::MenuItem("Open project's folder", "CTRL+K")) openProjectsFolder();
      if (ImGui::MenuItem("Export texture atlases")) Textures::exportData();
      if (ImGui::MenuItem("Export levels")) TiledLevel::exportData();
      ImGui::EndMenu();
    }
    if (Mova::isKeyHeld(MvKey::Ctrl) && Mova::isKeyPressed(MvKey::O)) loadProject(keepOldIfEmpty(projectSaveDirectory, openDir()));
    if (Mova::isKeyHeld(MvKey::Ctrl) && Mova::isKeyPressed(MvKey::S)) saveProject();
    if (Mova::isKeyHeld(MvKey::Ctrl) && Mova::isKeyHeld(MvKey::ShiftLeft) && Mova::isKeyPressed(MvKey::S)) saveProject(keepOldIfEmpty(projectSaveDirectory, openDir()));
    if (Mova::isKeyHeld(MvKey::Ctrl) && Mova::isKeyPressed(MvKey::K)) openProjectsFolder();

    if (ImGui::BeginMenu("Level")) {
      if (ImGui::MenuItem("New level")) TiledLevel::newLevel();
      if (ImGui::MenuItem("Level settings")) TiledLevel::levelSettings();
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("Texture")) {
      if (ImGui::MenuItem("Import texture atlas")) Textures::importAtlas(openFile());
      if (ImGui::MenuItem("Atlas settins")) Textures::atlasSettings();
      ImGui::EndMenu();
    }

    if (ImGui::BeginMenu("View")) {
      if (ImGui::MenuItem("Texture atlas", nullptr, Textures::showAtlas)) Textures::showAtlas = !Textures::showAtlas;
      if (ImGui::MenuItem("Tiled Level Editor", nullptr, TiledLevel::showEditor)) TiledLevel::showEditor = !TiledLevel::showEditor;
      ImGui::Separator();
      if (ImGui::MenuItem("Pixel / Tile workspace layout")) setLayout(Layout::PIXEL_TILE);
      ImGui::EndMenu();
    }
    ImGui::EndMainMenuBar();

    // Mova::setCursor(MvCursor::Default);
    renderViewport();

    if (ImGui::BeginViewportSideBar("##MainStatusBar", ImGui::GetMainViewport(), ImGuiDir_Down, ImGui::GetFrameHeight(), ImGuiWindowFlags_MenuBar)) {
      if (ImGui::BeginMenuBar()) {
        ImGui::TextUnformatted(status.c_str());
        ImGui::EndMenuBar();
      }
      ImGui::End();
    }

    glClearColor(MvColor::darkgray);
    glClear(GL_COLOR_BUFFER_BIT);
    Mova::ImGui_Render();
    Mova::nextFrame();
  }

  Mova::ImGui_Shutdown();
  delete window;
  for (auto atlas : Textures::atlases) delete atlas;
  for (auto object : Textures::objects) delete object;
  for (auto object : TiledLevel::levels) delete object;
  return 0;
}
