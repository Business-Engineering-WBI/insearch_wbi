#include "ProjectVariantExcelTables.hpp"

#include "Project.h"

#include <Engine/Core/Assert.h>
#include <format>

namespace LM
{

    std::filesystem::path ProjectVariantExcelTablesHelpers::GetBasePath() const
    {
        return m_Owner->GetPathInFolderAndCreateDirs("data/excel/");
    }

    std::filesystem::path ProjectVariantExcelTablesHelpers::GetXlsxStartupPath() const
    {
        return m_Owner->GetPathInFolderAndCreateDirs("data/excel/xlsx_startup/");
    }

    std::filesystem::path ProjectVariantExcelTablesHelpers::GetXlsxAddExtraInfoPath() const
    {
        return m_Owner->GetPathInFolderAndCreateDirs("data/excel/xlsx_add_info/");
    }

    std::filesystem::path ProjectVariantExcelTablesHelpers::GetXlsxAddExtraInfoWithProcessedImagesPath() const
    {
        return m_Owner->GetPathInFolderAndCreateDirs("data/excel/xlsx_add_info_with_processed_images/");
    }

    std::filesystem::path
    ProjectVariantExcelTablesHelpers::GetXlsxForServerImportPath(std::string_view _ParserName) const
    {
        return m_Owner->GetPathInFolderAndCreateDirs(std::format(
            "data/excel/xlsx_for_server_import{}/", _ParserName == kAddExtraInfoWbiToolsParser ? "" : "_yg1-shop"));
    }

    std::filesystem::path ProjectVariantExcelTablesHelpers::GetImgsPerPagePath() const
    {
        return m_Owner->GetPathInFolderAndCreateDirs("data/excel/img_per_page/");
    }

    std::filesystem::path ProjectVariantExcelTablesHelpers::GetImgsSimpleRulePath() const
    {
        return m_Owner->GetPathInFolderAndCreateDirs("data/excel/img_simple_rule/");
    }

    std::filesystem::path ProjectVariantExcelTablesHelpers::GetImgsNoConditionPath() const
    {
        return m_Owner->GetPathInFolderAndCreateDirs("data/excel/img_no_condition/");
    }

    std::filesystem::path ProjectVariantExcelTablesHelpers::GetImgsProcessedPath() const
    {
        return m_Owner->GetPathInFolderAndCreateDirs("data/excel/img_processed/");
    }

    std::filesystem::path ProjectVariantExcelTablesHelpers::GetJsonPrevProcessedImagesFilePath() const
    {
        return m_Owner->GetPathInFolder("data/excel/prev_imgs_hash_and_map.json");
    }

    void ProjectVariantExcelTables::TogglePageNameToSkipOnServerImport(const std::string& pageName)
    {
        auto it = std::find(m_PageNamesToSkipOnServerImport.begin(), m_PageNamesToSkipOnServerImport.end(), pageName);
        if (it != m_PageNamesToSkipOnServerImport.end())
        {
            m_PageNamesToSkipOnServerImport.erase(it);
        }
        else
        {
            m_PageNamesToSkipOnServerImport.push_back(pageName);
        }
    }

    void ProjectVariantExcelTables::TogglePicImgNameToSkipBgRemove(const std::string& _ImgName)
    {
        auto it = std::find(m_PicImgNamesToSkipBgRemove.begin(), m_PicImgNamesToSkipBgRemove.end(), _ImgName);
        if (it != m_PicImgNamesToSkipBgRemove.end())
        {
            m_PicImgNamesToSkipBgRemove.erase(it);
        }
        else
        {
            m_PicImgNamesToSkipBgRemove.push_back(_ImgName);
        }
    }

    void ProjectVariantExcelTables::SetIsAddExtraInfoNeedRebuild(bool _NeedRebuild)
    {
        IsAddExtraInfoNeedRebuild = _NeedRebuild;
        if (_NeedRebuild)
        {
            SetIsProcessImagesNeedRebuild(true);
        }
    }

    void ProjectVariantExcelTables::SetIsProcessImagesNeedRebuild(bool _NeedRebuild)
    {
        IsProcessImagesNeedRebuild = _NeedRebuild;
        if (_NeedRebuild)
        {
            SetIsUploadImagesAndPrepareXlsxForWbiToolsNeedRebuild(true);
        }
    }

    void ProjectVariantExcelTables::SetIsUploadImagesAndPrepareXlsxForWbiToolsNeedRebuild(bool _NeedRebuild)
    {
        IsUploadImagesAndPrepareXlsxForWbiToolsNeedRebuild = _NeedRebuild;
        if (_NeedRebuild)
        {
            SetIsImportDataToWbiToolsServerNeedRebuild(true);
        }
    }

    void ProjectVariantExcelTables::SetIsUploadImagesForYg1ShopNeedRebuild(bool _NeedRebuild)
    {
        IsUploadImagesForYg1ShopNeedRebuild = _NeedRebuild;
    }

    void ProjectVariantExcelTables::SetIsImportDataToWbiToolsServerNeedRebuild(bool _NeedRebuild)
    {
        IsImportDataToWbiToolsServerNeedRebuild = _NeedRebuild;
    }

    void ProjectVariantExcelTables::SetIsGenerateYg1ShopCombinedXlsxNeedRebuild(bool _NeedRebuild)
    {
        IsGenerateYg1ShopCombinedXlsxNeedRebuild = _NeedRebuild;
    }

}    // namespace LM
