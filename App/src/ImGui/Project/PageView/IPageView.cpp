#include "IPageView.h"

#include "Engine/Utils/Log.hpp"
#include "PageViewManager.h"

#include <imgui.h>

namespace LM
{

    IPageView::IPageView() { }

    void IPageView::Draw()
    {
        ImGuiWindowFlags windowFlags = ImGuiWindowFlags_None;
        if (m_IsDisableNextFrameInputEvents)
        {
            LOG_CORE_INFO("Disable next frame input events");
            windowFlags |= ImGuiWindowFlags_NoInputs;
            m_IsDisableNextFrameInputEvents = false;
        }

        if (ImGui::Begin(GetWindowName(), nullptr, windowFlags))
        {
            PageViewManager::GetCurrent()->DrawViewTopMenu();
            DrawTopMenuExtras();

            DrawWindowContent();

            DrawExtras();
        }
        ImGui::End();
    }

    void IPageView::SetContext(Ref<Project> _Project, int _PageId)
    {
        m_Project = _Project;
        m_PageId = _PageId;
        m_BasePath = GetBasePath();
    }

    void IPageView::ClearContext() { m_Project = Project::s_ProjectNotOpen; }

}    // namespace LM
