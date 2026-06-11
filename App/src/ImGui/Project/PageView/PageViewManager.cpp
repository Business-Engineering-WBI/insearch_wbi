#include "PageViewManager.h"

#include "CutByPatternImgPageView.h"
#include "RawImgPageView.h"
#include "Utils/FileSystemUtils.h"
#include "XlsxImgSyncPageView.h"
#include "XlsxPageView.hpp"

#include <imgui.h>

#include <algorithm>
#include <filesystem>

namespace LM
{

    int PageViewManager::GetPagesCountByManager(const Ref<Project>& _Project, const std::string& _ManagerHash)
    {
        if (_Project == Project::s_ProjectNotOpen)
        {
            return 0;
        }

        const auto CountFiles = [](const std::filesystem::path& _Path) -> int {
            return static_cast<int>(FileSystemUtils::FilesCountInDirectory(_Path));
        };

        if (_ManagerHash == kHashPdfOcr || _ManagerHash == kHashPdf)
        {
            return CountFiles(_Project->GetPdfTablesWithOcrTypeRawImgPath());
        }

        if (_ManagerHash == kHashExcelFolder)
        {
            const int xlsxCount = CountFiles(_Project->GetVariantExcelTablesHelpers().GetXlsxStartupPath());
            // const int imgCount = CountFiles(_Project->GetPdfTablesWithOcrTypeRawImgPrevPath());
            return xlsxCount;
            // return std::max(xlsxCount, imgCount);
        }

        return 0;
    }

    PageViewManager::PageViewManager() { }

    bool PageViewManager::Save()
    {
        for (auto& [managerHash, manager] : s_Managers)
        {
            for (auto& view : manager->m_Views)
            {
                view->Save();
            }
        }

        return true;
    }

    bool PageViewManager::Clear()
    {
        s_Managers.clear();
        return true;
    }

    bool PageViewManager::OnAppClose(Ref<Project> _Project)
    {
        for (auto& [managerHash, manager] : s_Managers)
        {
            // TODO: May need add function OnAppClose to better handle close event
            manager->m_Views.clear();
        }

        return true;
    }

    Ref<PageViewManager> PageViewManager::GetPdfOcr()
    {
        s_CurrentManagerHash = kHashPdfOcr;
        if (!s_Managers.contains(kHashPdfOcr))
        {
            s_Managers[kHashPdfOcr] = CreateRef<PageViewManager>();
            s_Managers[kHashPdfOcr]->m_Views.emplace_back(CreateRef<RawImgPageView>());
            s_Managers[kHashPdfOcr]->m_Views.emplace_back(CreateRef<CutByPatternImgPageView>());
            s_Managers[kHashPdfOcr]->m_Views.emplace_back(CreateRef<XlsxPageView>());
        }
        return s_Managers[kHashPdfOcr];
    }

    Ref<PageViewManager> PageViewManager::GetPdf()
    {
        s_CurrentManagerHash = kHashPdf;
        if (!s_Managers.contains(kHashPdf))
        {
            s_Managers[kHashPdf] = CreateRef<PageViewManager>();
        }
        return s_Managers[kHashPdf];
    }

    Ref<PageViewManager> PageViewManager::GetExcelFolder()
    {
        s_CurrentManagerHash = kHashExcelFolder;
        if (!s_Managers.contains(kHashExcelFolder))
        {
            s_Managers[kHashExcelFolder] = CreateRef<PageViewManager>();
            s_Managers[kHashExcelFolder]->m_Views.emplace_back(CreateRef<XlsxImgSyncPageView>());
            s_Managers[kHashExcelFolder]->m_Views.emplace_back(CreateRef<XlsxPageView>());
        }
        return s_Managers[kHashExcelFolder];
    }

    Ref<PageViewManager> PageViewManager::GetCurrent() { return s_Managers[s_CurrentManagerHash]; }

    void PageViewManager::DrawMenuItem() { }

    void PageViewManager::DrawViewTopMenu()
    {
        const int filesCount = GetPagesCountByManager(m_Project, s_CurrentManagerHash);
        const int maxPageId = glm::max(filesCount - 1, 0);

        ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, kTopMenuFramePaddingY);

        // ImVec2 buttonSize = { ImGui::GetFontSize() * kBntSizeCoef, ImGui::GetFontSize() * kBntSizeCoef };

        ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, kTopMenuButtonLargeFramePaddingX);
        int newPageId = m_PageId;
        if (ImGui::Button("<<"))
        {
            newPageId = 0;
        }
        ImGui::PopStyleVar();

        ImGui::SameLine();

        ImGui::PushButtonRepeat(true);
        ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, kTopMenuButtonSmallFramePaddingX);
        if (ImGui::Button("<"))
        {
            --newPageId;
        }
        ImGui::PopStyleVar();
        ImGui::PopButtonRepeat();

        ImGui::SameLine();

        ImGui::SetNextItemWidth(ImGui::GetFontSize() * 10.0f);
        ImGui::DragInt("##PageIdInput", &newPageId, 0.01f, 0, maxPageId);
        ImGui::SameLine();

        ImGui::PushButtonRepeat(true);
        ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, kTopMenuButtonSmallFramePaddingX);
        if (ImGui::Button(">"))
        {
            ++newPageId;
        }
        ImGui::PopStyleVar();
        ImGui::PopButtonRepeat();

        ImGui::SameLine();

        ImGui::PushStyleVarX(ImGuiStyleVar_FramePadding, kTopMenuButtonLargeFramePaddingX);
        if (ImGui::Button(">>"))
        {
            newPageId = maxPageId;
        }
        ImGui::PopStyleVar();

        newPageId = glm::clamp(newPageId, 0, maxPageId);

        if (newPageId != m_PageId)
        {
            bool isCanChangePage = true;
            for (auto& view : m_Views)
            {
                // TODO: Add handle for cases, where page can't be changed (overlay or window with error in front)
                isCanChangePage = isCanChangePage && view->OnPageWillBeChanged(m_PageId, newPageId);
            }

            if (isCanChangePage)
            {
                m_PageId = newPageId;
            }
        }

        ImGui::PopStyleVar();
    }

    void PageViewManager::DrawViews(Ref<Project> _Project)
    {
        if (_Project == Project::s_ProjectNotOpen)
        {
            return;
        }

        m_Project = _Project;

        for (size_t i = 0; i < m_Views.size(); ++i)
        {
            ImGui::PushID(static_cast<int>(i));
            m_Views[i]->SetContext(m_Project, m_PageId);
            m_Views[i]->Draw();
            m_Views[i]->DrawOtherWindows();
            ImGui::PopID();
        }

        m_Project = Project::s_ProjectNotOpen;
    }

    int PageViewManager::SetPage(int _PageId)
    {
        const int filesCount = GetPagesCountByManager(m_Project, s_CurrentManagerHash);
        const int maxPageId = glm::max(filesCount - 1, 0);

        m_PageId = glm::clamp(_PageId, 0, maxPageId);

        return m_PageId;
    }

}    // namespace LM
