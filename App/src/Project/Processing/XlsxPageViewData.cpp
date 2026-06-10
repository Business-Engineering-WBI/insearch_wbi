#include "XlsxPageViewData.h"

#include "Engine/Utils/Log.hpp"
#include "Engine/Utils/json.hpp"
#include "Engine/Utils/utf8.h"
#include "ImGui/Overlays/Overlay.h"

#include <algorithm>
#include <fstream>
#include <vector>

#include <xlnt/xlnt.hpp>

namespace LM
{

    constexpr std::string_view kExtraInfoFile = "extra_info.json";

    // ######################################################################################################
    // ################ XlsxPageViewData ####################################################################
    // ######################################################################################################

    XlsxPageViewData::XlsxPageViewData(const std::filesystem::path& _VariantExcelTablesBasePath,
                                       const std::filesystem::path& _VariantExcelTablesXlsxStartupPath)
        : m_VariantExcelTablesBasePath(_VariantExcelTablesBasePath),
          m_VariantExcelTablesXlsxStartupPath(_VariantExcelTablesXlsxStartupPath)
    {
        LoadExtraInfoJson();
    }

    XlsxPageViewData::~XlsxPageViewData() { SaveExtraInfoJson(); }

    void XlsxPageViewData::SaveExtraInfoJson()
    {
        std::ofstream fout(m_ExtraInfoJsonPath);
        if (!fout.is_open())
        {
            Overlay::Get()->Start(Format("Не удалось сохранить ExtraInfo: \n{}", m_ExtraInfoJsonPath.string()));
            return;
        }

        nlohmann::json result;

        // result["global_add_list"] = nlohmann::json::array();
        for (const auto& [name, value] : m_GlobalAddList)
        {
            result["global_add_list"].push_back(nlohmann::json {
                {  "name",  name },
                { "value", value }
            });
        }

        for (const auto& [name, items] : m_SimpleAddList)
        {
            if (items.size() == 0)
            {
                continue;
            }

            nlohmann::json jsonItems = {
                {   "name",                    name },
                { "values", nlohmann::json::array() }
            };
            for (const XlsxPageViewDataTypes::SimpleAddListItem& item : items)
            {
                jsonItems["values"].push_back(nlohmann::json {
                    { "index", item.SharedPages },
                    { "value",       item.Value }
                });
            }

            result["simple_add_list"].push_back(jsonItems);
        }

        for (const auto& [name, items] : m_SimpleCalcList)
        {
            if (items.size() == 0)
            {
                continue;
            }

            nlohmann::json jsonItems = {
                {   "name",                    name },
                { "values", nlohmann::json::array() }
            };
            for (const XlsxPageViewDataTypes::SimpleAddListItem& item : items)
            {
                jsonItems["values"].push_back(nlohmann::json {
                    { "index", item.SharedPages },
                    {  "exec",       item.Value }
                });
            }

            result["per_page_calc_list"].push_back(jsonItems);
        }

        for (const auto& [name, items] : m_SimpleRuleImgList)
        {
            if (items.size() == 0)
            {
                continue;
            }

            nlohmann::json jsonItems = {
                {   "name",                    name },
                { "values", nlohmann::json::array() }
            };
            for (const XlsxPageViewDataTypes::PageImgListItem& item : items)
            {
                nlohmann::json jsonList = nlohmann::json::array();
                for (const XlsxPageViewDataTypes::PageImgListItemValue& listItem : item.Value)
                {
                    jsonList.push_back(nlohmann::json {
                        {               "cmp",             listItem.Cmp },
                        {         "cmp_value",        listItem.CmpValue },
                        { "img_filename_hash", listItem.ImgFilenameHash }
                    });
                }
                jsonItems["values"].push_back(nlohmann::json {
                    { "index", item.SharedPages },
                    {  "list",         jsonList }
                });
            }

            result["per_page_simple_rule_img_list"].push_back(jsonItems);
        }

        fout << std::setw(4) << result;
    }

    void XlsxPageViewData::SavePageData()
    {
        if (m_CurrentPageData.has_value())
        {
            m_CurrentPageData->SaveXLSX();
        }
    }

    int XlsxPageViewData::GetCurrentPageId() const
    {
        if (m_CurrentPageData.has_value())
        {
            return m_CurrentPageData->GetPageId();
        }
        return -1;
    }

    void XlsxPageViewData::LoadPageData(int _PageId)
    {
        if (!IsPageLoaded(_PageId))
        {
            if (_PageId < 0)
            {
                LOG_CORE_ERROR("Invalid page ID: {}", _PageId);
                return;
            }

            std::vector<std::filesystem::path> xlsxPaths;
            for (const auto& entry : std::filesystem::directory_iterator(m_VariantExcelTablesXlsxStartupPath))
            {
                if (!entry.is_regular_file())
                {
                    continue;
                }

                std::filesystem::path path = entry.path();
                if (path.extension() == ".xlsx")
                {
                    xlsxPaths.push_back(path);
                }
            }

            std::ranges::sort(xlsxPaths);

            if (static_cast<size_t>(_PageId) >= xlsxPaths.size())
            {
                LOG_CORE_ERROR("No file found for page ID: {}", _PageId);
                return;
            }

            std::filesystem::path path = xlsxPaths[static_cast<size_t>(_PageId)];

            if (m_CurrentPageData.has_value() && m_CurrentPageData->IsLoaded())
            {
                SaveExtraInfoJson();
            }

            m_CurrentPageData.emplace(_PageId, path);
        }
    }

    bool XlsxPageViewData::IsPageLoaded(int _PageId)
    {
        return m_CurrentPageData.has_value() && m_CurrentPageData->IsLoaded() &&
               (m_CurrentPageData->GetPageId() == _PageId);
    }

    void XlsxPageViewData::PushHistory()
    {
        if (m_CurrentPageData.has_value() && m_CurrentPageData.value().IsLoaded())
        {
            m_CurrentPageData.value().PushHistory();
        }
    }

    void XlsxPageViewData::LoadExtraInfoJson()
    {
        std::filesystem::path inFilePath =
            (std::filesystem::path(m_VariantExcelTablesBasePath) / std::filesystem::path(kExtraInfoFile));
        m_ExtraInfoJsonPath = inFilePath.string();
        if (!std::filesystem::exists(inFilePath))
        {
            // Overlay::Get()->Start(Format("Файл не найден: \n{}", inFilePath.string()));
            return;
        }

        std::ifstream infile(inFilePath);
        if (!infile.is_open())
        {
            Overlay::Get()->Start(Format("Не удалось открыть файл ExtraInfo: \n{}", inFilePath.string()));
        }

        try
        {
            nlohmann::json json;
            infile >> json;

            if (json.contains("global_add_list"))
            {
                for (const auto& item : json["global_add_list"])
                {
                    m_GlobalAddList[item["name"]] = item["value"];
                }
            }

            if (json.contains("simple_add_list"))
            {
                for (const auto& item : json["simple_add_list"])
                {
                    m_SimpleAddList.try_emplace(item["name"]);
                    for (const auto& values : item["values"])
                    {
                        std::vector<int> index;
                        values["index"].get_to(index);
                        m_SimpleAddList[item["name"]].push_back({ { index }, values["value"] });
                    }
                }
            }

            if (json.contains("per_page_calc_list"))
            {
                for (const auto& item : json["per_page_calc_list"])
                {
                    m_SimpleCalcList.try_emplace(item["name"]);
                    for (const auto& values : item["values"])
                    {
                        std::vector<int> index;
                        values["index"].get_to(index);
                        m_SimpleCalcList[item["name"]].push_back({ { index }, values["exec"] });
                    }
                }
            }

            if (json.contains("per_page_simple_rule_img_list"))
            {
                for (const auto& item : json["per_page_simple_rule_img_list"])
                {
                    m_SimpleRuleImgList.try_emplace(item["name"]);
                    for (const auto& values : item["values"])
                    {
                        std::vector<int> index;
                        values["index"].get_to(index);

                        std::vector<XlsxPageViewDataTypes::PageImgListItemValue> list;

                        for (const auto& listItem : values["list"])
                        {
                            list.push_back({
                                .Cmp = listItem["cmp"],
                                .CmpValue = listItem["cmp_value"],
                                .ImgFilenameHash = listItem["img_filename_hash"],
                            });
                        }

                        m_SimpleRuleImgList[item["name"]].push_back({ { index }, list });
                    }
                }
            }
        }
        catch (...)
        {
            Overlay::Get()->Start(Format("Ошибка во время чтения формата json: \n{}", inFilePath.string()));
        }
    }

    // template <XlsxPageViewDataTypes::DerivedFromSimpleListItemBase T>
    // void
    // XlsxPageViewData::SetItemInSimpleListForCurrentPage(std::unordered_map<std::string, std::vector<T>>& _SimpleList,
    //                                                     std::string_view _FieldName, const T& _ItemValue)
    // {
    //     _SimpleList.try_emplace(_FieldName.data());
    //     // _SimpleList[_FieldName.data()].push_back(_ItemValue);
    // }

    // ######################################################################################################
    // ################ XlsxPageViewPageData ################################################################
    // ######################################################################################################

    XlsxPageViewPageData::XlsxPageViewPageData(int _PageId, const std::filesystem::path& _PageFilename)
        : m_IsLoaded(false),
          m_PageId(_PageId),
          m_PageFilename(_PageFilename)
    {
        LoadXLSX();
    }

    XlsxPageViewPageData::~XlsxPageViewPageData() { SaveXLSX(); }

    void XlsxPageViewPageData::LoadXLSX()
    {
        m_IsLoaded = false;
        m_TableData.clear();

        LOG_CORE_INFO("Loading file: {}", m_PageFilename.string());

        if (!std::filesystem::exists(m_PageFilename))
        {
            LOG_CORE_WARN("File does not exist: {}", m_PageFilename.string());
            return;
        }

        xlnt::workbook wb;

        try
        {
            wb.load(m_PageFilename);
        }
        catch (const std::exception& e)
        {
            LOG_CORE_ERROR("Failed to load workbook: {}", e.what());
            return;
        }

        xlnt::worksheet ws = wb.active_sheet();

        for (auto row : ws.rows(false))
        {
            std::vector<XlsxPageViewDataTypes::TableCell> rowData;
            for (auto cell : row)
            {
                rowData.push_back({ .Value = cell.has_value() ? cell.to_string() : "" });
            }
            m_TableData.push_back(rowData);
        }

        size_t maxCols = 0;
        for (const auto& row : m_TableData)
        {
            if (row.size() > maxCols)
            {
                maxCols = row.size();
            }
        }

        for (auto& row : m_TableData)
        {
            while (row.size() < maxCols)
            {
                row.push_back({ .Value = "" });
            }
        }

        m_IsLoaded = true;

        PushHistory();
    }

    void XlsxPageViewPageData::SaveXLSX()
    {
        if (!IsLoaded())
        {
            return;
        }

        LOG_CORE_INFO("Saving file: {}", m_PageFilename.string());

        // if (!std::filesystem::exists(path))
        // {
        //     return;
        // }

        xlnt::workbook wb;
        LOG_CORE_INFO("Created WB");

        xlnt::worksheet ws = wb.active_sheet();
        LOG_CORE_INFO("Created WS");

        for (size_t rowId = 0; rowId < m_TableData.size(); ++rowId)
        {
            for (size_t colId = 0; colId < m_TableData[rowId].size(); ++colId)
            {
                ws.cell(static_cast<xlnt::column_t>(static_cast<xlnt::column_t::index_t>(colId + 1)),
                        static_cast<xlnt::row_t>(rowId + 1))
                    .value(StrTrim(m_TableData[rowId][colId].Value));
            }
        }

        wb.save(m_PageFilename);
    }

    void XlsxPageViewPageData::PushHistory()
    {
        m_HistoryState.erase(m_HistoryState.begin() + m_HistoryPointer, m_HistoryState.end());
        m_HistoryState.push_back({
            .DataTable = m_TableData,
        });
        ++m_HistoryPointer;
    }

    void XlsxPageViewPageData::Undo()
    {
        if (m_HistoryPointer <= 1)
        {
            return;
        }

        --m_HistoryPointer;
        RestoreFromHistory(m_HistoryState[m_HistoryPointer - 1]);
    }

    void XlsxPageViewPageData::Redo()
    {
        if (m_HistoryPointer >= m_HistoryState.size())
        {
            return;
        }

        ++m_HistoryPointer;
        RestoreFromHistory(m_HistoryState[m_HistoryPointer - 1]);
    }

    void XlsxPageViewPageData::RestoreFromHistory(const XlsxPageViewDataTypes::HistoryState& _HistoryState)
    {
        m_TableData = _HistoryState.DataTable;
        // TODO: fix restore of m_SelectedCell and m_ExtraSelectedCell
        // m_SelectedCell = _HistoryState.SelectedCell;
        // m_ExtraSelectedCell = std::nullopt;
        // m_SelectedCol = _HistoryState.SelectedRow;
        // m_SelectedRow = _HistoryState.SelectedRow;
    }

    void XlsxPageViewPageData::ClearHistory()
    {
        m_HistoryPointer = 0;
        m_HistoryState.clear();
    }

    void XlsxPageViewPageData::DeleteCol(size_t _ColId)
    {
        for (auto& row : m_TableData)
        {
            if (_ColId >= row.size())
            {
                continue;
            }
            row.erase(row.begin() + _ColId);
        }
        if ((m_TableData.size() > 0) && (m_TableData[0].size() == 0))
        {
            m_TableData.clear();
        }

        PushHistory();
    }

    void XlsxPageViewPageData::DeleteRow(size_t _RowId)
    {
        if (_RowId >= m_TableData.size())
        {
            return;
        }
        m_TableData.erase(m_TableData.begin() + _RowId);

        PushHistory();
    }

    void XlsxPageViewPageData::InsertCol(size_t _ColId)
    {
        bool isChanged = false;
        for (auto& row : m_TableData)
        {
            if (_ColId > row.size())
            {
                continue;
            }
            isChanged = true;
            row.insert(row.begin() + _ColId, { .Value = "" });
        }

        if (isChanged)
        {
            PushHistory();
        }
    }

    void XlsxPageViewPageData::InsertRow(size_t _RowId)
    {
        if (_RowId > m_TableData.size())
        {
            return;
        }
        size_t colsCount = m_TableData.size() > 0 ? m_TableData[0].size() : 1;
        m_TableData.insert(m_TableData.begin() + _RowId,
                           std::vector<XlsxPageViewDataTypes::TableCell>(colsCount, { .Value = "" }));

        PushHistory();
    }

}    // namespace LM
