#pragma once

#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <unordered_map>

namespace LM
{

    namespace XlsxPageViewDataTypes
    {

        enum class CheckStatus
        {
            kNone,
            kOk,
            kWarning,
            kError,
        };

        struct TableCell
        {
            std::string Value = "";
            CheckStatus Check = CheckStatus::kNone;
        };

        struct HistoryState
        {
            std::vector<std::vector<TableCell>> DataTable;

            // std::optional<glm::u64vec2> SelectedCell = std::nullopt;
            // std::optional<size_t> SelectedCol = std::nullopt;
            // std::optional<size_t> SelectedRow = std::nullopt;
        };

        struct SimpleListItemBase
        {
            std::vector<int> SharedPages;
        };

        template <typename T>
        concept DerivedFromSimpleListItemBase = std::is_base_of_v<SimpleListItemBase, T>;

        struct SimpleAddListItem : public SimpleListItemBase
        {
            std::string Value;
        };

        struct PageImgListItemValue
        {
            std::string Cmp = "EQ";
            std::string CmpValue;
            std::string ImgFilenameHash;
        };

        struct PageImgListItem : public SimpleListItemBase
        {
            std::vector<PageImgListItemValue> Value;
        };

        typedef std::vector<std::vector<XlsxPageViewDataTypes::TableCell>> TableData;

        typedef std::unordered_map<std::string, std::string> GlobalAddList;
        typedef std::unordered_map<std::string, std::string> GlobalCalcList;
        typedef std::unordered_map<std::string, std::vector<XlsxPageViewDataTypes::SimpleAddListItem>> SimpleAddList;
        typedef std::unordered_map<std::string, std::vector<XlsxPageViewDataTypes::SimpleAddListItem>> SimpleCalcList;
        typedef std::unordered_map<std::string, std::vector<XlsxPageViewDataTypes::PageImgListItem>> SimpleRuleImgList;

    }    // namespace XlsxPageViewDataTypes

    class XlsxPageViewPageData
    {
    public:
        XlsxPageViewPageData(int _PageId, const std::filesystem::path& _PageFilename);
        ~XlsxPageViewPageData();

        void LoadXLSX();
        void SaveXLSX();

        bool IsLoaded() const { return m_IsLoaded; }

        int GetPageId() const { return m_PageId; }
        const std::filesystem::path& GetPageFilename() const { return m_PageFilename; }
        XlsxPageViewDataTypes::TableData& GetTableData() { return m_TableData; }

        void PushHistory();

        void Undo();
        void Redo();

        void RestoreFromHistory(const XlsxPageViewDataTypes::HistoryState& _HistoryState);
        void ClearHistory();

        void DeleteCol(size_t _ColId);
        void DeleteRow(size_t _RowId);

        void InsertCol(size_t _ColId);
        void InsertRow(size_t _RowId);

    protected:
        bool m_IsLoaded = false;
        int m_PageId = -1;
        std::filesystem::path m_PageFilename;

        XlsxPageViewDataTypes::TableData m_TableData;

        size_t m_HistoryPointer = 0;
        std::vector<XlsxPageViewDataTypes::HistoryState> m_HistoryState;
    };

    class XlsxPageViewData
    {
    public:
        XlsxPageViewData(const std::filesystem::path& _VariantExcelTablesBasePath,
                         const std::filesystem::path& _VariantExcelTablesXlsxStartupPath);
        ~XlsxPageViewData();

        void SaveExtraInfoJson();
        void SavePageData();

        int GetCurrentPageId() const;

        XlsxPageViewPageData& GetCurrentPageData() { return m_CurrentPageData.value(); }

        const std::filesystem::path& GetExtraInfoJsonPath() const { return m_ExtraInfoJsonPath; }

        XlsxPageViewDataTypes::GlobalAddList& GetGlobalAddList() { return m_GlobalAddList; }
        XlsxPageViewDataTypes::GlobalCalcList& GetGlobalCalcList() { return m_GlobalCalcList; }
        XlsxPageViewDataTypes::SimpleAddList& GetSimpleAddList() { return m_SimpleAddList; }
        XlsxPageViewDataTypes::SimpleCalcList& GetSimpleCalcList() { return m_SimpleCalcList; }
        XlsxPageViewDataTypes::SimpleRuleImgList& GetSimpleRuleImgList() { return m_SimpleRuleImgList; }

        void LoadPageData(int _PageId);
        bool IsPageLoaded(int _PageId);

        void PushHistory();

        std::optional<const std::reference_wrapper<XlsxPageViewDataTypes::SimpleAddListItem>>
        GetItemInSimpleAddListForCurrentPage(std::string_view _FieldName)
        {
            return GetItemInSimpleListForCurrentPage(m_SimpleAddList, _FieldName);
        }

        std::optional<const std::reference_wrapper<XlsxPageViewDataTypes::SimpleAddListItem>>
        GetItemInSimpleCalcListForCurrentPage(std::string_view _FieldName)
        {
            return GetItemInSimpleListForCurrentPage(m_SimpleCalcList, _FieldName);
        }

        template <XlsxPageViewDataTypes::DerivedFromSimpleListItemBase T>
        void DeleteFromSimpleListForCurrentPage(std::unordered_map<std::string, std::vector<T>>& _SimpleList,
                                                std::string_view _FieldName);

        void DeleteFromSimpleAddListForCurrentPage(std::string_view _FieldName)
        {
            DeleteFromSimpleListForCurrentPage(m_SimpleAddList, _FieldName);
        }

        template <XlsxPageViewDataTypes::DerivedFromSimpleListItemBase T>
        bool IsItemInSimpleListForCurrentPage(std::unordered_map<std::string, std::vector<T>>& _SimpleList,
                                              std::string_view _FieldName);

        bool IsItemInGlobalAddList(std::string_view _FieldName) { return m_GlobalAddList.contains(_FieldName.data()); }

        bool IsItemInGlobalCalcList(std::string_view _FieldName)
        {
            return m_GlobalCalcList.contains(_FieldName.data());
        }

        bool IsItemInSimpleAddListForCurrentPage(std::string_view _FieldName)
        {
            return IsItemInSimpleListForCurrentPage(m_SimpleAddList, _FieldName);
        }

        bool IsItemInSimpleCalcListForCurrentPage(std::string_view _FieldName)
        {
            return IsItemInSimpleListForCurrentPage(m_SimpleCalcList, _FieldName);
        }

        bool IsItemInAnyListForCurrentPage(std::string_view _FieldName)
        {
            return IsItemInGlobalAddList(_FieldName) || IsItemInGlobalCalcList(_FieldName) ||
                   IsItemInSimpleAddListForCurrentPage(_FieldName) || IsItemInSimpleCalcListForCurrentPage(_FieldName);
        }

    protected:
        void LoadExtraInfoJson();

        // template <XlsxPageViewDataTypes::DerivedFromSimpleListItemBase T>
        // void SetItemInSimpleListForCurrentPage(std::unordered_map<std::string, std::vector<T>>& _SimpleList,
        //                                        std::string_view _FieldName, const T& _ItemValue);

        template <XlsxPageViewDataTypes::DerivedFromSimpleListItemBase T>
        std::optional<const std::reference_wrapper<T>>
        GetItemInSimpleListForCurrentPage(std::unordered_map<std::string, std::vector<T>>& _SimpleList,
                                          std::string_view _FieldName);

    protected:
        std::filesystem::path m_VariantExcelTablesBasePath;
        std::filesystem::path m_VariantExcelTablesXlsxStartupPath;
        std::filesystem::path m_ExtraInfoJsonPath;
        std::optional<XlsxPageViewPageData> m_CurrentPageData;

        XlsxPageViewDataTypes::GlobalAddList m_GlobalAddList;
        XlsxPageViewDataTypes::GlobalCalcList m_GlobalCalcList;
        XlsxPageViewDataTypes::SimpleAddList m_SimpleAddList;
        XlsxPageViewDataTypes::SimpleCalcList m_SimpleCalcList;
        XlsxPageViewDataTypes::SimpleRuleImgList m_SimpleRuleImgList;
    };

    template <XlsxPageViewDataTypes::DerivedFromSimpleListItemBase T>
    void
    XlsxPageViewData::DeleteFromSimpleListForCurrentPage(std::unordered_map<std::string, std::vector<T>>& _SimpleList,
                                                         std::string_view _FieldName)
    {
        if (!_FieldName.empty())
        {
            auto& items = _SimpleList[_FieldName.data()];

            if (auto it = std::ranges::find_if(items,
                                               [this](T& item) {
                                                   return std::ranges::find(item.SharedPages, GetCurrentPageId()) !=
                                                          item.SharedPages.end();
                                               });
                it != items.end())
            {
                std::erase(it->SharedPages, GetCurrentPageId());
            }

            std::erase_if(items, [](const T& item) { return item.SharedPages.empty(); });

            if (items.empty())
            {
                _SimpleList.erase(_FieldName.data());
            }
        }
    }

    template <XlsxPageViewDataTypes::DerivedFromSimpleListItemBase T>
    bool
    XlsxPageViewData::IsItemInSimpleListForCurrentPage(std::unordered_map<std::string, std::vector<T>>& _SimpleList,
                                                       std::string_view _FieldName)
    {
        return _SimpleList.contains(_FieldName.data()) &&
               std::ranges::any_of(_SimpleList[_FieldName.data()], [this](const T& item) {
                   return std::ranges::find(item.SharedPages, GetCurrentPageId()) != item.SharedPages.end();
               });
    }

    template <XlsxPageViewDataTypes::DerivedFromSimpleListItemBase T>
    std::optional<const std::reference_wrapper<T>>
    XlsxPageViewData::GetItemInSimpleListForCurrentPage(std::unordered_map<std::string, std::vector<T>>& _SimpleList,
                                                        std::string_view _FieldName)
    {
        if (_SimpleList.contains(_FieldName.data()))
        {
            std::vector<T>& items = _SimpleList[_FieldName.data()];
            if (auto it = std::ranges::find_if(items,
                                               [this](T& item) {
                                                   return std::ranges::find(item.SharedPages, GetCurrentPageId()) !=
                                                          item.SharedPages.end();
                                               });
                it != items.end())
            {
                return *it;
            }
        }

        return std::nullopt;
    }

}    // namespace LM
