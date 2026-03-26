#include "XlsxPageView.hpp"

#include "Engine/ImGui/ImGuiButtonColored.hpp"
#include "Engine/Layers/ImGuiLayer.h"
#include "Engine/Utils/Utf8Extras.hpp"
#include "Engine/Utils/utf8.h"
#include "ImGui/Configs/SharedConnectionConfigSetup.hpp"
#include "ImGui/Configs/Yg1ShopConstructionLevelConfigSetup.hpp"
#include "ImGui/Overlays/Overlay.h"
#include "ImGui/Overlays/ScriptPopup.h"
#include "Managers/TextureManager.h"
#include "Project/Processing/XlsxPageViewData.h"
#include "Python/PythonCommand.h"
#include "Utils/FileFormat.h"

#include "Engine/Utils/Log.hpp"
#include "Engine/Utils/json.hpp"
#include "Utils/MakeScreenshot.hpp"

#include "glm/common.hpp"
#include "glm/fwd.hpp"
#include <imgui.h>
#include <imgui_internal.h>
#include <imstb_textedit.h>
#include <misc/cpp/imgui_stdlib.h>
#include <random>
#include <unordered_map>
#include <xlnt/xlnt.hpp>

#include <array>
#include <exception>
#include <filesystem>
#include <format>
#include <fstream>
#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

NLOHMANN_JSON_NAMESPACE_BEGIN
template <typename T>
struct adl_serializer<std::optional<T>>
{
    static void to_json(json& j, const std::optional<T>& opt)
    {
        if (opt == std::nullopt)
        {
            j = nullptr;
        }
        else
        {
            j = *opt;    // this will call adl_serializer<T>::to_json which will
                         // find the free function to_json in T's namespace!
        }
    }

    static void from_json(const json& j, std::optional<T>& opt)
    {
        if (j.is_null())
        {
            opt = std::nullopt;
        }
        else
        {
            opt = j.template get<T>();    // same as above, but with
                                          // adl_serializer<T>::from_json
        }
    }
};
NLOHMANN_JSON_NAMESPACE_END

bool CustomPassFilter(const ImGuiTextFilter& _TextFilter, std::string_view text)
{
    if (_TextFilter.Filters.Size == 0)
    {
        return true;
    }

    for (const ImGuiTextFilter::ImGuiTextRange& f : _TextFilter.Filters)
    {
        if (f.b == f.e)
        {
            continue;
        }

        std::string filterStr(f.b, f.e);
        std::string textStr(text);
        utf8upr(reinterpret_cast<utf8_int8_t*>(filterStr.data()));
        utf8upr(reinterpret_cast<utf8_int8_t*>(textStr.data()));
        if (textStr.find(filterStr) != std::string::npos)
        {
            return true;
        }
    }

    // // Implicit * grep
    // if (CountGrep == 0)
    // {
    //     return true;
    // }

    return false;
}

namespace LM
{
    using namespace std::literals;

    constexpr std::string_view kDefaultConstructionsTreeFile = "assets/constructions/constructions_tree.json";
    constexpr std::string_view kDefaultConstructionsFieldsFile = "assets/constructions/ctd_fields.json";
    constexpr std::string_view kDefaultFieldsDescriptionFile = "assets/constructions/gen_fields.json";
    constexpr std::string_view kDefaultRepresentationFieldsDescriptionFile = "assets/constructions/repr_fields.json";
    constexpr std::string_view kDefaultAmatiCodemsFile = "assets/data/amati_codem.xlsx";

    const std::vector<std::string> kProductBaseFields = { "fulldescription", "lcs", "moq", "codem" };
    const std::vector<std::string> kImgFileTypeList = { "pic", "drw" };

    const std::array kInterpModelSuffixesToRemove = { "_AMATI"sv, "_ASKUP"sv, "_DEREK"sv, "_HT"sv,
                                                      "_PL"sv,    "_LIK"sv,   "_ТИЗ"sv };

    static const std::array kInterpModelLettersToRemove = {
        std::pair {  " "sv,  ""sv },
         std::pair { " "sv,  ""sv },
         std::pair {  "-"sv,  ""sv },
         std::pair {  "."sv, ","sv },
        std::pair {  "*"sv,  ""sv },
         std::pair {  "^"sv,  ""sv },
         std::pair { "\\"sv,  ""sv },
         std::pair {  "/"sv,  ""sv },
        std::pair { "К"sv, "K"sv },
         std::pair { "Е"sv, "E"sv },
         std::pair { "Н"sv, "H"sv },
         std::pair { "Х"sv, "X"sv },
        std::pair { "В"sv, "B"sv },
         std::pair { "А"sv, "A"sv },
         std::pair { "Р"sv, "P"sv },
         std::pair { "О"sv, "O"sv },
        std::pair { "С"sv, "C"sv },
         std::pair { "М"sv, "M"sv },
         std::pair { "Т"sv, "T"sv },
    };

    const std::string InterpolateModel(std::string_view _Model)
    {
        std::string result = StrTrim(_Model.data());
        utf8upr(reinterpret_cast<utf8_int8_t*>(result.data()));
        for (auto suffix : kInterpModelSuffixesToRemove)
        {
            if (result.ends_with(suffix))
            {
                if (size_t pos = result.find(suffix); pos != std::string::npos)
                {
                    result.replace(pos, suffix.size(), "");
                }
            }
        }
        for (const auto& p : kInterpModelLettersToRemove)
        {
            size_t pos = 0;
            while ((pos = result.find(p.first, pos)) != std::string::npos)
            {
                result.replace(pos, p.first.size(), p.second);
                pos += p.second.size();
            }
        }
        return result;
    }

    std::string Random64CharStr()
    {
        const std::string chars = "abcdefghijklmnopqrstuvwxyz0123456789";
        std::string result;
        result.reserve(64);    // заранее резервируем память

        std::random_device rd;     // источник энтропии
        std::mt19937 gen(rd());    // генератор случайных чисел
        std::uniform_int_distribution<size_t> dist(0, chars.size() - 1);

        for (int i = 0; i < 64; ++i)
        {
            result += chars[dist(gen)];
        }
        return result;
    }

    inline bool DeleteButton()
    {
        ImGui::PushStyleColor(ImGuiCol_Button, 0xFF0000AA);
        bool result = ImGui::Button("X");
        ImGui::PopStyleColor();

        return result;
    }

    inline ImVec4 GetFrameBgColorNone(float _Alpha = 0.125f, float _Blue = 0.0f)
    {
        ImVec4 frameBgColorNone = ImGui::GetStyleColorVec4(ImGuiCol_FrameBg);
        frameBgColorNone.w = _Alpha;
        return { frameBgColorNone.x, frameBgColorNone.y, _Blue, _Alpha };
    }
    inline ImVec4 GetFrameBgColorOk(float _Alpha = 0.125f, float _Blue = 0.0f)
    {
        return ImVec4(0.0f, 1.0f, 0.0f, _Alpha);
    }
    inline ImVec4 GetFrameBgColorWarn(float _Alpha = 0.125f, float _Blue = 0.0f)
    {
        return ImVec4(1.0f, 1.0f, 0.0f, _Alpha);
    }
    inline ImVec4 GetFrameBgColorError(float _Alpha = 0.125f, float _Blue = 0.0f)
    {
        return ImVec4(1.0f, 0.0f, 0.0f, _Alpha);
    }

    XlsxPageView::XlsxPageView()
    {
        LoadConstructionsTree();
        LoadConstructionsFields();
        LoadFieldsDescription();
        LoadRepresentationFieldsDescription();
    }

    XlsxPageView::~XlsxPageView() { }

    bool XlsxPageView::OnPageWillBeChanged(int _CurrentPageId, int _NewPageId) { return true; }

    void XlsxPageView::Save()
    {
        if (m_Project)
        {
            XlsxPageViewData& xlsxViewData = m_Project->GetXlsxPageViewData();
            xlsxViewData.SavePageData();
            xlsxViewData.SaveExtraInfoJson();
        }
    }

    std::string XlsxPageView::GetFileName() const { return FileFormat::FormatXlsx(m_PageId); }

    void XlsxPageView::DrawWindowContent()
    {
        m_IsMainWindowFocused = ImGui::IsWindowFocused();
        if (m_PageId == -1 || !m_Project)
        {
            return;
        }

        XlsxPageViewData& xlsxViewData = m_Project->GetXlsxPageViewData();

        if (!xlsxViewData.IsPageLoaded(m_PageId))
        {
            UnSelectAll();
            xlsxViewData.LoadPageData(m_PageId);
        }

        if (!xlsxViewData.IsPageLoaded(m_PageId))
        {
            ImGui::Text("No data loaded or file not found.");
            return;
        }

        XlsxPageViewPageData& pageData = xlsxViewData.GetCurrentPageData();
        XlsxPageViewDataTypes::TableData& tableData = pageData.GetTableData();

        ImGui::Text("Имя файла: %s", pageData.GetPageFilename().string().c_str());

        if (m_IsAnyHeaderActive)
        {
            m_IsAnyCellActive = true;
        }

        HandleImGuiEvents(xlsxViewData, tableData);
        // DrawTableActions();

        size_t colsCount = 0;
        if (!tableData.empty())
        {
            colsCount = tableData[0].size();
        }

        DrawGlobalAddListWindow(xlsxViewData);

        DrawSimpleAddListWindow(xlsxViewData);
        DrawSimpleCalcListWindow(xlsxViewData);
        DrawSimpleRuleImgListWindow(xlsxViewData);

        DrawImgsPerListWindow(xlsxViewData);

        DrawJoinModal(xlsxViewData, tableData);
        DrawFindAndReplaceModal(xlsxViewData, tableData);

        ImGui::Separator();

        ImGui::Checkbox("Показать примеры", &m_IsShowConstrExamples);

        ImGui::Separator();

        static ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit | ImGuiTableFlags_Borders |
                                            ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollX | ImGuiTableFlags_ScrollY;

        const float framePaddingX = 8.0f;
        const float framePaddingY = 4.0f;

        // TODO: fix with: add col width for header and for content for future select in setwidth
        std::vector<float> headerWidths(colsCount + 1, 0.0f);
        std::vector<float> columnWidths(colsCount + 1, 0.0f);
        headerWidths[0] =
            std::max(ImGui::CalcTextSize(std::to_string(tableData.size()).c_str()).x + framePaddingX * 2.0f,
                     ImGui::CalcTextSize("  . . .  ").x);

        if (tableData.size() > 0)
        {
            for (size_t colId = 0; colId < colsCount; ++colId)
            {
                const auto& cellText = tableData[0][colId].Value;
                ImVec2 cellTextSize = ImGui::CalcTextSize(std::format("  {}  ", cellText).c_str());
                headerWidths[colId + 1] = std::max(headerWidths[colId + 1], cellTextSize.x);
                // + ImGui::GetFontSize() * 2.0f
                if (std::optional<const std::reference_wrapper<std::string>> extraValue =
                        GetExtraListValue(xlsxViewData, tableData[0][colId].Value);
                    extraValue.has_value())
                {
                    std::string::difference_type textOffset =
                        std::min(extraValue->get().end() - extraValue->get().begin(), 64ll);
                    ImVec2 extraTextSize =
                        ImGui::CalcTextSize(extraValue->get().c_str(), extraValue->get().c_str() + textOffset);
                    columnWidths[colId + 1] = std::max(columnWidths[colId + 1], extraTextSize.x + framePaddingX * 2.0f);
                }
            }
        }

        for (size_t rowId = 1; rowId < tableData.size(); ++rowId)
        {
            for (size_t colId = 0; colId < colsCount; ++colId)
            {
                const auto& cellText = tableData[rowId][colId].Value;
                float cellTextSize = ImGui::CalcTextSize(cellText.c_str()).x + ImGui::CalcTextSize(" ").x;
                columnWidths[colId + 1] = std::max(columnWidths[colId + 1], cellTextSize + framePaddingX * 2.0f);
                // + ImGui::GetFontSize() * 2.0f
            }
        }

        ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, { 0.0f, 0.0f });

        if (ImGui::BeginTable("XLSX Table", static_cast<int>(colsCount + 1), tableFlags))
        {
            ImGui::TableSetupScrollFreeze(1, 1);

            ImGui::PushStyleVarY(ImGuiStyleVar_CellPadding, framePaddingY);
            m_IsAnyHeaderActive = false;
            DrawTableHeaderReturn headerData = DrawTableHeader(colsCount, xlsxViewData, tableData);
            ImGui::PopStyleVar();

            auto constrItem = xlsxViewData.GetItemInSimpleAddListForCurrentPage("constr");
            if (constrItem.has_value() && m_IsShowConstrExamples)
            {
                const auto& constr = constrItem->get().Value;
                LoadConstrExample(constr);
                if (m_ConstrExamples.contains(constr))
                {
                    auto& constrExample = m_ConstrExamples[constr];
                    size_t rowsCount = (*constrExample.begin()).second.size();

                    ImGui::PushStyleColor(ImGuiCol_TableRowBg, 0x0a00ff00);
                    ImGui::PushStyleColor(ImGuiCol_TableRowBgAlt, 0x1900ff00);
                    for (size_t rowId = 1; rowId <= rowsCount; ++rowId)
                    {
                        ImGui::TableNextRow();
                        ImGui::PushID(-static_cast<int>(rowId));
                        ImGui::TableSetColumnIndex(static_cast<int>(0));
                        ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { framePaddingX, framePaddingY });
                        ImGui::Button("##RowId", ImVec2(std::max(headerWidths[0], columnWidths[0]), 0.0f));
                        ImGui::PopStyleVar();

                        for (size_t colId = 0; colId < colsCount; ++colId)
                        {
                            ImGui::PushID(static_cast<int>(colId));
                            ImGui::TableSetColumnIndex(static_cast<int>(colId + 1));

                            const std::string& header = tableData[0][colId].Value;
                            if (constrExample.contains(header))
                            {
                                ImGui::Text("  %s  ", constrExample[header][rowId - 1].c_str());
                                float cellTextSize = ImGui::CalcTextSize(constrExample[header][rowId - 1].c_str()).x +
                                                     ImGui::CalcTextSize("    ").x;
                                columnWidths[colId + 1] = std::max(columnWidths[colId + 1], cellTextSize);
                            }

                            ImGui::PopID();
                        }
                        ImGui::PopID();
                    }
                    ImGui::TableNextRow();
                    ImGui::PopStyleColor(2);
                }
            }

            m_IsAnyCellActive = false;
            ImDrawList* draw_list = ImGui::GetWindowDrawList();
            for (size_t rowId = 1; rowId < tableData.size(); ++rowId)
            {
                ImGui::TableNextRow();
                ImGui::PushID(static_cast<int>(rowId));
                ImGui::TableSetColumnIndex(static_cast<int>(0));

                bool isRowHovered = false;
                std::string rowIdStr = std::format("{}##RowId", rowId);

                ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { framePaddingX, framePaddingY });
                ImGui::Button(rowIdStr.c_str(), ImVec2(std::max(headerWidths[0], columnWidths[0]), 0.0f));
                ImGui::PopStyleVar();

                if (ImGui::IsItemHovered())
                {
                    isRowHovered = true;
                    // ImGui::SetTooltip("Right-click to open popup");
                }

                if (ImGui::IsItemFocused())
                {
                    UnSelectAll();
                    m_SelectedRow = rowId;
                }

                if (ImGui::BeginPopupContextItem(rowIdStr.c_str()))
                {
                    ImGui::Text("This a popup for Row:%zu", rowId);
                    if (ImGui::Button("Close"))
                    {
                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::EndPopup();
                }

                for (size_t colId = 0; colId < colsCount; ++colId)
                {
                    ImGui::PushID(static_cast<int>(colId));
                    ImGui::TableSetColumnIndex(static_cast<int>(colId + 1));

                    auto& t = tableData[rowId][colId].Value;
                    bool isColHovered = headerData.HoveredCol.has_value() && headerData.HoveredCol.value() == colId;

                    PushCellFrameBgColor(tableData, isRowHovered, isColHovered, rowId, colId);
                    ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, { framePaddingX, framePaddingY });

                    float width = std::max(headerWidths[colId + 1], columnWidths[colId + 1]);

                    if (std::optional<const std::reference_wrapper<std::string>> extraValue =
                            GetExtraListValue(xlsxViewData, tableData[0][colId].Value);
                        extraValue.has_value())
                    {
                        ImVec2 cursorPos = ImGui::GetCursorScreenPos();
                        ImVec4 clipRect(cursorPos.x + framePaddingX, cursorPos.y + framePaddingY,
                                        cursorPos.x + width - framePaddingX,
                                        cursorPos.y + ImGui::GetFontSize() + framePaddingY);
                        const ImVec2 textPos = ImVec2(cursorPos.x + framePaddingX, cursorPos.y + framePaddingY);
                        draw_list->AddText(ImGui::GetFont(), ImGui::GetFontSize(), textPos, 0xFF666666,
                                           extraValue->get().c_str(), NULL, 0.0f, &clipRect);
                    }

                    ImGui::SetNextItemWidth(width);
                    ImGui::InputText("##Input", &t, ImGuiInputTextFlags_NoHorizontalScroll);
                    ImGui::PopStyleVar();
                    ImGui::PopStyleColor();

                    ImGuiIO& io = ImGui::GetIO();

                    if (ImGui::IsItemFocused() && !ImGui::IsItemActive() && !io.InputQueueCharacters.empty())
                    {
                        // TODO:: may need to clear buffer
                        for (unsigned short c : io.InputQueueCharacters)
                        {
                            if (c >= 32)
                            {
                                t.push_back((char)c);
                            }
                        }
                        io.InputQueueCharacters.resize(0);

                        ImGui::ActivateItemByID(ImGui::GetItemID());
                    }

                    if (ImGui::IsItemActivated() && ImGui::IsItemEdited() == false)
                    {
                        ImGuiInputTextState* state = ImGui::GetInputTextState(ImGui::GetItemID());
                        if (state)
                        {
                            state->CursorAnimReset();    // сброс мигания
                            state->CursorClamp();    // убедиться, что курсор в допустимых границах
                            state->ClearSelection();
                        }
                    }

                    if (ImGui::IsItemActive())
                    {
                        m_IsAnyCellActive = true;
                    }
                    if (ImGui::IsItemDeactivatedAfterEdit())
                    {
                        xlsxViewData.PushHistory();
                    }

                    if (ImGui::IsItemFocused() && ImGui::IsKeyPressed(ImGuiKey_F2))
                    {
                        ImGui::ActivateItemByID(ImGui::GetItemID());
                    }

                    if (ImGui::IsItemActive() || ImGui::IsItemFocused())
                    {
                        if (!m_SelectedCell.has_value() || (m_SelectedCell->x != colId) || (m_SelectedCell->y != rowId))
                        {
                            UnSelectAll();
                            m_SelectedCell = { colId, rowId };
                        }
                    }
                    if (ImGui::IsItemHovered() && ImGui::IsMouseDown(ImGuiMouseButton_Middle))
                    {
                        if (ImGui::GetIO().KeyShift && m_SelectedCell.has_value())
                        {
                            m_ExtraSelectedCell = { colId, rowId };
                        }
                        else
                        {
                            UnSelectAll();
                            m_SelectedCell = { colId, rowId };
                            ImGui::ClearActiveID();
                            ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
                            ImGui::SetWindowFocus();
                        }
                        // ImGui::SetTooltip("Right-click to open popup");
                    }
                    if (ImGui::BeginPopupContextItem())
                    {
                        ImGui::Text("This a popup for Col: %zu Row:%zu", colId, rowId);
                        if (ImGui::Button("Close"))
                        {
                            ImGui::CloseCurrentPopup();
                        }
                        ImGui::EndPopup();
                    }

                    ImGui::PopID();
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
        ImGui::PopStyleVar();

        if (m_DeleteCol.has_value())
        {
            pageData.DeleteCol(*m_DeleteCol);
            m_DeleteCol = std::nullopt;
        }
        if (m_DeleteRow.has_value())
        {
            pageData.DeleteRow(*m_DeleteRow);
            m_DeleteRow = std::nullopt;
        }
    }

    void XlsxPageView::DrawOtherWindows()
    {
        DrawProcessingScriptsWindow();
        DrawImgsToSkipBgRemoveWindow();
    }

    void XlsxPageView::DrawTableActions(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData)
    {
        ImGui::SeparatorText("Действия с таблицей");

        if (ImGui::Button("Копировать без заголовка"))
        {
            std::string copyText;
            for (size_t rowId = 1; rowId < _TableData.size(); ++rowId)
            {
                for (size_t colId = 0; colId < _TableData[rowId].size(); ++colId)
                {
                    copyText += _TableData[rowId][colId].Value + "\t";
                }
                copyText += "\n";
            }
            ImGui::SetClipboardText(copyText.c_str());
        }
        ImGui::SameLine();
        if (ImGui::Button("Копировать с заголовком"))
        {
            std::string copyText;
            for (size_t colId = 0; colId < _TableData[0].size(); ++colId)
            {
                copyText += _TableData[0][colId].Value + "\t";
            }
            copyText += "\n";
            for (size_t rowId = 1; rowId < _TableData.size(); ++rowId)
            {
                for (size_t colId = 0; colId < _TableData[rowId].size(); ++colId)
                {
                    copyText += _TableData[rowId][colId].Value + "\t";
                }
                copyText += "\n";
            }
            ImGui::SetClipboardText(copyText.c_str());
        }

        // TODO: Move to separate function
        if (ImGui::Button("Вставить из буфера (заменить все)"))
        {
            ReplaceFromClipboard(_XlsxViewData, _TableData, false);
        }
        ImGui::SameLine();
        if (ImGui::Button("Вставить из буфера (пустая строка заголовка)"))
        {
            ReplaceFromClipboard(_XlsxViewData, _TableData, true);
        }

        // TODO: Move to separate function
        if (ImGui::Button("Test Fix"))
        {
            for (auto& row : _TableData)
            {
                if (!row.empty())
                {
                    auto& cell = row[0].Value;
                    cell.erase(
                        std::remove_if(cell.begin(), cell.end(), [](unsigned char ch) { return std::isspace(ch); }),
                        cell.end());
                }
            }

            for (auto& row : _TableData)
            {
                for (auto& cell : row)
                {
                    std::string& cellValue = cell.Value;

                    std::string::size_type pos;
                    while ((pos = cellValue.find(" ~")) != std::string::npos)
                    {
                        cellValue.replace(pos, 2, "~");
                    }
                    while ((pos = cellValue.find("~ ")) != std::string::npos)
                    {
                        cellValue.replace(pos, 2, "~");
                    }
                    cellValue.erase(
                        std::remove_if(cellValue.begin(), cellValue.end(), [](unsigned char ch) { return ch >= 0x80; }),
                        cellValue.end());
                    cellValue.erase(cellValue.begin(),
                                    std::find_if(cellValue.begin(), cellValue.end(),
                                                 [](unsigned char ch) { return !std::isspace(ch); }));
                    cellValue.erase(std::find_if(cellValue.rbegin(), cellValue.rend(),
                                                 [](unsigned char ch) { return !std::isspace(ch); })
                                        .base(),
                                    cellValue.end());
                    cellValue.erase(std::unique(cellValue.begin(), cellValue.end(),
                                                [](char a, char b) { return std::isspace(a) && std::isspace(b); }),
                                    cellValue.end());
                }
            }

            _XlsxViewData.PushHistory();
        }

        ImGui::SameLine();

        if (ImGui::Button("Разделить столбцы по пробелу"))
        {
            SplitAndExpandTable(_XlsxViewData, _TableData);
        }

        ImGui::SeparatorText("Вставка данных из внешних источников");

        if (ImGui::Button("Вставить артикул AMATI"))
        {
            FindAndInsertAmatiCodems(_XlsxViewData, _TableData);
        }

        ImGui::SeparatorText("Конструкции");

        if (ImGui::Button("Изменить конструкцию"))
        {
            ImGui::OpenPopup("Изменить конструкцию");
        }
        if (ImGui::BeginPopup("Изменить конструкцию"))
        {
            static ImGuiTextFilter constrFilter;
            constrFilter.Draw("Фильтрация конструкции");

            ImGui::BeginChild("ConstrList", ImVec2(0.0f, ImGui::GetFontSize() * 24.0f),
                              ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_NavFlattened);
            for (size_t i = 0; i < m_Constructions.size(); ++i)
            {
                const auto& constr = m_Constructions[i];
                if (CustomPassFilter(constrFilter, constr.Label.c_str()) ||
                    CustomPassFilter(constrFilter, constr.Key.c_str()))
                {
                    ImGui::PushID(static_cast<int>(i));
                    if (ImGui::Selectable(std::format("{}\n{}", constr.Label, constr.Key).c_str(), false))
                    {
                        ChangeHeadersByConstruction(_XlsxViewData, _TableData, constr.Key);

                        std::string fieldName = "constr";
                        _XlsxViewData.DeleteFromSimpleAddListForCurrentPage(fieldName);
                        XlsxPageViewDataTypes::SimpleAddList& simpleAddList = _XlsxViewData.GetSimpleAddList();
                        simpleAddList.try_emplace(fieldName);
                        simpleAddList[fieldName].push_back({ { { _XlsxViewData.GetCurrentPageId() } }, constr.Key });
                        _XlsxViewData.SaveExtraInfoJson();

                        ImGui::CloseCurrentPopup();
                    }
                    ImGui::PopID();
                    ImGui::Spacing();
                }
            }
            ImGui::EndChild();

            if (ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }
    }

    void XlsxPageView::DrawGlobalAddListWindow(XlsxPageViewData& _XlsxViewData)
    {
        std::string windowName = "Глобальный список заполнения";
        if (ImGui::Begin(windowName.c_str()))
        {
            std::string toDeleteName;

            XlsxPageViewDataTypes::GlobalAddList& globalAddList = _XlsxViewData.GetGlobalAddList();

            for (auto& [globalAddListFieldName, globalAddListFieldValue] : globalAddList)
            {
                ImGui::PushID(globalAddListFieldName.c_str());
                ImGui::Text("%s", globalAddListFieldName.c_str());

                if (IsExtraInfoAutoFocusField(windowName, globalAddListFieldName))
                {
                    ImGui::SetKeyboardFocusHere();
                }
                const auto& style = ImGui::GetStyle();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("X").x -
                                        style.ItemSpacing.x - style.FramePadding.x * 2.0f);
                ImGui::InputText(std::format("##{}", globalAddListFieldName).c_str(), &globalAddListFieldValue);
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    _XlsxViewData.PushHistory();
                    _XlsxViewData.SaveExtraInfoJson();
                }

                ImGui::SameLine();
                if (DeleteButton())
                {
                    toDeleteName = globalAddListFieldName;
                }

                ImGui::Spacing();
                ImGui::PopID();
            }

            bool isFirstTimeOpened = false;
            if (ImGui::Button("Добавить поле"))
            {
                ImGui::OpenPopup("Добавление поля##GlobalAddList");
                isFirstTimeOpened = true;
            }

            if (ImGui::BeginPopup("Добавление поля##GlobalAddList"))
            {
                static ImGuiTextFilter fieldsFilter;
                if (isFirstTimeOpened)
                {
                    ImGui::SetKeyboardFocusHere();
                }
                fieldsFilter.Draw("Фильтрация полей##GlobalAddList");

                ImGui::BeginChild("GlobalAddListField", ImVec2(0.0f, ImGui::GetFontSize() * 24.0f),
                                  ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_NavFlattened);
                for (const auto& [fieldName, fieldDescr] : m_FieldsDescription)
                {
                    if (globalAddList.contains(fieldName))
                    {
                        continue;
                    }

                    if (fieldsFilter.PassFilter(fieldDescr.Description.c_str()) ||
                        fieldsFilter.PassFilter(fieldName.c_str()))
                    {
                        ImGui::PushID(fieldName.c_str());
                        if (ImGui::Selectable(std::format("{}\n{}", fieldDescr.Description, fieldName).c_str(), false))
                        {
                            globalAddList[fieldName] = "";
                            m_ExtraInfoAutoFocusField = ExtraInfoAutoFocusField {
                                .WindowName = windowName,
                                .Field = fieldName,
                            };
                            fieldsFilter.Clear();
                            ImGui::CloseCurrentPopup();
                            _XlsxViewData.SaveExtraInfoJson();
                        }
                        ImGui::PopID();
                        ImGui::Spacing();
                    }
                }
                ImGui::EndChild();

                if (ImGui::Button("Close"))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            if (!toDeleteName.empty())
            {
                globalAddList.erase(toDeleteName);
            }
        }
        ImGui::End();
    }

    template <XlsxPageViewDataTypes::DerivedFromSimpleListItemBase T>
    void XlsxPageView::DrawSimpleListTemplateWindow(std::string_view _WindowName, XlsxPageViewData& _XlsxViewData,
                                                    std::unordered_map<std::string, std::vector<T>>& _SimpleList,
                                                    std::function<void(std::string_view, T&)> _ItemInputHandle,
                                                    std::function<std::string(const T&)> _ItemPreviewTextFn)
    {
        std::string toDeleteName;

        for (auto& [simpleListFieldName, simpleListPages] : _SimpleList)
        {
            ImGui::PushID(simpleListFieldName.c_str());
            for (T& simpleListItem : simpleListPages)
            {
                const std::vector<int>& sharedPages = simpleListItem.SharedPages;
                if (std::ranges::find(sharedPages, _XlsxViewData.GetCurrentPageId()) == sharedPages.end())
                {
                    continue;
                }

                ImGui::Text("%s", simpleListFieldName.c_str());
                ImGui::SameLine();
                std::string sharedPagesStr = StrJoin(sharedPages, ", ");
                ImGui::TextDisabled("%s", sharedPagesStr.c_str());

                if (IsExtraInfoAutoFocusField(_WindowName, simpleListFieldName))
                {
                    ImGui::SetKeyboardFocusHere();
                }
                _ItemInputHandle(simpleListFieldName, simpleListItem);
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    _XlsxViewData.PushHistory();
                    _XlsxViewData.SaveExtraInfoJson();
                }

                ImGui::SameLine();
                if (DeleteButton())
                {
                    toDeleteName = simpleListFieldName;
                }

                ImGui::Spacing();

                break;
            }
            ImGui::PopID();
        }

        std::string popupName = std::format("Добавление поля##{}", _WindowName);

        bool isFirstTimeOpened = false;
        if (ImGui::Button("Добавить поле"))
        {
            ImGui::OpenPopup(popupName.c_str());
            isFirstTimeOpened = true;
        }

        if (ImGui::BeginPopup(popupName.c_str()))
        {
            static ImGuiTextFilter fieldsFilter;
            if (isFirstTimeOpened)
            {
                ImGui::SetKeyboardFocusHere();
            }
            fieldsFilter.Draw("Фильтрация полей");

            ImGui::BeginChild("SimpleListField", ImVec2(0.0f, ImGui::GetFontSize() * 24.0f),
                              ImGuiChildFlags_AutoResizeX | ImGuiChildFlags_NavFlattened);
            for (const auto& [fieldName, fieldDescr] : m_FieldsDescription)
            {
                if (_XlsxViewData.IsItemInSimpleListForCurrentPage(_SimpleList, fieldName))
                {
                    continue;
                }

                if (fieldsFilter.PassFilter(fieldDescr.Description.c_str()) ||
                    fieldsFilter.PassFilter(fieldName.c_str()))
                {
                    ImGui::PushID(fieldName.c_str());
                    if (ImGui::Selectable(std::format("{}\n{}", fieldDescr.Description, fieldName).c_str(), false))
                    {
                        _SimpleList.try_emplace(fieldName);
                        _SimpleList[fieldName].push_back({ { { _XlsxViewData.GetCurrentPageId() } }, {} });
                        m_ExtraInfoAutoFocusField = ExtraInfoAutoFocusField {
                            .WindowName = _WindowName.data(),
                            .Field = fieldName,
                        };
                        fieldsFilter.Clear();
                        ImGui::CloseCurrentPopup();
                        _XlsxViewData.SaveExtraInfoJson();
                    }

                    if (_SimpleList.contains(fieldName))
                    {
                        for (T& simpleListItem : _SimpleList[fieldName])
                        {
                            std::vector<int>& sharedPages = simpleListItem.SharedPages;
                            std::string sharedPagesStr = StrJoin(sharedPages, ", ");

                            ImGui::Text("\t");
                            ImGui::SameLine();
                            ImGui::BeginGroup();
                            ImGui::SetNextItemAllowOverlap();
                            if (ImGui::Selectable(std::format("{}\n\t##{}", _ItemPreviewTextFn(simpleListItem),
                                                              static_cast<void*>(&simpleListItem))
                                                      .c_str(),
                                                  false))
                            {
                                sharedPages.push_back(_XlsxViewData.GetCurrentPageId());
                                std::ranges::sort(sharedPages);
                                fieldsFilter.Clear();
                                ImGui::CloseCurrentPopup();
                                _XlsxViewData.SaveExtraInfoJson();
                            }
                            ImGui::SetCursorPosY(ImGui::GetCursorPosY() - ImGui::GetTextLineHeightWithSpacing());
                            ImGui::TextDisabled("%s", sharedPagesStr.c_str());
                            ImGui::EndGroup();
                        }
                    }

                    ImGui::PopID();
                    ImGui::Spacing();
                }
            }
            ImGui::EndChild();

            if (ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        _XlsxViewData.DeleteFromSimpleListForCurrentPage(_SimpleList, toDeleteName);
    }

    void XlsxPageView::DrawSimpleAddListWindow(XlsxPageViewData& _XlsxViewData)
    {
        std::string windowTitle = "Постраничный список заполнения";
        std::function<void(std::string_view, XlsxPageViewDataTypes::SimpleAddListItem&)> handle =
            [this](std::string_view _SimpleListFieldName, XlsxPageViewDataTypes::SimpleAddListItem& _SimpleListItem) {
                const auto& style = ImGui::GetStyle();
                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("X").x -
                                        style.ItemSpacing.x - style.FramePadding.x * 2.0f);

                if (m_FieldsRepresentation.contains(_SimpleListFieldName.data()))
                {
                    const auto& items = m_FieldsRepresentation[_SimpleListFieldName.data()];

                    std::string displayName = _SimpleListItem.Value;
                    for (const auto& item : items)
                    {
                        if (item.Key == _SimpleListItem.Value)
                        {
                            displayName = std::format("{} ({})", item.Key, item.Ru);
                            break;
                        }
                    }

                    if (ImGui::BeginCombo("##Combo", displayName.c_str()))
                    {
                        for (const auto& item : items)
                        {
                            const bool is_selected = (_SimpleListItem.Value == item.Key);
                            if (ImGui::Selectable(std::format("{} ({})", item.Key, item.Ru).c_str(), is_selected))
                            {
                                _SimpleListItem.Value = item.Key;
                            }

                            if (is_selected)
                            {
                                ImGui::SetItemDefaultFocus();
                            }
                        }
                        ImGui::EndCombo();
                    }
                }
                else
                {
                    ImGui::InputText(std::format("##{}", _SimpleListFieldName).c_str(), &_SimpleListItem.Value);
                }
            };

        if (ImGui::Begin(windowTitle.c_str()))
        {
            std::function<std::string(const XlsxPageViewDataTypes::SimpleAddListItem&)> previewTextFn =
                [](const XlsxPageViewDataTypes::SimpleAddListItem& _SimpleListItem) { return _SimpleListItem.Value; };
            DrawSimpleListTemplateWindow(windowTitle, _XlsxViewData, _XlsxViewData.GetSimpleAddList(), handle,
                                         previewTextFn);
            ImGui::End();
        }
    }

    void XlsxPageView::DrawSimpleCalcListWindow(XlsxPageViewData& _XlsxViewData)
    {
        std::string windowTitle = "Постраничный список рассчета";
        std::function<void(std::string_view, XlsxPageViewDataTypes::SimpleAddListItem&)> handle =
            [](std::string_view _SimpleListFieldName, XlsxPageViewDataTypes::SimpleAddListItem& _SimpleListItem) {
                const auto& style = ImGui::GetStyle();
                float height = ImGui::GetTextLineHeight() *
                                   static_cast<float>(std::ranges::count(_SimpleListItem.Value, '\n') + 1) +
                               style.FramePadding.y * 2;

                ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x - ImGui::CalcTextSize("X").x -
                                        style.ItemSpacing.x - style.FramePadding.x * 2.0f);

                bool isFontPushed = ImGuiLayer::Get()->PushConsolasFont();
                ImGui::InputTextMultiline(std::format("##{}", _SimpleListFieldName).c_str(), &_SimpleListItem.Value,
                                          { 0.0f, height });
                if (isFontPushed)
                {
                    ImGui::PopFont();
                }
            };

        if (ImGui::Begin(windowTitle.c_str()))
        {
            ImGui::Text(
                "Python code. Example: result.append(df['bsg'][i].replace(' ', '_') + ':N' + str(df['dcon'][i]))");
            ImGui::Separator();

            std::function<std::string(const XlsxPageViewDataTypes::SimpleAddListItem&)> previewTextFn =
                [](const XlsxPageViewDataTypes::SimpleAddListItem& _SimpleListItem) { return _SimpleListItem.Value; };
            DrawSimpleListTemplateWindow(windowTitle, _XlsxViewData, _XlsxViewData.GetSimpleCalcList(), handle,
                                         previewTextFn);
        }
        ImGui::End();
    }

    bool XlsxPageView::PageImgListItemValueFilenameExists(XlsxPageViewData& _XlsxViewData, std::string_view _Hash)
    {
        for (auto& [key, list] : _XlsxViewData.GetSimpleRuleImgList())
        {
            for (auto& page : list)
            {
                for (auto& v : page.Value)
                {
                    if (v.ImgFilenameHash == _Hash)
                    {
                        return true;
                    }
                }
            }
        }

        return false;
    }

    void XlsxPageView::DrawSimpleRuleImgListWindow(XlsxPageViewData& _XlsxViewData)
    {
        std::string windowTitle = "Картинки по условию на странице";

        std::function<void(std::string_view, XlsxPageViewDataTypes::PageImgListItem&)> handle =
            [&](std::string_view _SimpleListFieldName, XlsxPageViewDataTypes::PageImgListItem& _SimpleListItem) {
                // ImGui::Text("%s", Random64CharStr().data());
                // TODO: Add Cmp

                if (ImGui::Button("Добавить новое правило"))
                {
                    std::string newFileName;
                    do
                    {
                        newFileName = Random64CharStr();
                    }
                    while (PageImgListItemValueFilenameExists(_XlsxViewData, newFileName));
                    _SimpleListItem.Value.emplace_back(
                        XlsxPageViewDataTypes::PageImgListItemValue { .ImgFilenameHash = newFileName });
                }
                ImGui::Separator();

                for (auto& v : _SimpleListItem.Value)
                {
                    ImGui::PushID(v.ImgFilenameHash.c_str());

                    ImGui::Text("Сравнение по: %s", v.Cmp.c_str());

                    ImGui::SetNextItemWidth(ImGui::GetContentRegionAvail().x);
                    ImGui::InputText("##Value", &v.CmpValue);

                    for (std::string_view filetype : kImgFileTypeList)
                    {
                        ImGui::PushID(filetype.data());

                        ImGui::SeparatorText(filetype == "pic" ? "Фото" : "Чертеж");

                        std::string imgFilename =
                            GetSimpleRuleImgFilename(std::format("{}_{}.png", v.ImgFilenameHash, filetype));
                        DrawImgPreview(imgFilename);
                        DrawImgMakeScreenshot(imgFilename);

                        ImGui::PopID();
                    }

                    ImGui::PopID();
                    ImGui::Separator();
                }

                // float height =
                //     ImGui::GetTextLineHeight() * static_cast<float>(std::ranges::count(_SimpleListItem.Value,
                //     '\n') + 1)
                //     + ImGui::GetStyle().FramePadding.y * 2;
                // ImGui::InputTextMultiline(std::format("##{}", _SimpleListFieldName).c_str(),
                // &_SimpleListItem.Value,
                //                           { 0.0f, height });
                // ImGui::InputText(std::format("##{}", _SimpleListFieldName).c_str(), &_SimpleListItem.Value);
            };

        std::function<std::string(const XlsxPageViewDataTypes::PageImgListItem&)> previewTextFn =
            [](const XlsxPageViewDataTypes::PageImgListItem& _SimpleListItem) { return "Используются на:"; };
        if (ImGui::Begin(windowTitle.c_str()))
        {
            DrawSimpleListTemplateWindow(windowTitle, _XlsxViewData, _XlsxViewData.GetSimpleRuleImgList(), handle,
                                         previewTextFn);
        }
        ImGui::End();
    }

    void XlsxPageView::DrawImgsPerListWindow(XlsxPageViewData& _XlsxViewData)
    {
        if (!m_Project)
        {
            return;
        }

        // TODO: Implement Buttons
        if (ImGui::Begin("Картинки для страницы"))
        {
            ImGui::Button("Обновить все");
            for (std::string_view filetype : kImgFileTypeList)
            {
                ImGui::PushID(filetype.data());

                ImGui::SeparatorText(filetype == "pic" ? "Фото" : "Чертеж");

                std::string imgFilename = GetRawImgFilename(_XlsxViewData, filetype);

                DrawImgPreview(imgFilename);
                DrawImgMakeScreenshot(imgFilename);

                ImGui::PopID();
            }
        }
        ImGui::End();
    }

    void XlsxPageView::DrawImgPreview(std::string_view _ImgFilename)
    {
        static float elementHeight = ImGui::GetFontSize() * 10;

        Ref<Texture2D> texture = nullptr;
        if (TextureManager::Contains(_ImgFilename.data()))
        {
            texture = TextureManager::Get(_ImgFilename);
        }
        else if (std::filesystem::exists(_ImgFilename))
        {
            texture = TextureManager::AddOrReplace(_ImgFilename);
        }

        if (texture)
        {
            float elementWidth = ImGui::GetContentRegionAvail().x;
            float imgSizeCoef = glm::min(elementWidth / texture->GetWidth(), elementHeight / texture->GetHeight());
            ImVec2 imgSize { texture->GetWidth() * imgSizeCoef, texture->GetHeight() * imgSizeCoef };
            ImGui::Image(reinterpret_cast<ImTextureID>(texture->GetTextureId()), imgSize);
        }
        else
        {
            ImGui::Button("##empty", { elementHeight, elementHeight });
        }
    }

    bool XlsxPageView::DrawImgMakeScreenshot(std::string_view _ImgFilename)
    {
        bool res = false;
        if (ImGui::Button("Сделать скиншот"))
        {
            res = MakeScreenshot(_ImgFilename.data());
        }
        ImGui::SameLine();
        if (ImGui::Button("Вставить из буфера"))
        {
            res = MakeScreenshotFromClipboard(_ImgFilename.data());
        }
        if (res)
        {
            TextureManager::RemoveFile(_ImgFilename);
        }
        return res;
    }

    void XlsxPageView::DrawJoinModal(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData)
    {
        static bool isSkipEmpty = true;
        static std::string joinStr;

        if (m_IsJoinModalOpen)
        {
            if (ImGui::Begin("Объединение колонок", &m_IsJoinModalOpen))
            {
                ImGui::InputText("Разделитель", &joinStr);
                ImGui::Text("Текст разделителя: '%s'", joinStr.c_str());
                ImGui::Checkbox("Соединять пустые ячейки", &isSkipEmpty);

                ImGui::Separator();

                if (ImGui::Button("Применить"))
                {
                    SelectionRegion selectionRegion = GetSelectionRegion(_TableData, false);
                    if (selectionRegion.RowsCount > 0 && selectionRegion.ColsCount > 0)
                    {
                        for (auto& row : std::ranges::subrange(_TableData.begin() + selectionRegion.StartRow,
                                                               _TableData.begin() + selectionRegion.StartRow +
                                                                   selectionRegion.RowsCount))
                        {
                            auto& writeCell = *(row.begin() + selectionRegion.StartCol);
                            for (auto& cell : std::ranges::subrange(row.begin() + selectionRegion.StartCol + 1,
                                                                    row.begin() + selectionRegion.StartCol +
                                                                        selectionRegion.ColsCount))
                            {
                                if (cell.Value.empty() && isSkipEmpty)
                                {
                                    continue;
                                }
                                writeCell.Value = StrJoin({ writeCell.Value, cell.Value }, joinStr);
                                cell.Value = "";
                            }
                        }

                        _XlsxViewData.PushHistory();
                        m_IsJoinModalOpen = false;
                    }
                }
            }
            ImGui::End();
        }
    }

    void XlsxPageView::DrawFindAndReplaceModal(XlsxPageViewData& _XlsxViewData,
                                               XlsxPageViewDataTypes::TableData& _TableData)
    {
        static std::string findStr;
        static std::string replaceStr;
        if (m_IsFindAndReplaceModalOpen)
        {
            if (ImGui::Begin("Найти и заменить", &m_IsFindAndReplaceModalOpen))
            {
                ImGui::InputText("Найти", &findStr);
                ImGui::InputText("Заменить на", &replaceStr);

                ImGui::Separator();

                if (ImGui::Button("Применить"))
                {
                    SelectionRegion selectionRegion = GetSelectionRegion(_TableData, false);
                    if (!findStr.empty())
                    {
                        for (auto& row : std::ranges::subrange(_TableData.begin() + selectionRegion.StartRow,
                                                               _TableData.begin() + selectionRegion.StartRow +
                                                                   selectionRegion.RowsCount))
                        {
                            for (auto& cell : std::ranges::subrange(row.begin() + selectionRegion.StartCol,
                                                                    row.begin() + selectionRegion.StartCol +
                                                                        selectionRegion.ColsCount))
                            {
                                size_t pos = 0;
                                for (uint8_t i = 0; i < std::numeric_limits<uint8_t>::max(); ++i)
                                {
                                    pos = cell.Value.find(findStr, pos);
                                    if (pos == std::string::npos)
                                    {
                                        break;
                                    }
                                    cell.Value.replace(pos, findStr.size(), replaceStr);
                                    pos += replaceStr.size();
                                }
                            }
                        }

                        _XlsxViewData.PushHistory();
                        m_IsFindAndReplaceModalOpen = false;
                    }
                }
                ImGui::End();
            }
        }
    }

    void XlsxPageView::HandleImGuiEvents(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData)
    {
        if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) || _TableData.empty())
        {
            return;
        }

        ImGuiIO& io = ImGui::GetIO();

        if (m_SelectedRow.has_value() && (*m_SelectedRow >= _TableData.size()))
        {
            m_SelectedRow = std::nullopt;
        }
        if (m_SelectedCol.has_value() && ((_TableData.size() == 0) || (*m_SelectedCol >= _TableData[0].size())))
        {
            m_SelectedCol = std::nullopt;
        }
        for (auto& selectedCellRef :
             std::array<std::reference_wrapper<std::optional<glm::u64vec2>>, 2> { m_SelectedCell, m_ExtraSelectedCell })
        {
            auto& selectedCell = selectedCellRef.get();
            if (selectedCell.has_value() && (((_TableData.size() == 0) || (selectedCell->x >= _TableData[0].size())) ||
                                             (selectedCell->y >= _TableData.size())))
            {
                selectedCell = std::nullopt;
            }
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !m_IsAnyCellActive)
        {
            LOG_CORE_INFO("Escape key pressed");
            ImGui::ClearActiveID();
            ImGui::SetNavID(0, ImGuiNavLayer_Main, 0, {});
            ImGui::SetWindowFocus(GetWindowName());
            UnSelectAll();
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Z) && io.KeyCtrl && !m_IsAnyCellActive)
        {
            _XlsxViewData.GetCurrentPageData().Undo();
        }
        if (ImGui::IsKeyPressed(ImGuiKey_Y) && io.KeyCtrl && !m_IsAnyCellActive)
        {
            _XlsxViewData.GetCurrentPageData().Redo();
        }

        if (ImGui::IsKeyPressed(ImGuiKey_J) && io.KeyCtrl)
        {
            m_IsJoinModalOpen = true;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_F) && io.KeyCtrl)
        {
            m_IsFindAndReplaceModalOpen = true;
        }

        // TODO: Add range for Key_X and Key_C

        if (ImGui::IsKeyPressed(ImGuiKey_X) && io.KeyCtrl)
        {
            SelectionRegion selectedRegion = GetSelectionRegion(_TableData, io.KeyShift);
            CopySelectedToClipboard(_TableData, selectedRegion);
            ClearSelected(_XlsxViewData, _TableData, selectedRegion);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_C) && io.KeyCtrl)
        {
            SelectionRegion selectedRegion = GetSelectionRegion(_TableData, io.KeyShift);
            CopySelectedToClipboard(_TableData, selectedRegion);
        }

        if (ImGui::IsKeyPressed(ImGuiKey_V) && io.KeyCtrl)
        {
            // TODO: Insert with column or row selectd (with shift and without)
            InsertFromClipboard(_XlsxViewData, _TableData);
        }

        // TODO: Move to separate function
        if (ImGui::IsKeyPressed(ImGuiKey_Equal) && io.KeyCtrl)
        {
            size_t selectedRow = io.KeyAlt ? 0 : _TableData.size();
            size_t selectedCol = io.KeyAlt ? 0 : _TableData[0].size();
            if (m_SelectedCell.has_value())
            {
                selectedCol = m_SelectedCell->x + 1;
                selectedRow = m_SelectedCell->y + 1;
            }
            else if (m_SelectedRow.has_value())
            {
                selectedRow = *m_SelectedRow + 1;
            }
            else if (m_SelectedCol.has_value())
            {
                selectedCol = *m_SelectedCol + 1;
            }

            bool isAddToCol = io.KeyShift;
            size_t selectedItem = isAddToCol ? selectedCol : selectedRow;
            if ((selectedItem > 0) && io.KeyAlt)
            {
                --selectedItem;
            }

            if (isAddToCol)
            {
                _XlsxViewData.GetCurrentPageData().InsertCol(selectedItem);
            }
            else
            {
                _XlsxViewData.GetCurrentPageData().InsertRow(selectedItem);
            }
            return;
        }

        if (ImGui::IsKeyPressed(ImGuiKey_Minus) && io.KeyCtrl && !io.KeyShift && !io.KeyAlt)
        {
            if (m_SelectedRow.has_value())
            {
                _XlsxViewData.GetCurrentPageData().DeleteRow(*m_SelectedRow);
            }
            if (m_SelectedCol.has_value())
            {
                _XlsxViewData.GetCurrentPageData().DeleteCol(*m_SelectedCol);
            }
            if ((m_SelectedRow.has_value() || m_SelectedCol.has_value()) && (_TableData.size() == 0))
            {
                _TableData.resize(1, std::vector<XlsxPageViewDataTypes::TableCell>(1, { .Value = "" }));
            }
            return;
        }

        // TODO:
        if (ImGui::IsKeyPressed(ImGuiKey_Delete))
        {

            SelectionRegion selectedRegion = GetSelectionRegion(_TableData, io.KeyShift);
            ClearSelected(_XlsxViewData, _TableData, selectedRegion);
            return;
        }
    }

    void XlsxPageView::PushCellFrameBgColor(XlsxPageViewDataTypes::TableData& _TableData, bool _IsRowHovered,
                                            bool _IsColHovered, size_t _RowId, size_t _ColId)
    {
        float frameBgAlpha = 0.0;
        float frameBgBlue = 0.0f;

        bool isHovered = _IsRowHovered || _IsColHovered;

        // TODO: move GetSelectionRegion upper for optimization
        bool isSelected = IsInSelectionRegion(GetSelectionRegion(_TableData, false), _RowId, _ColId);

        // Check if the cell is selected
        if (isSelected && isHovered)
        {
            frameBgAlpha = 0.87f;
            frameBgBlue = 0.65f;
        }
        else if (isHovered)
        {
            frameBgAlpha = 0.6f;
            frameBgBlue = 0.5f;
        }
        else if (isSelected)
        {
            frameBgAlpha = 0.75f;
            frameBgBlue = 0.75f;
        }

        if (frameBgAlpha == 0.0f && _TableData[_RowId][_ColId].Check != XlsxPageViewDataTypes::CheckStatus::kNone)
        {
            frameBgAlpha = 0.125f;
        }

        switch (_TableData[_RowId][_ColId].Check)
        {
            case XlsxPageViewDataTypes::CheckStatus::kOk:
                ImGui::PushStyleColor(ImGuiCol_FrameBg, GetFrameBgColorOk(frameBgAlpha, frameBgBlue));
                break;
            case XlsxPageViewDataTypes::CheckStatus::kWarning:
                ImGui::PushStyleColor(ImGuiCol_FrameBg, GetFrameBgColorWarn(frameBgAlpha, frameBgBlue));
                break;
            case XlsxPageViewDataTypes::CheckStatus::kError:
                ImGui::PushStyleColor(ImGuiCol_FrameBg, GetFrameBgColorError(frameBgAlpha, frameBgBlue));
                break;
            default: ImGui::PushStyleColor(ImGuiCol_FrameBg, GetFrameBgColorNone(frameBgAlpha, frameBgBlue)); break;
        }
    }

    XlsxPageView::DrawTableHeaderReturn XlsxPageView::DrawTableHeader(size_t _ColsCount,
                                                                      XlsxPageViewData& _XlsxViewData,
                                                                      XlsxPageViewDataTypes::TableData& _TableData)
    {
        DrawTableHeaderReturn result;

        for (size_t colId = 0; colId < _ColsCount + 1; ++colId)
        {
            ImGui::TableSetupColumn(std::format("Header_{}", colId).c_str());
        }

        ImGui::TableNextRow();
        ImGui::PushID(static_cast<int>(0));
        ImGui::TableSetColumnIndex(static_cast<int>(0));
        ImGui::TableHeader("  . . .  ##Header");
        if (ImGui::IsKeyPressed(ImGuiKey_Escape) && !m_IsAnyCellActive && m_IsMainWindowFocused)
        {
            ImGui::SetFocusID(ImGui::GetItemID(), ImGui::GetCurrentWindow());
        }
        if (ImGui::BeginPopupContextItem("Header_0"))
        {
            DrawTableHeaderRowContextMenu(_XlsxViewData, _TableData);

            ImGui::Separator();

            if (ImGui::Button("Close"))
            {
                ImGui::CloseCurrentPopup();
            }
            ImGui::EndPopup();
        }

        for (size_t colId = 0; colId < _ColsCount; ++colId)
        {
            ImGui::PushID(static_cast<int>(colId));
            ImGui::TableSetColumnIndex(static_cast<int>(colId + 1));

            auto& t = _TableData[0][colId].Value;
            // std::string headerText = std::format("{}##_Header_{}", t, colId + 1);
            ImU32 headerColor = 0xFF000000;
            if (_XlsxViewData.IsItemInGlobalAddList(t) || _XlsxViewData.IsItemInSimpleAddListForCurrentPage(t) ||
                _XlsxViewData.IsItemInSimpleCalcListForCurrentPage(t))
            {
                headerColor = 0xFF008000;
            }
            else if (std::ranges::find(kProductBaseFields, t) != kProductBaseFields.end())
            {
                headerColor = 0xFF555555;
            }
            else if (m_FieldsDescription.contains(t))
            {
                if (m_FieldsDescription[t].IsDescr)
                {
                    headerColor = 0xFF008CFF;
                }
                else if (!m_FieldsDescription[t].AllowNulls)
                {
                    headerColor = 0xFFCC3299;
                }
            }
            ImGui::PushStyleColor(ImGuiCol_TableHeaderBg, headerColor);
            ImGui::TableHeader(std::format("  {}  ", t).c_str());
            ImGui::PopStyleColor();

            bool autoFocusNameChange = false;
            std::string popupStrId = std::format("Header_{}", colId);
            if (ImGui::IsItemHovered())
            {
                result.HoveredCol = colId;

                if (m_FieldsDescription.contains(t))
                {
                    ImGui::SetTooltip("%s", m_FieldsDescription[t].Description.c_str());
                }
            }

            if (ImGui::IsItemActivated() && !ImGui::IsMouseDown(ImGuiMouseButton_Left))
            {
                ImGui::OpenPopup(popupStrId.c_str());
            }

            if ((ImGui::IsItemHovered() || ImGui::IsItemFocused()) && ImGui::IsKeyPressed(ImGuiKey_F2))
            {
                autoFocusNameChange = true;
                ImGui::OpenPopup(popupStrId.c_str());
            }

            if (ImGui::IsItemFocused())
            {
                UnSelectAll();
                m_SelectedCol = colId;
            }

            if (ImGui::BeginPopupContextItem(popupStrId.c_str()))
            {
                if (autoFocusNameChange)
                {
                    ImGui::SetKeyboardFocusHere();
                }
                ImGui::InputText("Имя заголовка##HeaderInput", &t);
                if (ImGui::IsItemDeactivatedAfterEdit())
                {
                    _XlsxViewData.PushHistory();
                    ImGui::CloseCurrentPopup();
                }
                if (ImGui::IsItemActive())
                {
                    m_IsAnyHeaderActive = true;
                }

                ImGui::SeparatorText("Fix Data");

                // TODO: Move to separate function
                if (ImGui::Button("Fix as float"))
                {
                    std::unordered_map<char, char> replacements = {
                        { ',', '.' },
                        { 'O', '0' },
                        { 'o', '0' },
                        { 'D', '0' },
                        { 'Q', '0' },
                        { 'i', '1' },
                        { 'I', '1' },
                        { 'l', '1' },
                        { 'L', '1' },
                        { '|', '1' },
                        { 'Z', '2' },
                        { 'A', '4' },
                        { 'H', '4' },
                        { 'S', '5' },
                        { 'G', '6' },
                        { 'b', '6' },
                        { 'B', '8' },
                        { 'g', '9' },
                        { 'q', '9' },
                    };

                    for (size_t rowId = 1; rowId < _TableData.size(); ++rowId)
                    {
                        auto& cellText = _TableData[rowId][colId].Value;
                        for (auto& ch : cellText)
                        {
                            if (replacements.find(ch) != replacements.end())
                            {
                                ch = replacements[ch];
                            }
                        }

                        size_t pos = cellText.find("..");
                        while (pos != std::string::npos)
                        {
                            cellText.replace(pos, 2, ".");
                            pos = cellText.find("..", pos + 1);
                        }
                    }

                    _XlsxViewData.PushHistory();
                }

                ImGui::SeparatorText("Checks");

                // TODO: Move to separate function
                if (ImGui::Button("Clear Checks"))
                {
                    for (size_t rowId = 1; rowId < _TableData.size(); ++rowId)
                    {
                        _TableData[rowId][colId].Check = XlsxPageViewDataTypes::CheckStatus::kNone;
                    }

                    _XlsxViewData.PushHistory();
                }

                // TODO: Move to separate function
                if (ImGui::Button("Check for float"))
                {
                    for (size_t rowId = 1; rowId < _TableData.size(); ++rowId)
                    {
                        auto& cellText = _TableData[rowId][colId].Value;
                        float _;
                        auto [ptr, ec] = std::from_chars(cellText.data(), cellText.data() + cellText.size(), _);
                        bool result = ec == std::errc() && ptr == cellText.data() + cellText.size();
                        if (result)
                        {
                            _TableData[rowId][colId].Check = XlsxPageViewDataTypes::CheckStatus::kOk;
                        }
                        else
                        {
                            _TableData[rowId][colId].Check = XlsxPageViewDataTypes::CheckStatus::kError;
                        }
                    }

                    _XlsxViewData.PushHistory();
                }

                ImGui::SeparatorText("Actions");
                if (ImGui::Button("Delete Col"))
                {
                    m_DeleteCol = colId;
                    ImGui::CloseCurrentPopup();
                }

                ImGui::Separator();
                if (ImGui::Button("Close"))
                {
                    ImGui::CloseCurrentPopup();
                }
                ImGui::EndPopup();
            }

            ImGui::PopID();
        }

        ImGui::PopID();

        return result;
    }

    void XlsxPageView::DrawTableHeaderRowContextMenu(XlsxPageViewData& _XlsxViewData,
                                                     XlsxPageViewDataTypes::TableData& _TableData)
    {
        if (ImGui::Button("Удалить заголовок"))
        {
            m_DeleteRow = 0;
            ImGui::CloseCurrentPopup();
        }

        ImGui::SameLine();

        ImGui::Text("Вторая строка станет заголовком");

        DrawTableActions(_XlsxViewData, _TableData);
    }

    std::optional<const std::reference_wrapper<std::string>>
    XlsxPageView::GetExtraListValue(XlsxPageViewData& _XlsxViewData, std::string_view _Header)
    {
        if (std::optional<std::reference_wrapper<XlsxPageViewDataTypes::SimpleAddListItem>> cell =
                _XlsxViewData.GetItemInSimpleCalcListForCurrentPage(_Header);
            cell.has_value())
        {
            return cell->get().Value;
        }

        if (std::optional<std::reference_wrapper<XlsxPageViewDataTypes::SimpleAddListItem>> cell =
                _XlsxViewData.GetItemInSimpleAddListForCurrentPage(_Header);
            cell.has_value())
        {
            return cell->get().Value;
        }

        if (_XlsxViewData.GetGlobalAddList().contains(_Header.data()))
        {
            return _XlsxViewData.GetGlobalAddList()[_Header.data()];
        }

        return std::nullopt;
    }

    std::string XlsxPageView::GetRawImgFilename(XlsxPageViewData& _XlsxViewData, std::string_view _Filetype)
    {
        std::filesystem::path filename = _XlsxViewData.GetCurrentPageData().GetPageFilename().filename();
        filename.replace_extension();
        filename = std::format("{}_{}.png", filename.string(), _Filetype);

        return (m_Project->GetVariantExcelTablesHelpers().GetImgsPerPagePath() / filename).string();
    }

    std::string XlsxPageView::GetSimpleRuleImgFilename(std::string_view _Filename)
    {
        return (m_Project->GetVariantExcelTablesHelpers().GetImgsSimpleRulePath() / _Filename).string();
    }

    void XlsxPageView::ReplaceFromClipboard(XlsxPageViewData& _XlsxViewData,
                                            XlsxPageViewDataTypes::TableData& _TableData, bool _IsNeedEmptyHeaderRow)
    {
        if (const char* clipboard = ImGui::GetClipboardText())
        {
            _TableData.clear();

            std::istringstream iss(clipboard);
            std::string line;
            while (std::getline(iss, line))
            {
                std::vector<XlsxPageViewDataTypes::TableCell> row;
                std::istringstream lineStream(line);
                std::string cell;
                while (std::getline(lineStream, cell, '\t'))
                {
                    row.push_back({ .Value = cell });
                }
                _TableData.push_back(std::move(row));
            }

            if (_IsNeedEmptyHeaderRow)
            {
                _TableData.insert(_TableData.begin(), std::vector<XlsxPageViewDataTypes::TableCell>());
            }
            FixDimensions(_TableData);
            _XlsxViewData.PushHistory();
        }
    }

    XlsxPageView::SelectionRegion XlsxPageView::GetSelectionRegion(XlsxPageViewDataTypes::TableData& _TableData,
                                                                   bool _IncludeHeader)
    {
        if ((!m_SelectedCell.has_value() && !m_SelectedRow.has_value() && !m_SelectedCol.has_value()) ||
            (m_SelectedCell.has_value() && m_IsAnyCellActive) || (_TableData.empty() || _TableData[0].empty()))
        {
            return { .StartRow = 0, .StartCol = 0, .RowsCount = 0, .ColsCount = 0 };
        }

        // TODO: Add checks if needed

        if (m_SelectedCell.has_value())
        {
            glm::u64vec2 minCell = *m_SelectedCell;
            glm::u64vec2 maxCell = *m_SelectedCell;
            if (m_ExtraSelectedCell.has_value())
            {
                minCell = glm::min(*m_SelectedCell, *m_ExtraSelectedCell);
                maxCell = glm::max(*m_SelectedCell, *m_ExtraSelectedCell);
            }
            return {
                .StartRow = minCell.y,
                .StartCol = minCell.x,
                .RowsCount = maxCell.y - minCell.y + 1,
                .ColsCount = maxCell.x - minCell.x + 1,
            };
        }
        if (m_SelectedRow.has_value())
        {
            return { .StartRow = *m_SelectedRow, .StartCol = 0, .RowsCount = 1, .ColsCount = _TableData[0].size() };
        }
        if (m_SelectedCol.has_value())
        {
            size_t offset = _IncludeHeader ? 0 : 1;
            return {
                .StartRow = offset, .StartCol = *m_SelectedCol, .RowsCount = _TableData.size() - offset, .ColsCount = 1
            };
        }

        return { .StartRow = 0, .StartCol = 0, .RowsCount = 0, .ColsCount = 0 };
    }

    bool XlsxPageView::IsInSelectionRegion(const SelectionRegion& _SelectionRegion, size_t _RowId, size_t _ColId)
    {
        if (_SelectionRegion.RowsCount == 0 || _SelectionRegion.ColsCount == 0)
        {
            return false;
        }

        glm::u64vec2 minCell = { _SelectionRegion.StartRow, _SelectionRegion.StartCol };
        glm::u64vec2 maxCell = { _SelectionRegion.StartRow + _SelectionRegion.RowsCount - 1,
                                 _SelectionRegion.StartCol + _SelectionRegion.ColsCount - 1 };

        return _RowId >= minCell.x && _RowId <= maxCell.x && _ColId >= minCell.y && _ColId <= maxCell.y;
    }

    void XlsxPageView::CopySelectedToClipboard(XlsxPageViewDataTypes::TableData& _TableData,
                                               const SelectionRegion& _SelectionRegion)
    {
        std::string result;
        for (const auto& row :
             std::ranges::subrange(_TableData.begin() + _SelectionRegion.StartRow,
                                   _TableData.begin() + _SelectionRegion.StartRow + _SelectionRegion.RowsCount))
        {
            std::vector<std::string> cellArr;
            for (const auto& cell :
                 std::ranges::subrange(row.begin() + _SelectionRegion.StartCol,
                                       row.begin() + _SelectionRegion.StartCol + _SelectionRegion.ColsCount))
            {
                result += cell.Value;
                if (&cell != &row[_SelectionRegion.StartCol + _SelectionRegion.ColsCount - 1])
                {
                    result += "\t";
                }
            }

            if (&row != &_TableData[_SelectionRegion.StartRow + _SelectionRegion.RowsCount - 1])
            {
                result += "\n";
            }
        }
        ImGui::SetClipboardText(result.c_str());

        LOG_CORE_WARN("{}", result.c_str());
    }

    void XlsxPageView::InsertFromClipboard(XlsxPageViewData& _XlsxViewData,
                                           XlsxPageViewDataTypes::TableData& _TableData)
    {
        const char* clipboard = ImGui::GetClipboardText();
        if (!clipboard)
        {
            return;
        }

        if (m_SelectedCell.has_value() && (!m_IsAnyCellActive || StrContainsNewlineOrTab(clipboard)))
        {
            ImGui::ClearActiveID();
            std::istringstream iss(clipboard);
            std::string line;
            for (size_t insertRow = m_SelectedCell->y; std::getline(iss, line); ++insertRow)
            {
                if (_TableData.size() == insertRow)
                {
                    _TableData.push_back(std::vector<XlsxPageViewDataTypes::TableCell>(
                        _TableData[0].size(), XlsxPageViewDataTypes::TableCell {}));
                }
                std::vector<XlsxPageViewDataTypes::TableCell>& row = _TableData[insertRow];

                std::istringstream lineStream(line);
                std::string cell;
                for (size_t insertCol = m_SelectedCell->x; std::getline(lineStream, cell, '\t'); ++insertCol)
                {
                    if (row.size() == insertCol)
                    {
                        row.push_back({});
                    }
                    row[insertCol].Value = cell;
                }
            }
            FixDimensions(_TableData);
            _XlsxViewData.PushHistory();
        }
    }

    void XlsxPageView::ClearSelected(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData,
                                     const SelectionRegion& _SelectionRegion)
    {
        for (auto& row :
             std::ranges::subrange(_TableData.begin() + _SelectionRegion.StartRow,
                                   _TableData.begin() + _SelectionRegion.StartRow + _SelectionRegion.RowsCount))
        {
            for (auto& cell :
                 std::ranges::subrange(row.begin() + _SelectionRegion.StartCol,
                                       row.begin() + _SelectionRegion.StartCol + _SelectionRegion.ColsCount))
            {
                cell = {};
            }
        }

        _XlsxViewData.PushHistory();
    }

    void XlsxPageView::UnSelectAll(bool _UnSelectExtraCell)
    {
        m_SelectedCell = std::nullopt;
        if (_UnSelectExtraCell)
        {
            m_ExtraSelectedCell = std::nullopt;
        }
        m_SelectedCol = std::nullopt;
        m_SelectedRow = std::nullopt;
    }

    void XlsxPageView::SplitAndExpandTable(XlsxPageViewData& _XlsxViewData,
                                           XlsxPageViewDataTypes::TableData& _TableData)
    {
        if (_TableData.empty())
        {
            return;
        }

        size_t rows = _TableData.size();
        size_t cols = _TableData[0].size();

        LOG_CORE_WARN("Splitting table with columns: {}", cols);

        std::vector<std::vector<std::vector<std::string>>> colsSplitData(cols);
        for (auto& col : colsSplitData)
        {
            col.resize(_TableData.size());
        }
        std::vector<size_t> colsAfterSplitCount(cols, 1);

        for (size_t col = 0; col < cols; ++col)
        {
            for (size_t row = 0; row < _TableData.size(); ++row)
            {
                std::istringstream iss(_TableData[row][col].Value);
                std::string word;

                while (iss >> word)
                {
                    colsSplitData[col][row].push_back(word);
                }
                if (colsSplitData[col][row].size() == 0)
                {
                    colsSplitData[col][row].push_back("");
                }

                colsAfterSplitCount[col] = std::max(colsAfterSplitCount[col], colsSplitData[col][row].size());
            }
        }

        size_t totalSplitCount = std::accumulate(colsAfterSplitCount.begin(), colsAfterSplitCount.end(), 0);

        _TableData.clear();
        _TableData.resize(rows, std::vector<XlsxPageViewDataTypes::TableCell>(totalSplitCount, { .Value = "" }));

        for (size_t oldCol = 0; oldCol < cols; ++oldCol)
        {
            size_t newCol = 0;
            for (size_t i = 0; i < oldCol; ++i)
            {
                newCol += colsAfterSplitCount[i];
            }

            for (size_t row = 0; row < _TableData.size(); ++row)
            {
                for (size_t col = 0; col < colsAfterSplitCount[oldCol]; ++col)
                {
                    _TableData[row][newCol + col] = {
                        .Value = col < colsSplitData[oldCol][row].size() ? colsSplitData[oldCol][row][col] : "",
                    };
                }
            }
        }

        _XlsxViewData.PushHistory();
    }

    void XlsxPageView::FixDimensions(XlsxPageViewDataTypes::TableData& _TableData)
    {
        size_t maxCols = 0;
        for (const auto& row : _TableData)
        {
            maxCols = std::max(maxCols, row.size());
        }
        for (auto& row : _TableData)
        {
            row.resize(maxCols, { .Value = "" });
        }
    }

    void XlsxPageView::ChangeHeadersByConstruction(XlsxPageViewData& _XlsxViewData,
                                                   XlsxPageViewDataTypes::TableData& _TableData,
                                                   std::string_view _ConstrKey)
    {
        if (!m_ConstructionsFields.contains(_ConstrKey.data()))
        {
            Overlay::Get()->Start(Format("Не найдены поля для конструкции: \n{}", _ConstrKey));
        }
        if (_TableData.size() == 0)
        {
            _TableData.push_back(std::vector<XlsxPageViewDataTypes::TableCell>());
        }

        std::vector<XlsxPageViewDataTypes::TableCell>& headerRow = _TableData[0];
        headerRow.clear();
        for (const std::string& field : kProductBaseFields)
        {
            headerRow.push_back({ .Value = field });
        }
        for (std::string& field : m_ConstructionsFields[_ConstrKey.data()])
        {
            headerRow.push_back({ .Value = field });
        }

        FixDimensions(_TableData);
        _XlsxViewData.PushHistory();
    }

    void XlsxPageView::LoadConstructionsTree()
    {
        std::ifstream infile(kDefaultConstructionsTreeFile.data());
        if (!infile.is_open())
        {
            Overlay::Get()->Start(
                Format("Не удалось открыть стандартный файл дерева конструкций: \n{}", kDefaultConstructionsTreeFile));
        }

        try
        {
            nlohmann::json json;
            infile >> json;

            for (const auto& jsonEntity : json["nodes"])
            {
                for (const auto& jsonGroup : jsonEntity["nodes"])
                {
                    for (const auto& jsonConstr : jsonGroup["nodes"])
                    {
                        ConstructionTreeConstr constr {
                            { .Img = jsonConstr["img_link"], .Label = jsonConstr["text"], .Key = jsonConstr["key"] },
                        };
                        constr.Drw = jsonConstr["drw_link"];
                        m_Constructions.emplace_back(std::move(constr));
                    }
                }
            }
        }
        catch (...)
        {
            Overlay::Get()->Start(Format("Ошибка во время чтения формата json: \n{}", kDefaultConstructionsTreeFile));
        }
    }

    void XlsxPageView::LoadConstructionsFields()
    {
        std::ifstream infile(kDefaultConstructionsFieldsFile.data());
        if (!infile.is_open())
        {
            Overlay::Get()->Start(
                Format("Не удалось открыть стандартный файл конструкций: \n{}", kDefaultConstructionsFieldsFile));
        }

        try
        {
            nlohmann::json json;
            infile >> json;

            for (const auto& element : json.items())
            {
                std::vector<std::string> v;
                element.value().get_to(v);
                m_ConstructionsFields[element.key()] = v;
            }
        }
        catch (...)
        {
            Overlay::Get()->Start(Format("Ошибка во время чтения формата json: \n{}", kDefaultConstructionsFieldsFile));
        }
    }

    void XlsxPageView::LoadFieldsDescription()
    {
        std::ifstream infile(kDefaultFieldsDescriptionFile.data());
        if (!infile.is_open())
        {
            Overlay::Get()->Start(Format("Не удалось открыть стандартный файл описания полей конструкций: \n{}",
                                         kDefaultFieldsDescriptionFile));
        }

        try
        {
            nlohmann::json json;
            infile >> json;

            for (const auto& element : json)
            {
                m_FieldsDescription[element["fieldname"].get<std::string>()] = FieldDescription {
                    .Type = element["typ"],
                    .UnitRu = element.at("unitru").get<std::optional<std::string>>(),
                    .RuDescription = element["rudescription"],
                    .AllowNulls = element["allownulls"],
                    .IfBooleanTrue = element.at("ifbooleantrue").get<std::optional<std::string>>(),
                    .IfBooleanFalse = element.at("ifbooleanfalse").get<std::optional<std::string>>(),
                    .QType = element["qtyp"],
                    .IsDescr = element["is_descr"],
                    .Description = element["descr"],
                };
            }
        }
        catch (std::exception& e)
        {
            LOG_CORE_WARN("{}", e.what());
            Overlay::Get()->Start(Format("Ошибка во время чтения формата json: \n{}", kDefaultFieldsDescriptionFile));
        }
    }

    void XlsxPageView::LoadRepresentationFieldsDescription()
    {
        std::ifstream infile(kDefaultRepresentationFieldsDescriptionFile.data());
        if (!infile.is_open())
        {
            Overlay::Get()->Start(Format("Не удалось открыть стандартный файл представления полей: \n{}",
                                         kDefaultRepresentationFieldsDescriptionFile));
        }

        try
        {
            nlohmann::json json;
            infile >> json;

            for (const auto& element : json.items())
            {
                std::vector<FieldRepresentationItem> v;
                for (const auto& item : element.value())
                {
                    std::string key;
                    if (item["key"].is_number())
                    {
                        uint32_t val = item["key"];
                        key = std::to_string(val);
                    }
                    else
                    {
                        key = item["key"];
                    }

                    v.push_back({
                        .Key = key,
                        .RuShort = item["rushort"],
                        .Ru = item["ru"],
                    });
                }
                m_FieldsRepresentation[element.key()] = v;
            }
        }
        catch (std::exception& e)
        {
            LOG_CORE_WARN("{}", e.what());
            Overlay::Get()->Start(
                Format("Ошибка во время чтения формата json: \n{}", kDefaultRepresentationFieldsDescriptionFile));
        }
    }

    void XlsxPageView::LoadConstrExample(std::string_view _Constr)
    {
        if (m_ConstrExamples.contains(_Constr.data()))
        {
            return;
        }

        std::filesystem::path path = std::filesystem::path("assets/constructions/examples") /
                                     std::filesystem::path(std::format("{}.xlsx", _Constr));

        LOG_CORE_INFO("Loading file: {}", path.string());

        if (!std::filesystem::exists(path))
        {
            return;
        }

        xlnt::workbook wb;

        try
        {
            wb.load(path);
        }
        catch (const std::exception& e)
        {
            LOG_CORE_ERROR("Failed to load workbook: {}", e.what());
            return;
        }

        xlnt::worksheet ws = wb.active_sheet();

        m_ConstrExamples[_Constr.data()] = {};
        auto& constr = m_ConstrExamples[_Constr.data()];
        for (const auto& col : ws.columns(true))
        {
            if (col.length() < 2 || !col[0].has_value())
            {
                continue;
            }

            std::vector<std::string> values;
            auto begin = col.begin();
            ++begin;
            for (const auto& cell : std::ranges::subrange(begin, col.end()))
            {
                values.push_back(cell.has_value() ? cell.to_string() : "");
            }
            constr[col[0].to_string()] = values;
        }
    }

    void XlsxPageView::LoadAmatiCodems()
    {
        xlnt::workbook wb;

        try
        {
            wb.load(kDefaultAmatiCodemsFile.data());
        }
        catch (const std::exception& e)
        {
            LOG_CORE_ERROR("Failed to load workbook: {}", e.what());
            return;
        }

        xlnt::worksheet ws = wb.active_sheet();

        for (auto row : ws.rows(false))
        {
            if (row.length() < 2)
            {
                continue;
            }
            m_AmatiCodems[InterpolateModel(row[0].to_string())] = StrTrim(row[1].to_string());
        }

        m_IsAmatiCodemsLoaded = true;
    }

    const std::unordered_map<std::string, std::string>& XlsxPageView::GetAmatiCodemsWithLoad()
    {
        if (m_AmatiCodems.empty())
        {
            LoadAmatiCodems();
        }
        return m_AmatiCodems;
    }

    void XlsxPageView::FindAndInsertAmatiCodems(XlsxPageViewData& _XlsxViewData,
                                                XlsxPageViewDataTypes::TableData& _TableData)
    {
        std::optional<size_t> modelColId;
        std::optional<size_t> codemColId;

        if (_TableData.empty())
        {
            return;
        }

        for (size_t i = 0; i < _TableData[0].size(); ++i)
        {
            const auto& cell = _TableData[0][i];
            if (cell.Value == "model")
            {
                modelColId = i;
            }
            if (cell.Value == "codem")
            {
                codemColId = i;
            }
        }

        if (!modelColId.has_value() || !codemColId.has_value())
        {
            return;
        }

        const std::unordered_map<std::string, std::string>& amatiCodems = GetAmatiCodemsWithLoad();

        for (auto& row : _TableData)
        {
            std::string iterpmodel = InterpolateModel(row[*modelColId].Value);
            if (amatiCodems.contains(iterpmodel))
            {
                row[*codemColId].Value = amatiCodems.at(iterpmodel);
            }
        }

        _XlsxViewData.PushHistory();
    }

    bool XlsxPageView::IsExtraInfoAutoFocusField(std::string_view _WindowName, std::string_view _FieldName)
    {
        bool result = m_ExtraInfoAutoFocusField.has_value() && (m_ExtraInfoAutoFocusField->WindowName == _WindowName) &&
                      (m_ExtraInfoAutoFocusField->Field == _FieldName);
        if (result)
        {
            m_ExtraInfoAutoFocusField = std::nullopt;
        }
        return result;
    }

    void XlsxPageView::DrawProcessingScriptsWindow()
    {
        if (ImGui::Begin("Обработка и импорт данных"))
        {
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Выбранная конфигурация подключения: %s",
                               SharedConnectionConfigSetup::GetCurrentConfigName().data());

            ImGui::TextColored(
                ImVec4(1.0f, 1.0f, 0.0f, 1.0f),
                "Если предыдущие шаги не были выполнены, то они будут выполнены автоматически перед запуском скрипта.");

            ImVec4 needProcessColor = ImVec4(0.5f, 0.0f, 0.0f, 1.0f);
            ImVec4 processedColor = ImVec4(0.0f, 0.5f, 0.0f, 1.0f);

            ImGui::TextColored(ImVec4(0.85f, 0.0f, 0.0f, 1.0f), "Кнопка данного цвета - процесс нужно выполнить.");

            ImGui::TextColored(ImVec4(0.0f, 0.85f, 0.0f, 1.0f), "Кнопка данного цвета - процесс уже выполнен.");

            if (ImGui::TreeNodeEx("Общее", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGuiButtonColored("Обработать файлы по правилам",
                                       m_Project->GetVariantExcelTables().GetIsAddExtraInfoNeedRebuild()
                                           ? needProcessColor
                                           : processedColor))
                {
                    Save();
                    ProcessAddExtraInfo();
                }

                if (ImGuiButtonColored("Обработать картинки",
                                       m_Project->GetVariantExcelTables().GetIsProcessImagesNeedRebuild()
                                           ? needProcessColor
                                           : processedColor))
                {
                    Save();
                    ProcessImages();
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("WBI Tools", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGuiButtonColored(
                        "Загрузить картинки и подготовить Xlsx",
                        m_Project->GetVariantExcelTables().GetIsUploadImagesAndPrepareXlsxForWbiToolsNeedRebuild()
                            ? needProcessColor
                            : processedColor))
                {
                    Save();
                    UploadImagesAndPrepareXlsxForWbiTools();
                }

                if (ImGui::Button("Просмотреть отсутствующие ADINT"))
                {
                    Save();
                    ViewNotInDbAdintFields();
                }
                ImGui::SameLine();
                if (ImGui::Button("Обновить базу недостающими ADINT"))
                {
                    Save();
                    AddNotInDbAdintFieldsToServer();
                }

                if (ImGuiButtonColored("Загрузить данные на сервер",
                                       m_Project->GetVariantExcelTables().GetIsImportDataToWbiToolsServerNeedRebuild()
                                           ? needProcessColor
                                           : processedColor))
                {
                    Save();
                    ImportDataToWbiToolsServer();
                }

                ImGui::TreePop();
            }

            if (ImGui::TreeNodeEx("YG1-Shop", ImGuiTreeNodeFlags_Framed | ImGuiTreeNodeFlags_DefaultOpen))
            {
                if (ImGui::Button("Собрать файлы в один и добавить доп. поля для yg1-shop"))
                {
                    Save();
                    Yg1ShopConstructionLevelConfigSetup::Get().SaveConfigIfHasChanges();

                    PythonCommand pythonCommand("./assets/scripts/excel_add_extra_info-yg1-shop.py");
                    pythonCommand.AddPathArg(
                        m_Project->GetVariantExcelTablesHelpers().GetXlsxAddExtraInfoWithProcessedImagesPath(),
                        "--xlsx_path");
                    pythonCommand.AddPathArg(
                        m_Project->GetVariantExcelTablesHelpers().GetXlsxForServerImportPath(kAddExtraInfoYg1Parser),
                        "--save_path");
                    pythonCommand.AddPathArg("https://yg1-shop.ru/nameduploads/", "--img_prefix");

                    ScriptPopup::Get()->AddToQueue(pythonCommand,
                                                   { "Заполнение данных для yg1-shop",
                                                     []() {
                                                         ImGui::Text("Работает скрипт заполнения данных для yg1-shop");
                                                         ImGui::Text("Это может занять несколько минут");
                                                         ImGui::Text("После его завершения можно закрыть это окно");
                                                     },
                                                     [](int) {} });
                }

                ImGui::TreePop();
            }
        }
        ImGui::End();
    }

    void XlsxPageView::DrawImgsToSkipBgRemoveWindow()
    {
        if (ImGui::Begin("Изображения, пропускаемые при удалении фона"))
        {
            ImGui::TextWrapped("Список изображений, которые не будут обрабатываться скриптом удаления фона из-за "
                               "особенностей их структуры. "
                               "Каждое имя файла должно быть на новой строке. (Временно строка)");

            ImGui::InputTextMultiline("##imgs_to_skip", &m_TMP_SkippedBgRemoveImgNames);
        }
        ImGui::End();
    }

    void XlsxPageView::ProcessAddExtraInfo(bool _IsNeedRunWithoutCheckIsDone)
    {
        if (!_IsNeedRunWithoutCheckIsDone && !m_Project->GetVariantExcelTables().GetIsAddExtraInfoNeedRebuild())
        {
            return;
        }

        PythonCommand pythonCommand("./assets/scripts/excel_add_extra_info.py");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetXlsxStartupPath(), "--xlsx_path");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetXlsxAddExtraInfoPath(), "--save_path");
        pythonCommand.AddPathArg(m_Project->GetXlsxPageViewData().GetExtraInfoJsonPath(), "--rules_path");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetImgsPerPagePath(),
                                 "--per_page_img_folder");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetImgsSimpleRulePath(),
                                 "--per_page_rule_img_folder");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetImgsNoConditionPath(),
                                 "--no_condition_img_folder");

        ScriptPopup::Get()->AddToQueue(
            pythonCommand, { "Заполнение данных по правилам",
                             []() {
                                 ImGui::Text("Работает скрипт заполнения данных по правилам");
                                 ImGui::Text("Это может занять несколько минут");
                                 ImGui::Text("После его завершения можно закрыть это окно");
                             },
                             [this](int) { m_Project->GetVariantExcelTables().SetIsAddExtraInfoNeedRebuild(false); } });
    }

    void XlsxPageView::ProcessImages(bool _IsNeedRunWithoutCheckIsDone)
    {
        if (!_IsNeedRunWithoutCheckIsDone && !m_Project->GetVariantExcelTables().GetIsProcessImagesNeedRebuild())
        {
            return;
        }

        ProcessAddExtraInfo(false);

        PythonCommand pythonCommand("./assets/scripts/imgs_process.py");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetXlsxAddExtraInfoPath(), "--xlsx_path");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetXlsxAddExtraInfoWithProcessedImagesPath(),
                                 "--xlsx_save_path");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetJsonPrevProcessedImagesFilePath(),
                                 "--prev_imgs_hash_and_map_filepath");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetImgsProcessedPath(), "--imgs_save_path");
        std::string bgSkipListPath = m_TMP_SkippedBgRemoveImgNames;
        StrReplaceAll(bgSkipListPath, "\n", ";");
        pythonCommand.AddArg(bgSkipListPath, "--bg_remove_skip_list_path");

        ScriptPopup::Get()->AddToQueue(
            pythonCommand,
            { "Обработка изображений",
              []() {
                  ImGui::Text("Работает скрипт обработки изображений");
                  ImGui::Text("Это может занять несколько минут");
                  ImGui::Text("После его завершения можно закрыть это окно");
              },
              [this](int) { m_Project->GetVariantExcelTables().SetIsProcessImagesNeedRebuild(false); } });
    }

    void XlsxPageView::UploadImagesAndPrepareXlsxForWbiTools(bool _IsNeedRunWithoutCheckIsDone)
    {
        if (!_IsNeedRunWithoutCheckIsDone &&
            !m_Project->GetVariantExcelTables().GetIsUploadImagesAndPrepareXlsxForWbiToolsNeedRebuild())
        {
            return;
        }

        ProcessImages(false);

        // TODO: add variant for use add extra info xlsx without processed images
        PythonCommand pythonCommand("./assets/scripts/imgs_to_server_format_and_upload.py");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetXlsxAddExtraInfoWithProcessedImagesPath(),
                                 "--xlsx_path");
        pythonCommand.AddPathArg(
            m_Project->GetVariantExcelTablesHelpers().GetXlsxForServerImportPath(kAddExtraInfoWbiToolsParser),
            "--xlsx_save_path");

        ScriptPopup::Get()->AddToQueue(
            pythonCommand,
            { "Подготовка данных для WBI Tools",
              []() {
                  ImGui::Text("Работает скрипт подготовки данных для WBI Tools");
                  ImGui::Text("Это может занять несколько минут");
                  ImGui::Text("После его завершения можно закрыть это окно");
              },
              [this](int) {
                  m_Project->GetVariantExcelTables().SetIsUploadImagesAndPrepareXlsxForWbiToolsNeedRebuild(false);
              } });
    }

    void XlsxPageView::ViewNotInDbAdintFields(bool _IsNeedRunWithoutCheckIsDone)
    {
        if (!_IsNeedRunWithoutCheckIsDone)
        {
            return;
        }

        UploadImagesAndPrepareXlsxForWbiTools(false);

        PythonCommand pythonCommand("./assets/scripts/view_not_exist_adint.py");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetXlsxAddExtraInfoWithProcessedImagesPath(),
                                 "--xlsx_path");

        ScriptPopup::Get()->AddToQueue(pythonCommand,
                                       { "Просмотр полей ADINT, отсутствующих в БД",
                                         []() {
                                             ImGui::Text("Работает скрипт просмотра полей ADINT, отсутствующих в БД");
                                             ImGui::Text("Это может занять несколько минут");
                                             ImGui::Text("После его завершения можно закрыть это окно");
                                         },
                                         [](int) {} });
    }

    void XlsxPageView::AddNotInDbAdintFieldsToServer(bool _IsNeedRunWithoutCheckIsDone)
    {
        if (!_IsNeedRunWithoutCheckIsDone)
        {
            return;
        }

        UploadImagesAndPrepareXlsxForWbiTools(false);

        PythonCommand pythonCommand("./assets/scripts/import_not_exist_adint.py");
        pythonCommand.AddPathArg(m_Project->GetVariantExcelTablesHelpers().GetXlsxAddExtraInfoWithProcessedImagesPath(),
                                 "--xlsx_path");

        ScriptPopup::Get()->AddToQueue(
            pythonCommand, { "Добавление полей ADINT, отсутствующих в БД, на сервер",
                             []() {
                                 ImGui::Text("Работает скрипт добавления полей ADINT, отсутствующих в БД, на сервер");
                                 ImGui::Text("Это может занять несколько минут");
                                 ImGui::Text("После его завершения можно закрыть это окно");
                             },
                             [](int) {} });
    }

    void XlsxPageView::ImportDataToWbiToolsServer(bool _IsNeedRunWithoutCheckIsDone)
    {
        if (!_IsNeedRunWithoutCheckIsDone &&
            !m_Project->GetVariantExcelTables().GetIsImportDataToWbiToolsServerNeedRebuild())
        {
            return;
        }

        UploadImagesAndPrepareXlsxForWbiTools(false);

        PythonCommand pythonCommand("./assets/scripts/import_to_server.py");
        pythonCommand.AddPathArg(
            m_Project->GetVariantExcelTablesHelpers().GetXlsxForServerImportPath(kAddExtraInfoWbiToolsParser),
            "--xlsx_path");
        pythonCommand.AddArg(false, "--remove_previous_images");
        pythonCommand.AddArg(StrJoin(m_Project->GetVariantExcelTables().GetPageNamesToSkipOnServerImport(), ";"),
                             "--skip_files");

        ScriptPopup::Get()->AddToQueue(
            pythonCommand,
            { "Импорт данных в WBI Tools Server",
              []() {
                  ImGui::Text("Работает скрипт импорта данных в WBI Tools Server");
                  ImGui::Text("Это может занять несколько минут");
                  ImGui::Text("После его завершения можно закрыть это окно");
              },
              [this](int) { m_Project->GetVariantExcelTables().SetIsImportDataToWbiToolsServerNeedRebuild(false); } });
    }

}    // namespace LM
