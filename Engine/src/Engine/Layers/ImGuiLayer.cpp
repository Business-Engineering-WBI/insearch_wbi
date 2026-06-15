#include "ImGuiLayer.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <Engine/ImGui/imgui_impl_glfw.h>
#include <Engine/ImGui/imgui_impl_opengl3.h>

#include "Engine/Core/Application.h"
#include "Engine/Events/EventDispatcher.h"
#include "Engine/ImGui/Fonts/ImGuiFontDefinesIconsFA.inl"
#include "Engine/ImGui/Fonts/ImGuiFontDefinesIconsFABrands.inl"
#include "Engine/Utils/Log.hpp"

// TODO: Remove TEMPORARY
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>

#include <filesystem>

#define USE_CUSTOM_FONT true

namespace LM
{

    const std::string regFont = "assets/fonts/roboto/Roboto-Regular.ttf";
    const std::string regConsolas = "assets/fonts/consolas/consolas.ttf";
    const std::string settingsFile = "assets/settings/imgui.json";
    const std::string faFontsFolder = "assets/fonts/fa/";

    ImGuiLayer::ImGuiLayer() : Layer("ImGuiLayer") { s_Instance = this; }

    void ImGuiLayer::OnAttach()
    {

        // Setup Dear ImGui context
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();
        ImGuiIO& io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;    // Enable Keyboard Controls
        // io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;      // Enable Gamepad Controls
        io.ConfigFlags |= ImGuiConfigFlags_DockingEnable;      // Enable Docking
        io.ConfigFlags |= ImGuiConfigFlags_ViewportsEnable;    // Enable Multi-Viewport / Platform Windows
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoTaskBarIcons;
        // io.ConfigFlags |= ImGuiConfigFlags_ViewportsNoMerge;
        io.ConfigDpiScaleFonts = true;

#if USE_CUSTOM_FONT

        m_ChangeSize = true;
        ChangeFontSize(false);
#endif

        // ImFontGlyphRangesBuilder

        /*
        auto grb = ImFontAtlas::GetGlyphRangesCyrillic();
        ImFontAtlas::
        grb.AddText(u8"?????");
        for (int n = 0; n < 0xFFFF; n++)
        {
            if (grb.GetBit(n))
            {
                printf("Contains: U+%04X\n", n);
            }
        }
        */

        // Setup Dear ImGui style
        ImGui::StyleColorsDark();
        // ImGui::StyleColorsClassic();

        // When viewports are enabled we tweak WindowRounding/WindowBg so platform windows can look identical to regular
        // ones.
        ImGuiStyle& style = ImGui::GetStyle();
        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            style.WindowRounding = 0.0f;
            style.Colors[ImGuiCol_WindowBg].w = 1.0f;
        }

        SetDarkThemeColors();

        Application& app = Application::Get();
        GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());

        // Setup Platform/Renderer bindings
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }

    void ImGuiLayer::OnDetach()
    {
        ImGui_ImplOpenGL3_Shutdown();
        ImGui_ImplGlfw_Shutdown();
        ImGui::DestroyContext();
    }

    void ImGuiLayer::OnEvent(Event& e)
    {
        EventDispatcher dispatcher(e);

        dispatcher.Dispatch<WindowMonitorScaleChangedEvent>([&](WindowMonitorScaleChangedEvent& event) {
            // m_ChangeSize = true;

            // bool control = Input::IsKeyPressed(LM::Key::LeftControl) || Input::IsKeyPressed(LM::Key::RightControl);
            // bool shift = Input::IsKeyPressed(LM::Key::LeftShift) || Input::IsKeyPressed(LM::Key::RightShift);
            //{
            //     if (control && shift)
            //     {
            //         if (event.GetKeyCode() == LM::Key::Minus)
            //         {
            //             --m_SizeId;
            //             m_ChangeSize = true;
            //         }
            //         if (event.GetKeyCode() == LM::Key::Equal)
            //         {
            //             ++m_SizeId;
            //             m_ChangeSize = true;
            //         }
            //     }
            return false;
            // }
        });

        if (m_BlockEvents)
        {
            ImGuiIO& io = ImGui::GetIO();
            e.Handled |= e.IsInCategory(EventCategoryMouse) & io.WantCaptureMouse;
            e.Handled |= e.IsInCategory(EventCategoryKeyboard) & io.WantCaptureKeyboard;
        }
    }

    bool ImGuiLayer::PushConsolasFont()
    {
        if (m_ConsolasFont)
        {
            ImGui::PushFont(m_ConsolasFont);
            return true;
        }
        return false;
    }

    void ImGuiLayer::Begin()
    {
        ChangeFontSize(true);

        Application& app = Application::Get();
        GLFWwindow* window = static_cast<GLFWwindow*>(app.GetWindow().GetNativeWindow());
        ImGuiIO& io = ImGui::GetIO();

        int win_w, win_h;
        glfwGetWindowSize(window, &win_w, &win_h);
        double mouse_x, mouse_y;
        glfwGetCursorPos(window, &mouse_x, &mouse_y);
        // bool on_edge = mouse_x <= 8 || mouse_x >= win_w - 8 || mouse_y <= 8 || mouse_y >= win_h - 8;
        bool on_edge = (mouse_x >= 0 && mouse_x <= win_w && mouse_y >= 0 && mouse_y <= win_h) &&
                       (mouse_x <= 8 || mouse_x >= win_w - 8 || mouse_y <= 8 || mouse_y >= win_h - 8);

        if (on_edge)
        {
            io.ConfigFlags |= ImGuiConfigFlags_NoMouseCursorChange;
        }
        else
        {
            io.ConfigFlags &= ~ImGuiConfigFlags_NoMouseCursorChange;
        }

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();
    }

    void ImGuiLayer::End()
    {

        ImGuiIO& io = ImGui::GetIO();
        Application& app = Application::Get();
        io.DisplaySize = ImVec2((float)app.GetWindow().GetWidth(), (float)app.GetWindow().GetHeight());

        // Rendering
        ImGui::Render();
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        if (io.ConfigFlags & ImGuiConfigFlags_ViewportsEnable)
        {
            GLFWwindow* backup_current_context = glfwGetCurrentContext();
            ImGui::UpdatePlatformWindows();
            ImGui::RenderPlatformWindowsDefault();
            glfwMakeContextCurrent(backup_current_context);
        }
    }

    void ImGuiLayer::SetDarkThemeColors()
    {
        auto& colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_WindowBg] = ImVec4 { 0.1f, 0.105f, 0.11f, 1.0f };

        // Headers
        colors[ImGuiCol_Header] = ImVec4 { 0.2f, 0.205f, 0.21f, 1.0f };
        colors[ImGuiCol_HeaderHovered] = ImVec4 { 0.3f, 0.305f, 0.31f, 1.0f };
        colors[ImGuiCol_HeaderActive] = ImVec4 { 0.15f, 0.1505f, 0.151f, 1.0f };

        // Buttons
        colors[ImGuiCol_Button] = ImVec4 { 0.2f, 0.205f, 0.21f, 1.0f };
        colors[ImGuiCol_ButtonHovered] = ImVec4 { 0.3f, 0.305f, 0.31f, 1.0f };
        colors[ImGuiCol_ButtonActive] = ImVec4 { 0.15f, 0.1505f, 0.151f, 1.0f };

        // Frame BG
        colors[ImGuiCol_FrameBg] = ImVec4 { 0.2f, 0.205f, 0.21f, 1.0f };
        colors[ImGuiCol_FrameBgHovered] = ImVec4 { 0.3f, 0.305f, 0.31f, 1.0f };
        colors[ImGuiCol_FrameBgActive] = ImVec4 { 0.15f, 0.1505f, 0.151f, 1.0f };

        // Tabs
        colors[ImGuiCol_Tab] = ImVec4 { 0.15f, 0.1505f, 0.151f, 1.0f };
        colors[ImGuiCol_TabHovered] = ImVec4 { 0.38f, 0.3805f, 0.381f, 1.0f };
        colors[ImGuiCol_TabActive] = ImVec4 { 0.28f, 0.2805f, 0.281f, 1.0f };
        colors[ImGuiCol_TabUnfocused] = ImVec4 { 0.15f, 0.1505f, 0.151f, 1.0f };
        colors[ImGuiCol_TabUnfocusedActive] = ImVec4 { 0.2f, 0.205f, 0.21f, 1.0f };

        // Title
        colors[ImGuiCol_TitleBg] = ImVec4 { 0.15f, 0.1505f, 0.151f, 1.0f };
        colors[ImGuiCol_TitleBgActive] = ImVec4 { 0.15f, 0.1505f, 0.151f, 1.0f };
        colors[ImGuiCol_TitleBgCollapsed] = ImVec4 { 0.15f, 0.1505f, 0.151f, 1.0f };
    }

    void ImGuiLayer::ChangeFontSize(bool _NeedUpdateFontTexture)
    {
        if (m_FontSize == 0)
        {
            return;
        }

#if USE_CUSTOM_FONT
        if (m_ChangeSize)
        {
            ImGuiIO& io = ImGui::GetIO();

            ImFontConfig config;

            ImFont* font = nullptr;

            float fontSize = static_cast<float>(m_FontSize);
            font = io.Fonts->AddFontFromFileTTF(regFont.c_str(), fontSize, &config, io.Fonts->GetGlyphRangesCyrillic());

            config.MergeMode = true;
            config.GlyphMinAdvanceX = fontSize;    // Use if you want to make the icon monospaced
            // config.DstFont = font;

            static const ImWchar iconRangesFa[] = { ICON_MIN_FA, ICON_MAX_FA, 0 };
            std::filesystem::path farPath =
                std::filesystem::path(faFontsFolder) / std::filesystem::path(FONT_ICON_FILE_NAME_FAR);
            std::filesystem::path fasPath =
                std::filesystem::path(faFontsFolder) / std::filesystem::path(FONT_ICON_FILE_NAME_FAS);
            io.Fonts->AddFontFromFileTTF(farPath.string().c_str(), fontSize, &config, iconRangesFa);
            io.Fonts->AddFontFromFileTTF(fasPath.string().c_str(), fontSize, &config, iconRangesFa);
            // io.Fonts->AddFontFromFileTTF(regFont.c_str(), fontSize, &config, );

            static const ImWchar iconRangesFab[] = { ICON_MIN_FAB, ICON_MAX_FAB, 0 };
            std::filesystem::path fabPath =
                std::filesystem::path(faFontsFolder) / std::filesystem::path(FONT_ICON_FILE_NAME_FAB);
            io.Fonts->AddFontFromFileTTF(fabPath.string().c_str(), fontSize, &config, iconRangesFab);

            m_ConsolasFont = io.Fonts->AddFontFromFileTTF(regConsolas.c_str(), fontSize, nullptr,
                                                          io.Fonts->GetGlyphRangesCyrillic());

            io.FontDefault = font;

            if (_NeedUpdateFontTexture)
            {
                io.Fonts->Build();
                ImGui_ImplOpenGL3_DestroyFontsTexture();
                ImGui_ImplOpenGL3_CreateFontsTexture();
            }

            LOG_CORE_INFO("Font size: {}", m_FontSize);

            m_ChangeSize = false;
        }

#endif
    }

    uint32_t ImGuiLayer::GetActiveWidgetID() const { return GImGui->ActiveId; }

}    // namespace LM
