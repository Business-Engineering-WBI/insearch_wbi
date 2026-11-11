#pragma once

#include "Database/DatabaseData.hpp"
#include "IPageView.h"
#include "ImGui/Constructions/SelectConstructionFromTree.hpp"
#include "Project/Processing/XlsxPageViewData.h"

#include <functional>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

struct ImVec4;

namespace LM
{

    class XlsxPageView : public IPageView
    {
    protected:
        struct DrawTableHeaderReturn
        {
            std::optional<size_t> HoveredCol = std::nullopt;
        };

        struct SelectionRegion
        {
            size_t StartRow = 0;
            size_t StartCol = 0;
            size_t RowsCount = 0;
            size_t ColsCount = 0;
        };

        struct ExtraInfoAutoFocusField
        {
            std::string WindowName;
            std::string Field;
        };

        // Ключ кеша ширин колонок: пересчет ширин выполняется только при его изменении,
        // а не каждый кадр (иначе CalcTextSize вызывается для всех ячеек таблицы ежекадрово)
        struct TableWidthsCacheKey
        {
            const void* ViewData = nullptr;    // адрес XlsxPageViewData (защита от смены проекта)
            int PageId = -1;
            size_t Rows = 0;
            size_t Cols = 0;
            size_t HistoryPointer = 0;    // меняется при PushHistory/Undo/Redo
            size_t HistoryStates = 0;
            bool ShowConstrExamples = false;
            float FontSize = 0.0f;

            bool operator==(const TableWidthsCacheKey&) const = default;
        };

        struct TableWidthsCache
        {
            std::vector<float> HeaderWidths;    // [0] - колонка номеров строк, [colId + 1] - заголовки
            std::vector<float> ColumnWidths;    // [0] не используется, [colId + 1] - данные колонок
        };

    public:
        XlsxPageView();
        virtual ~XlsxPageView();

        virtual bool OnPageWillBeChanged(int _CurrentPageId, int _NewPageId) override;

        virtual void Save() override;

    protected:
        virtual std::string GetBasePath() const override
        {
            return m_Project->GetVariantExcelTablesHelpers().GetXlsxStartupPath().string();
        }
        virtual const char* GetWindowName() const override { return "Первый Excel"; }
        std::string GetFileName() const override;

        virtual void Draw() override;

        void DrawWindowContent() override;
        void DrawOtherWindows() override;

    protected:
        void DrawTableActions(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData);
        void DrawGlobalAddListWindow(XlsxPageViewData& _XlsxViewData);
        void DrawGlobalCalcListWindow(XlsxPageViewData& _XlsxViewData);
        std::vector<std::string> GetSortedFieldNamesByHeaderAndColumnFill(XlsxPageViewData& _XlsxViewData) const;

        template <XlsxPageViewDataTypes::DerivedFromSimpleListItemBase T>
        void DrawSimpleListTemplateWindow(std::string_view _WindowName, XlsxPageViewData& _XlsxViewData,
                                          std::unordered_map<std::string, std::vector<T>>& _SimpleList,
                                          std::function<void(std::string_view, T&)> _ItemInputHandle,
                                          std::function<std::string(const T&)> _ItemPreviewTextFn);

        void DrawSimpleAddListWindow(XlsxPageViewData& _XlsxViewData);
        void DrawSimpleCalcListWindow(XlsxPageViewData& _XlsxViewData);
        bool PageImgListItemValueFilenameExists(XlsxPageViewData& _XlsxViewData, std::string_view _Hash);
        void DrawSimpleRuleImgListWindow(XlsxPageViewData& _XlsxViewData);
        void DrawImgsPerListWindow(XlsxPageViewData& _XlsxViewData);

        void DrawImgPreview(std::string_view _ImgFilename);
        bool DrawImgMakeScreenshot(std::string_view _ImgFilename);

        void DrawJoinModal(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData);
        void DrawFindAndReplaceModal(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData);

        void HandleAnyRelatedWindowsImGuiEvents();
        void HandleImGuiEvents(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData);

        void PushCellFrameBgColor(XlsxPageViewDataTypes::TableData& _TableData, bool _IsRowHovered, bool _IsColHovered,
                                  size_t _RowId, size_t _ColId, const SelectionRegion& _SelectionRegion);

        DrawTableHeaderReturn DrawTableHeader(size_t _ColsCount, XlsxPageViewData& _XlsxViewData,
                                              XlsxPageViewDataTypes::TableData& _TableData);
        void DrawConstrExampleItem(size_t _ItemIdx, size_t _ExampleRowsCount, const ConstrExample& _ConstrExample,
                                   size_t _ColsCount, XlsxPageViewDataTypes::TableData& _TableData);

        void InvalidateTableWidthsCache() { m_IsTableWidthsCacheValid = false; }
        void RecomputeTableWidthsCache(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData,
                                       const ConstrExample* _ConstrExample);
        void RecomputeColumnWidth(size_t _ColId, XlsxPageViewDataTypes::TableData& _TableData,
                                  const std::optional<const std::reference_wrapper<std::string>>& _ExtraValue,
                                  const ConstrExample* _ConstrExample);
        void DrawTableHeaderRowContextMenu(XlsxPageViewData& _XlsxViewData,
                                           XlsxPageViewDataTypes::TableData& _TableData);

        std::optional<const std::reference_wrapper<std::string>> GetExtraListValue(XlsxPageViewData& _XlsxViewData,
                                                                                   std::string_view _Header);

        std::string GetRawImgFilename(XlsxPageViewData& _XlsxViewData, std::string_view _Filetype);
        std::string GetSimpleRuleImgFilename(std::string_view _Filename);

        void ReplaceFromClipboard(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData,
                                  bool _IsNeedEmptyHeaderRow);

        SelectionRegion GetSelectionRegion(XlsxPageViewDataTypes::TableData& _TableData, bool _IncludeHeader);
        bool IsInSelectionRegion(const SelectionRegion& _SelectionRegion, size_t _RowId, size_t _ColId);

        void CopySelectedToClipboard(XlsxPageViewDataTypes::TableData& _TableData,
                                     const SelectionRegion& _SelectionRegion);
        void InsertFromClipboard(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData);
        void ClearSelected(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData,
                           const SelectionRegion& _SelectionRegion);

        void DuplicateCellToColumn(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData,
                                   const glm::u64vec2& _Cell);

        void UnSelectAll(bool _UnSelectExtraCell = true);

        void SplitAndExpandTable(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData);

        void FixDimensions(XlsxPageViewDataTypes::TableData& _TableData);

        void ChangeHeadersByConstruction(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData,
                                         std::string_view _ConstrKey);

        void DeleteUnknownColumnsForConstruction(XlsxPageViewData& _XlsxViewData,
                                                 XlsxPageViewDataTypes::TableData& _TableData);

        void LoadAmatiCodems();
        const std::unordered_map<std::string, std::string>& GetAmatiCodemsWithLoad();

        void FindAndInsertAmatiCodems(XlsxPageViewData& _XlsxViewData, XlsxPageViewDataTypes::TableData& _TableData);

        bool IsExtraInfoAutoFocusField(std::string_view _WindowName, std::string_view _FieldName);

        void DrawProcessingScriptsWindow();
        void DrawImgsToSkipBgRemoveWindow();

        void ProcessAddExtraInfo(bool _IsNeedRunWithoutCheckIsDone = true);

        void ProcessImages(bool _IsNeedRunWithoutCheckIsDone = true);

        void UploadImagesAndPrepareXlsxForWbiTools(bool _IsNeedRunWithoutCheckIsDone = true);

        void ViewNotInDbAdintFields(bool _IsNeedRunWithoutCheckIsDone = true);
        void AddNotInDbAdintFieldsToServer(bool _IsNeedRunWithoutCheckIsDone = true);

        void ImportDataToWbiToolsServer(bool _IsNeedRunWithoutCheckIsDone = true);

        void UploadImagesForYg1Shop(bool _IsNeedRunWithoutCheckIsDone = true);

        void GenerateYg1ShopCombinedXlsx(bool _IsNeedRunWithoutCheckIsDone = true);

        void PushHistory(XlsxPageViewData& _XlsxViewData);

    protected:
        bool m_IsMainWindowFocused = false;
        bool m_IsMainWindowAndChildFocused = false;
        bool m_IsAnyOtherRelatedWindowFocused = false;
        bool m_IsAnyCellActive = false;
        bool m_IsAnyHeaderActive = false;
        std::optional<glm::u64vec2> m_SelectedCell = std::nullopt;
        std::optional<glm::u64vec2> m_ExtraSelectedCell = std::nullopt;
        std::optional<size_t> m_SelectedCol = std::nullopt;
        std::optional<size_t> m_SelectedRow = std::nullopt;
        std::optional<size_t> m_DeleteCol = std::nullopt;
        std::optional<size_t> m_DeleteRow = std::nullopt;

        std::optional<ExtraInfoAutoFocusField> m_ExtraInfoAutoFocusField = std::nullopt;

        std::unordered_map<std::string, std::string> m_AmatiCodems;
        bool m_IsAmatiCodemsLoaded = false;

        bool m_IsJoinModalOpen = false;
        bool m_IsFindAndReplaceModalOpen = false;

        bool m_IsShowConstrExamples = false;

        TableWidthsCache m_TableWidthsCache;
        TableWidthsCacheKey m_TableWidthsCacheKey;
        bool m_IsTableWidthsCacheValid = false;

        std::string m_TMP_SkippedBgRemoveImgNames;
    };

}    // namespace LM
