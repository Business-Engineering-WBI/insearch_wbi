#include "XlsxImgSyncPageView.h"

#include "Engine/Textures/Texture2D.h"
#include "ImGui/Overlays/Overlay.h"
#include "Managers/TextureManager.h"
#include "PageViewManager.h"

#include "Engine/Utils/Log.hpp"
#include "Utils/FileFormat.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <glm/gtc/type_ptr.hpp>

namespace LM
{

    std::string XlsxImgSyncPageView::GetFileName() const { return FileFormat::FormatImg(m_PageId); }

    void XlsxImgSyncPageView::DrawWindowContent()
    {
        Ref<Texture2D> texture = nullptr;

        std::filesystem::path imgFilename = m_OpenedFileName;
        if (m_PageId != m_OpenedPageId || !m_OpenedFileName.empty())
        {
            std::optional<std::filesystem::path> xlsxFile =
                GetFileByIndex(m_Project->GetVariantExcelTablesHelpers().GetXlsxStartupPath(), m_PageId);
            if (xlsxFile.has_value())
            {
                std::string pageIdStr = xlsxFile.value().filename().string().substr(
                    0, xlsxFile.value().filename().string().find_first_of('_'));
                int pageId = std::stoi(pageIdStr);

                imgFilename = std::filesystem::path(m_Project->GetPdfTablesWithOcrTypeRawImgPrevPath()) /
                              std::filesystem::path(FileFormat::FormatImg(pageId + m_PageOffset));
                m_OpenedPageId = m_PageId;
                m_OpenedFileName = imgFilename.string();
            }
            else
            {
                m_OpenedPageId = -1;
                m_OpenedFileName.clear();
                return;
            }
        }

        std::string imgFilenameStr = imgFilename.string();
        if (TextureManager::Contains(imgFilenameStr))
        {
            texture = TextureManager::Get(imgFilenameStr);
        }
        else if (std::filesystem::exists(imgFilename))
        {
            texture = TextureManager::AddOrReplace(imgFilenameStr);
        }
        if (texture == Ref<Texture2D>())
        {
            return;
        }

        ImVec2 contentSize = ImGui::GetContentRegionAvail();
        float texWidth = texture->GetWidth();
        float texHeight = texture->GetHeight();
        if (texWidth > contentSize.x)
        {
            float factor = contentSize.x / texWidth;
            texWidth *= factor;
            texHeight *= factor;
        }
        if (texHeight > contentSize.y)
        {
            float factor = contentSize.y / texHeight;
            texWidth *= factor;
            texHeight *= factor;
        }

        texWidth *= m_Scale;
        texHeight *= m_Scale;

        ImGui::Image(reinterpret_cast<ImTextureID>(texture->GetTextureId()), ImVec2(texWidth, texHeight));
    }

    void XlsxImgSyncPageView::DrawTopMenuExtras()
    {
        ImGui::PushStyleVarY(ImGuiStyleVar_FramePadding, PageViewManager::kTopMenuFramePaddingY);

        ImGui::SameLine();

        ImGui::SetNextItemWidth(100.0f);
        if (ImGui::DragInt("Page Offset", &m_PageOffset, 0.01f))
        {
            m_OpenedPageId = -1;
            m_OpenedFileName.clear();
        }

        ImGui::SameLine();

        ImGui::SetNextItemWidth(100.0f);
        ImGui::DragFloat("Scale", &m_Scale, 0.01f, 1.0f, 10.0f);

        ImGui::PopStyleVar();
    }

    void XlsxImgSyncPageView::DrawExtras() { }

    std::optional<std::filesystem::path> XlsxImgSyncPageView::GetFileByIndex(const std::filesystem::path& _Folder,
                                                                             std::size_t _Index)
    {
        if (!std::filesystem::is_directory(_Folder))
        {
            return std::nullopt;
        }

        std::vector<std::filesystem::path> files;
        for (const auto& entry : std::filesystem::directory_iterator(_Folder))
        {
            if (entry.is_regular_file())
            {
                files.push_back(entry.path());
            }
        }
        std::sort(files.begin(), files.end());

        if (_Index >= files.size())
        {
            return std::nullopt;
        }

        return files[_Index];
    }

}    // namespace LM
