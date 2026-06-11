#include "IImgPageView.h"

#include "Managers/TextureManager.h"
#include "Utils/FileFormat.h"

#include <imgui.h>
#include <imgui_internal.h>

#include <filesystem>

namespace LM
{

    std::string IImgPageView::GetFileName() const { return FileFormat::FormatImg(m_PageId); }

    void IImgPageView::DrawWindowContent()
    {
        const ImU32 kColorWhite = ImGui::GetColorU32(IM_COL32(255, 255, 255, 255));
        const ImU32 kColorRed = ImGui::GetColorU32(IM_COL32(255, 0, 0, 255));

        Ref<Texture2D> texture = nullptr;
        // TODO: fix

        const std::string imgFilename =
            (std::filesystem::path(m_BasePath) / std::filesystem::path(GetFileName())).string();
        if (TextureManager::Contains(imgFilename))
        {
            texture = TextureManager::Get(imgFilename);
        }
        else if (std::filesystem::exists(imgFilename))
        {
            texture = TextureManager::AddOrReplace(imgFilename);
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

        ImVec2 texStartPos = ImGui::GetCursorScreenPos();
        m_ImgStart = { texStartPos.x, texStartPos.y };
        m_ImgEnd = m_ImgStart + glm::vec2(texWidth, texHeight);
        ImGui::Image(reinterpret_cast<ImTextureID>(texture->GetTextureId()), ImVec2(texWidth, texHeight));
        bool isImageHover = ImGui::IsItemHovered();

        ImDrawList* drawList = ImGui::GetWindowDrawList();
        m_ImgDrawList = drawList;

        if (isImageHover)
        {
            ImGui::BeginTooltip();
            drawList = ImGui::GetWindowDrawList();

            ImVec2 tooltipStartPos = ImGui::GetCursorScreenPos();
            ImGuiIO& io = ImGui::GetIO();

            float regionSize = glm::min(100.0f, glm::min(texWidth, texHeight));
            float regionDrawSize = ImGui::GetFontSize() * 16.0f;
            float regionX = glm::clamp(io.MousePos.x - texStartPos.x - regionSize * 0.5f, 0.0f, texWidth - regionSize);
            float regionY = glm::clamp(io.MousePos.y - texStartPos.y - regionSize * 0.5f, 0.0f, texHeight - regionSize);
            ImVec2 uv0 = ImVec2((regionX) / texWidth, (regionY) / texHeight);
            ImVec2 uv1 = ImVec2((regionX + regionSize) / texWidth, (regionY + regionSize) / texHeight);
            ImGui::Image(reinterpret_cast<ImTextureID>(texture->GetTextureId()), ImVec2(regionDrawSize, regionDrawSize),
                         uv0, uv1, { 1.0f, 1.0f, 1.0f, 1.0f }, { 1.0f, 1.0f, 1.0f, 0.5f });

            float regionSizeKoef = regionSize / regionDrawSize;
            ImVec2 cursorPosInTex { (io.MousePos.x - texStartPos.x - regionX) / regionSize * regionDrawSize,
                                    (io.MousePos.y - texStartPos.y - regionY) / regionSize * regionDrawSize };

            ImGui::Text("Min: (%.2f, %.2f)", regionX, regionY);
            ImGui::SameLine();
            ImGui::Text("Max: (%.2f, %.2f)", regionX + regionSize, regionY + regionSize);
            ImGui::Text("cursorPosInTex: (%.2f, %.2f)", cursorPosInTex.x, cursorPosInTex.y);

            ImVec2 circleCenter = { tooltipStartPos.x + cursorPosInTex.x, tooltipStartPos.y + cursorPosInTex.y };

            float circleRadius = GetCircleRadius();

            drawList->AddCircle(circleCenter, circleRadius, kColorWhite, 0, kLineThickness + 2.0f);
            drawList->AddCircle(circleCenter, circleRadius, kColorRed, 0, kLineThickness);

            ImGui::EndTooltip();
        }
    }

    float IImgPageView::GetCircleRadius() const { return glm::clamp(ImGui::GetFontSize() / 4.0f, 6.0f, 16.0f); }

}    // namespace LM
