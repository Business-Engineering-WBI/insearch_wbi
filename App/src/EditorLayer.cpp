#include "EditorLayer.h"

#include "Engine/Textures/Texture2D.h"
#include "ImGui/Constructions/SelectConstructionFromTree.hpp"
#include "ImGui/Overlays/Overlay.h"
#include "ImGui/Overlays/ScriptPopup.h"
#include "ImGui/Project/PageView/PageViewManager.h"
#include "ImGui/Tables/Table.h"

#include "Engine/Core/Application.h"
#include "Engine/ImGui/Fonts/ImGuiFontDefinesIconsFA.inl"
#include "Engine/ImGui/Fonts/ImGuiFontDefinesIconsFABrands.inl"
#include "Engine/Utils/Log.hpp"
#include "Engine/Utils/json.hpp"

#include <imgui.h>
#include <imgui_internal.h>

#include <fstream>
#include <iomanip>

namespace LM
{

    constexpr std::string_view kLastProjectPath = "./assets/last_project.json";
    constexpr std::string_view kAppLogoLight = "./assets/textures/wbi_white_logo.png";
    const std::string kJsonLastProjectPath = "folder";
    constexpr float kMainMenuFramePadding = 16.0f;

    EditorLayer::EditorLayer()
    {
        std::ifstream infile(kLastProjectPath.data());
        if (!infile.is_open())
        {
            LOG_CORE_INFO("Last project file not found");
            return;
        }

        nlohmann::json json;
        infile >> json;

        if (Ref<Project> project = Project::Open(json[kJsonLastProjectPath]); project != Project::s_ProjectNotOpen)
        {
            LOG_CORE_INFO("Opened last project: {}", project->GetCatalogFilename());
            m_Project = project;
            m_SetupProjectWindow.Open();
        }

        m_AppLogoLight = Texture2D::Create(kAppLogoLight);
        m_AppLogoLight->OnAttach();

        SelectConstructionFromTree::LoadTreeFromDefaultFile();
    }

    EditorLayer::~EditorLayer() { PageViewManager::OnAppClose(m_Project); }

    bool MyBeginMenu(std::string_view _Title)
    {
        ImGuiStyle& style = ImGui::GetStyle();

        style.TouchExtraPadding.y = kMainMenuFramePadding;
        bool res = ImGui::BeginMenu(_Title.data());
        style.TouchExtraPadding.y = 0.0f;
        if (ImGui::IsItemHovered())
        {
            Application::Get().SetIsMainMenuAnyItemHovered(true);
        }

        return res;
    }

    bool MyMenuButton(std::string_view _Title, float _CursorPosY, const ImVec2& _Size)
    {
        ImGui::SetCursorPosY(_CursorPosY);
        bool res = ImGui::Button(_Title.data(), _Size);
        if (ImGui::IsItemHovered())
        {
            Application::Get().SetIsMainMenuAnyItemHovered(true);
        }

        return res;
    }

    void EditorLayer::OnImGuiRender()
    {
        static bool dockspaceOpen = true;
        static bool opt_fullscreen_persistant = true;
        ImGuiViewport* viewport = ImGui::GetMainViewport();

        float frameHeightOld = ImGui::GetFrameHeight();

        ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, kMainMenuFramePadding);
        ImGui::PushStyleColor(ImGuiCol_MenuBarBg, 0xff1b1b1b);
        ImGuiWindowFlags viewportSideBarFlags = ImGuiWindowFlags_NoBackground | ImGuiWindowFlags_NoScrollbar |
                                                ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_MenuBar;
        Application::Get().SetIsMainMenuAnyItemHovered(false);
        float frameHeight = ImGui::GetFrameHeight();
        Application::Get().SetMainMenuFrameHeight(frameHeight);
        if (ImGui::BeginViewportSideBar("##Toolbar", viewport, ImGuiDir_Up, frameHeight, viewportSideBarFlags))
        {
            if (ImGui::BeginMenuBar())
            {
                ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, kMainMenuFramePadding);
                float logoHeight = frameHeight - kMainMenuFramePadding * 1.5f;
                float logoPosY = (frameHeight - logoHeight) / 2.0f;
                float cursorPosY = ImGui::GetCursorPosY();
                ImGui::SetCursorPosY(cursorPosY + logoPosY);
                ImGui::Image(reinterpret_cast<ImTextureID>(m_AppLogoLight->GetTextureId()),
                             { m_AppLogoLight->GetWidth() / m_AppLogoLight->GetHeight() * logoHeight, logoHeight });

                ImGui::SetCursorPosY(cursorPosY);
                if (MyBeginMenu("File"))
                {
                    if (ImGui::MenuItem("Open Project...", "Ctrl+O"))
                    {
                        OpenProject();
                    }

                    if (ImGui::MenuItem("New Project", "Ctrl+N"))
                    {
                        NewProject();
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Save", "Ctrl+S"))
                    {
                        SaveProject();
                    }

                    if (ImGui::MenuItem("Save Project As...", "Ctrl+Shift+S"))
                    {
                        SaveProjectAs();
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Close", "Ctrl+F4"))
                    {
                        CloseProject();
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("Exit"))
                    {
                        Application::Get().Close();
                    }

                    ImGui::EndMenu();
                }

                ImGui::SetCursorPosY(cursorPosY);
                if (MyBeginMenu("Window"))
                {
                    if (ImGui::MenuItem("Select Construction", nullptr, m_IsDrawSelectConstructionWindow))
                    {
                        m_IsDrawSelectConstructionWindow = !m_IsDrawSelectConstructionWindow;
                    }

                    if (ImGui::MenuItem("Test Table", nullptr, m_IsDrawTestTableWindow))
                    {
                        m_IsDrawTestTableWindow = !m_IsDrawTestTableWindow;
                    }

                    ImGui::Separator();

                    if (ImGui::MenuItem("ImGui Demo", nullptr, m_IsDrawImGuiDemoWindow))
                    {
                        m_IsDrawImGuiDemoWindow = !m_IsDrawImGuiDemoWindow;
                    }

                    ImGui::EndMenu();
                }

                ImGui::PopStyleVar();

                ImGui::PushStyleVarX(ImGuiStyleVar_ItemSpacing, 0.0f);
                ImGui::PushStyleColor(ImGuiCol_Button, 0x00000000);
                float buttonWidth = frameHeight * 1.2f;
                ImVec2 buttonSize = { buttonWidth, frameHeight };
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + ImGui::GetContentRegionAvail().x - buttonWidth * 3.0f +
                                     ImGui::GetStyle().WindowPadding.x);

                Window& window = Application::Get().GetWindow();

                if (MyMenuButton(ICON_FA_MINUS, cursorPosY, buttonSize))
                {
                    window.Minimize();
                }

                if (window.IsWindowMaximized())
                {
                    if (MyMenuButton(ICON_FA_WINDOW_RESTORE, cursorPosY, buttonSize))
                    {
                        window.Restore();
                    }
                }
                else
                {
                    if (MyMenuButton(ICON_FA_WINDOW_MAXIMIZE, cursorPosY, buttonSize))
                    {
                        window.Maximize();
                    }
                }

                ImGui::PushStyleColor(ImGuiCol_ButtonHovered, 0xff4f4eff);
                if (MyMenuButton(ICON_FA_XMARK, cursorPosY, buttonSize))
                {
                    Application::Get().Close();
                }
                ImGui::PopStyleColor();

                ImGui::PopStyleColor();

                ImGui::PopStyleVar();

                ImGui::EndMenuBar();
            }

            ImGui::End();
        }
        ImGui::PopStyleColor();
        ImGui::PopStyleVar();

        bool opt_fullscreen = opt_fullscreen_persistant;
        static ImGuiDockNodeFlags dockspace_flags = ImGuiDockNodeFlags_None;

        ImGuiWindowFlags window_flags = ImGuiWindowFlags_MenuBar | ImGuiWindowFlags_NoDocking;
        if (opt_fullscreen)
        {
            float offsetY = frameHeight - frameHeightOld;
            ImVec2 pos = viewport->Pos;
            pos.y += offsetY;
            ImVec2 size = viewport->Size;
            size.y -= offsetY;
            ImGui::SetNextWindowPos(pos);
            ImGui::SetNextWindowSize(size);
            ImGui::SetNextWindowViewport(viewport->ID);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, 0.0f);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowBorderSize, 0.0f);
            window_flags |= ImGuiWindowFlags_NoTitleBar | ImGuiWindowFlags_NoCollapse | ImGuiWindowFlags_NoResize |
                            ImGuiWindowFlags_NoMove | ImGuiWindowFlags_NoBringToFrontOnFocus |
                            ImGuiWindowFlags_NoNavFocus;
        }

        if (dockspace_flags & ImGuiDockNodeFlags_PassthruCentralNode)
        {
            window_flags |= ImGuiWindowFlags_NoBackground;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(0.0f, 0.0f));
        ImGui::Begin("DockSpace Demo", &dockspaceOpen, window_flags);
        ImGui::PopStyleVar();

        if (opt_fullscreen)
        {
            ImGui::PopStyleVar(2);
        }

        ImGuiIO& io = ImGui::GetIO();
        ImGuiStyle& style = ImGui::GetStyle();
        float minWinSizeX = style.WindowMinSize.x;
        style.WindowMinSize.x = 180.0f;
        if (io.ConfigFlags & ImGuiConfigFlags_DockingEnable)
        {
            ImGuiID dockspace_id = ImGui::GetID("MyDockSpace");
            ImGui::DockSpace(dockspace_id, ImVec2(0.0f, 0.0f), dockspace_flags);
        }
        style.WindowMinSize.x = minWinSizeX;
        ImGui::End();

        if (m_IsDrawTestTableWindow)
        {
            if (ImGui::Begin("Test Table", &m_IsDrawTestTableWindow))
            {
                Table::DrawTable();
            }
            ImGui::End();
        }

        if (m_CreateNewProject.Draw())
        {
            m_Project = m_CreateNewProject.GetNewProject();
            m_CreateNewProject.Close();
            m_SetupProjectWindow.Open();
            SaveLastProjectPath();
        }

        m_SetupProjectWindow.Draw(m_Project);

        m_SharedConnectionConfigSetup.OnImGuiRender();
        m_Yg1ShopConstructionLevelConfigSetup.OnImGuiRender();

        if (m_Project)
        {
            switch (m_Project->GetType())
            {
                case ProjectVariant::kPdfTablesWithOcr: PageViewManager::GetPdfOcr()->DrawViews(m_Project); break;
                case ProjectVariant::kPdfTablesWithoutOcr: PageViewManager::GetPdf()->DrawViews(m_Project); break;
                case ProjectVariant::kExcelTables: PageViewManager::GetExcelFolder()->DrawViews(m_Project); break;
            }
        }

        Overlay::Get()->Draw();
        ScriptPopup::Get()->Draw();

        if (m_IsDrawImGuiDemoWindow)
        {
            ImGui::ShowDemoWindow(&m_IsDrawImGuiDemoWindow);
        }

        if (m_IsDrawSelectConstructionWindow)
        {
            static SelectConstructionFromTree constrTreeTest;
            if (auto construction = constrTreeTest(); !construction.empty())
            {
                LOG_CORE_WARN("Constr tree test message on select: {}", construction);
            }
        }
    }

    void EditorLayer::OpenProject()
    {
        if (Ref<Project> project = Project::Open(); project != Project::s_ProjectNotOpen)
        {
            CloseProject();
            m_Project = project;
            m_SetupProjectWindow.Open();
            SaveLastProjectPath();
        }
    }

    void EditorLayer::NewProject()
    {
        CloseProject();
        m_CreateNewProject.Open();
        /*if (Ref<Project> project = Project::New(); project != Project::s_ProjectNotOpen)
        {
            m_Project = project;
            m_SetupProjectWindow.Open();
        }*/
    }

    void EditorLayer::SaveProject()
    {
        Project::Save(m_Project);
        SaveLastProjectPath();
    }

    void EditorLayer::SaveProjectAs()
    {
        // TODO: Save and copy assets
    }

    void EditorLayer::CloseProject()
    {
        PageViewManager::Save();
        PageViewManager::Clear();
        ClearLastProjectPath();
        m_Project = Project::s_ProjectNotOpen;
    }

    void EditorLayer::SaveLastProjectPath()
    {
        if (m_Project == Project::s_ProjectNotOpen)
        {
            return;
        }

        std::ofstream fout(kLastProjectPath.data());
        if (!fout.is_open())
        {
            LOG_CORE_WARN("Can't save last project path");
            return;
        }

        nlohmann::json json;
        json[kJsonLastProjectPath] = m_Project->GetPathInFolder(Project::s_ProjectFileName);

        fout << std::setw(4) << json;
    }

    void EditorLayer::ClearLastProjectPath()
    {
        if (m_Project == Project::s_ProjectNotOpen)
        {
            return;
        }
    }

}    // namespace LM
