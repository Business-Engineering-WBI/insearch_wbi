#include "Project/Project.h"

#include "ImGui/Overlays/Overlay.h"
#include "Serializer/Serializer.h"

#include "Engine/Utils/FileDialogs.h"
#include "Engine/Utils/Log.hpp"
#include "Engine/Utils/json.hpp"
#include "Engine/Utils/utf8.h"

#include <chrono>
#include <filesystem>
#include <format>
#include <fstream>

namespace LM
{

    const std::vector<nfdfilteritem_t> kFileDialogsFilter = {
        { "InSearch Project (*.lmproj)", "lmproj" },
    };

    static bool WriteJsonToFile(std::string_view _FileName, nlohmann::json _Json)
    {
        std::ofstream fout(_FileName.data());
        if (!fout.is_open())
        {
            Overlay::Get()->Start(Format("Не удалось сохранить проект: \n{}", _FileName));
            return false;
        }
        fout << std::setw(4) << _Json;

        return true;
    }

    Project::Project(std::string_view _Folder) : m_Folder(_Folder) { }

    Ref<Project> Project::New(std::string_view _Folder, std::string_view _Name)
    {
        std::filesystem::path folder = std::filesystem::path(_Folder) / std::filesystem::path(_Name);
        Ref<Project> project = Ref<Project>(new Project(folder.string()));

        if (std::filesystem::create_directories(project->m_Folder) && Save(project))
        {
            project->m_LastBuildCatalog = Catalog::CreateDefaultLastBuildCatalog();
            return project;
        }

        return s_ProjectNotOpen;
    }

    Ref<Project> Project::Open()
    {
        std::string filename = FileDialogs::OpenFile(kFileDialogsFilter);
        if (filename == std::string())
        {
            return s_ProjectNotOpen;
        }

        return Open(filename);
    }

    Ref<Project> Project::Open(std::string _FileName)
    {
        Ref<Project> project = Ref<Project>(new Project());

        std::ifstream infile(_FileName);
        if (!infile.is_open())
        {
            Overlay::Get()->Start(Format("Не удалось открыть проект: \n{}", _FileName));
            return s_ProjectNotOpen;
        }

        try
        {
            nlohmann::json json;
            infile >> json;

            if (!SerializerLast().DeSerialize(project, json))
            {
                Overlay::Get()->Start(Format("Не верный формат проекта: \n{}", _FileName));
                return s_ProjectNotOpen;
            }
            project->m_LastBuildCatalog =
                project->m_Catalog.NeedRebuild ? Catalog::CreateDefaultLastBuildCatalog() : project->m_Catalog;
            Overlay::Get()->Start(Format("Проект успешно открыт: \n{}", _FileName));

            project->MakeBackup();

            return project;
        }
        catch (...)
        {
            Overlay::Get()->Start(Format("Ошибка во время чтения формата json: \n{}", _FileName));
        }

        return s_ProjectNotOpen;
    }

    bool Project::Save(Ref<Project> _Project)
    {
        if (_Project == s_ProjectNotOpen || _Project->GetFolder().empty())
        {
            return false;
        }

        return WriteJsonToFile(_Project->GetPathInFolder(s_ProjectFileName), SerializerLast().Serialize(_Project));
    }

    const std::string Project::GetPathInFolder(std::string_view _Path) const
    {
        return (std::filesystem::path(m_Folder) / std::filesystem::path(_Path)).string();
    }

    std::string Project::GetPathInFolderAndCreateDirs(std::string_view _Path) const
    {
        std::string path = GetPathInFolder(_Path);
        std::filesystem::create_directories(path);
        return path;
    }

    std::string Project::GetPdfTablesWithOcrTypeRawImgPath() const
    {
        return GetPathInFolderAndCreateDirs("data/catalog/raw_img/");
    }

    std::string Project::GetPdfTablesWithOcrTypeRawImgPrevPath() const
    {
        return GetPathInFolderAndCreateDirs("data/catalog/prev_raw_img/");
    }

    std::string Project::GetPdfTablesWithOcrTypeCutByPatternImgsPath() const
    {
        return GetPathInFolderAndCreateDirs("data/catalog/cut_by_pattern_img/");
    }

    std::string Project::GetPdfTablesWithOcrTypeCutByPatternImgsPrevPath() const
    {
        return GetPathInFolderAndCreateDirs("data/catalog/prev_cut_by_pattern_img/");
    }

    std::string Project::GetPdfTablesWithOcrTypeRawExcelPath() const
    {
        return GetPathInFolderAndCreateDirs("data/catalog/raw_xlsx/");
    }

    void Project::MakeBackup()
    {
        try
        {
            auto now = std::chrono::system_clock::now();
            std::string timestamp = std::format("{:%Y-%m-%d_%H-%M-%S}", now);
            std::filesystem::path source = GetDataFolder();
            std::filesystem::path backup = GetBackupFolder() / ("data_" + timestamp);

            std::filesystem::create_directories(backup);

            std::filesystem::copy(source, backup,
                                  std::filesystem::copy_options::recursive |
                                      std::filesystem::copy_options::overwrite_existing);

            LOG_CORE_INFO("Бэкап завершён успешно.");
        }
        catch (const std::filesystem::filesystem_error& e)
        {
            Overlay::Get()->Start(Format("Не удалось сохранить бэкап: \n{}", e.what()));
        }
    }

    bool Project::IsPageInGeneratedCatalogExcludePages(uint32_t _PageId) const
    {
        const auto itPageId = std::find(m_Catalog.GeneratedCatalogExcludePages.begin(),
                                        m_Catalog.GeneratedCatalogExcludePages.end(), _PageId);
        return itPageId != m_Catalog.GeneratedCatalogExcludePages.end();
    }

    bool Project::AddGeneratedCatalogExcludePage(uint32_t _PageId)
    {
        if (IsPageInGeneratedCatalogExcludePages(_PageId))
        {
            return false;
        }

        m_Catalog.GeneratedCatalogExcludePages.push_back(_PageId);
        std::sort(m_Catalog.GeneratedCatalogExcludePages.begin(), m_Catalog.GeneratedCatalogExcludePages.end());

        return true;
    }

    bool Project::RemoveGeneratedCatalogExcludePage(uint32_t _PageId)
    {
        if (!IsPageInGeneratedCatalogExcludePages(_PageId))
        {
            return false;
        }

        m_Catalog.GeneratedCatalogExcludePages.erase(std::find(m_Catalog.GeneratedCatalogExcludePages.begin(),
                                                               m_Catalog.GeneratedCatalogExcludePages.end(), _PageId));

        return true;
    }

    void Project::OnGenCatalogRawImgs()
    {
        m_LastBuildCatalog = m_Catalog;
        m_Catalog.IsGenerated = true;
        m_Catalog.NeedRebuild = false;
        m_Catalog.GenImgsByCutPattern.NeedRebuild = true;
    }

    void Project::OnGenImgsByCutPattern()
    {
        m_Catalog.GenImgsByCutPattern.IsGenerated = true;
        m_Catalog.GenImgsByCutPattern.NeedRebuild = false;
        m_Catalog.GenImgsByCutPattern.Version = 0;

        if (m_Catalog.GenRawExcel.UseCutPattern)
        {
            m_Catalog.GenRawExcel.NeedRebuild = true;
        }
    }

    void Project::OnGenRawExcel()
    {
        m_Catalog.GenRawExcel.IsGenerated = true;
        m_Catalog.GenRawExcel.NeedRebuild = false;
        m_Catalog.GenRawExcel.Version = 0;
    }

    XlsxPageViewData& Project::GetXlsxPageViewData()
    {
        if (!m_XlsxPageViewData.has_value())
        {
            m_XlsxPageViewData = XlsxPageViewData(m_VariantExcelTablesHelpers.GetBasePath(),
                                                  m_VariantExcelTablesHelpers.GetXlsxStartupPath());
        }
        return m_XlsxPageViewData.value();
    }

    void Project::ClearXlsxPageViewData() { m_XlsxPageViewData.reset(); }

}    // namespace LM
