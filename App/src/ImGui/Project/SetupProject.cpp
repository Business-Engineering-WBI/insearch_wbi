#include "SetupProject.h"

#include <filesystem>

#include <imgui.h>
#include <nfd.hpp>

#include "Engine/Utils/FileDialogs.h"
#include "Engine/Utils/Log.hpp"
#include "Engine/Utils/utf8.h"

#include "ImGui/Overlays/Overlay.h"
#include "ImGui/Overlays/ScriptPopup.h"
#include "Managers/TextureManager.h"
#include "Python/PythonCommand.h"
#include "Utils/FileFormat.h"
#include "Utils/FileSystemUtils.h"
#include "misc/cpp/imgui_stdlib.h"
#include "xlnt/workbook/workbook.hpp"

namespace LM
{

    const std::vector<nfdfilteritem_t> kFileDialogsXlsxFilter = {
        { "Excel xlsx", "xlsx" },
    };

    const std::vector<nfdfilteritem_t> kFileDialogsPdfFilter = {
        { "Pdf", "pdf" },
    };

    SetupProject::SetupProject() { }

    void SetupProject::Draw(Ref<Project> _Project)
    {
        if (!m_IsOpen || _Project == Project::s_ProjectNotOpen)
        {
            return;
        }

        if (ImGui::Begin("Настройка проекта"))
        {
            if (ImGui::TreeNodeEx("Базовые настройки", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
            {
                DrawProjectSettings(_Project);
                ImGui::TreePop();
            }

            switch (_Project->GetType())
            {
                case ProjectVariant::kPdfTablesWithOcr: {
                    if (ImGui::TreeNodeEx("Настройки PDF OCR",
                                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
                    {
                        DrawPdfOcrSettings(_Project);
                        ImGui::TreePop();
                    }
                    break;
                }
                case ProjectVariant::kPdfTablesWithoutOcr: {
                    if (ImGui::TreeNodeEx("Настройки PDF", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
                    {
                        DrawPdfSettings(_Project);
                        ImGui::TreePop();
                    }
                    break;
                }
                case ProjectVariant::kExcelTables: {
                    if (ImGui::TreeNodeEx("Настройки Excel каталога",
                                          ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
                    {
                        DrawRawExcelFolderSettings(_Project);
                        ImGui::TreePop();
                    }
                }
                break;
            }
        }
        ImGui::End();
    }
    void SetupProject::DrawProjectSettings(Ref<Project> _Project)
    {
        int projectType = static_cast<int>(_Project->GetType());

        ImGui::Text("Тип проекта: %s", ConvertProjectVariantToString(_Project->GetType()).c_str());

        if (ImGui::RadioButton(ConvertProjectVariantToString(ProjectVariant::kPdfTablesWithOcr).c_str(), &projectType,
                               0))
        {
            _Project->SetType(ProjectVariant::kPdfTablesWithOcr);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(ConvertProjectVariantToString(ProjectVariant::kPdfTablesWithoutOcr).c_str(),
                               &projectType, 1))
        {
            _Project->SetType(ProjectVariant::kPdfTablesWithoutOcr);
        }
        ImGui::SameLine();
        if (ImGui::RadioButton(ConvertProjectVariantToString(ProjectVariant::kExcelTables).c_str(), &projectType, 2))
        {
            _Project->SetType(ProjectVariant::kExcelTables);
        }

        ImGui::Spacing();

        ImGui::Text("Папка проекта: %s", _Project->GetFolder().c_str());
    }

    void SetupProject::DrawPdfOcrSettings(Ref<Project> _Project)
    {
        if (ImGui::TreeNodeEx("Настройки каталога", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            DrawCatalog(_Project);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Обрезание по паттерну", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            DrawImgsByCutPattern(_Project);
            ImGui::TreePop();
        }

        if (ImGui::TreeNodeEx("Генерация первого Excel", ImGuiTreeNodeFlags_DefaultOpen | ImGuiTreeNodeFlags_Framed))
        {
            DrawGenRawExcel(_Project);
            ImGui::TreePop();
        }
    }

    void SetupProject::DrawPdfSettings(Ref<Project> _Project) { }

    void SetupProject::DrawRawExcelFolderSettings(Ref<Project> _Project)
    {
        std::filesystem::path xlsxStartupPath = _Project->GetVariantExcelTablesHelpers().GetXlsxStartupPath();

        ImGui::Text("Папка с Excel: %s", xlsxStartupPath.string().c_str());

        ImGui::Spacing();

        static bool isNeedRebuild = true;
        // TODO: Fix (add number in front of file)
        // if (ImGui::Button("Добавить Excel файлы"))
        // {
        //     if (std::vector<std::string> filenames = FileDialogs::OpenMultipleFiles(kFileDialogsXlsxFilter);
        //     !filenames.empty())
        //     {
        //         for (const auto& filename : filenames)
        //         {
        //             try
        //             {
        //                 std::filesystem::copy_file(filename,
        //                                            std::filesystem::path(xlsxStartupPath) /
        //                                                std::filesystem::path(filename).filename(),
        //                                            std::filesystem::copy_options::overwrite_existing);
        //             }
        //             catch (const std::filesystem::filesystem_error& err)
        //             {
        //                 Overlay::Get()->Start(
        //                     Format("Не удалось скопировать файл: \n{} \nПричина: {}", filename, err.what()));
        //                 LOG_CORE_ERROR("File copy error ({}), filesystem error: {}", filename, err.what());
        //             }
        //         }
        //         isNeedRebuild = true;
        //     }
        // }

        static std::string excelFilename;
        ImGui::InputText("Имя файла", &excelFilename);
        if (ImGui::Button("Создать Excel файл"))
        {
            size_t filesCount = FileSystemUtils::FilesCountInDirectory(xlsxStartupPath);
            std::filesystem::path newFilepath =
                xlsxStartupPath /
                std::filesystem::path(std::format("{}_{}.xlsx", FileFormat::FormatId(filesCount), excelFilename));

            xlnt::workbook wb;
            wb.save(newFilepath);

            isNeedRebuild = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Добавить существующие Excel файлы"))
        {
            if (std::vector<std::string> filenames = FileDialogs::OpenMultipleFiles(kFileDialogsXlsxFilter);
                !filenames.empty())
            {
                size_t filesCount = FileSystemUtils::FilesCountInDirectory(xlsxStartupPath);
                for (const auto& filename : filenames)
                {
                    try
                    {
                        const std::filesystem::path destPath =
                            xlsxStartupPath / std::format("{}_{}", FileFormat::FormatId(filesCount++),
                                                          std::filesystem::path(filename).filename().string());
                        std::filesystem::copy_file(filename, destPath, std::filesystem::copy_options::skip_existing);
                    }
                    catch (const std::filesystem::filesystem_error& err)
                    {
                        Overlay::Get()->Start(
                            Format("Не удалось скопировать файл: \n{} \nПричина: {}", filename, err.what()));
                        LOG_CORE_ERROR("File copy error ({}), filesystem error: {}", filename, err.what());
                    }
                }
                isNeedRebuild = true;
            }
        }

        if (ImGui::Button("Открыть папку с картинками без правил"))
        {
            std::filesystem::path path = _Project->GetVariantExcelTablesHelpers().GetImgsNoConditionPath();
            if (!std::filesystem::exists(path))
            {
                Overlay::Get()->Start(Format("Папка не найдена \nВозникла неизвестная ошибка \nПуть: {}",
                                                path.make_preferred().string()));
            }
            else
            {
                std::filesystem::path command = std::filesystem::path("explorer.exe");
                // Open Explorer directly inside the folder (don't open parent and select)
                std::string arg = "\"" + path.make_preferred().string() + "\"";
                std::system((command.string() + " " + arg).c_str());
            }
        }


        static size_t filesCount = FileSystemUtils::FilesCountInDirectory(xlsxStartupPath);
        static std::vector<std::filesystem::path> paths;
        if (ImGui::Button("Обновить отображаемые файлы") || isNeedRebuild)
        {
            filesCount = FileSystemUtils::FilesCountInDirectory(xlsxStartupPath);
            paths.clear();
            for (const auto& entry : std::filesystem::directory_iterator(xlsxStartupPath))
            {
                paths.push_back(entry.path());
            }

            isNeedRebuild = false;
        }

        ImGui::Text("Файлов в каталоге: %zu", filesCount);

        static ImGuiTableFlags tableFlags = ImGuiTableFlags_SizingFixedFit;    // | ImGuiTableFlags_ScrollX
        if (ImGui::BeginTable("XLSX Table", 2, tableFlags))
        {
            ImGui::TableNextColumn();
            ImGui::Text("Файлы в папке:");
            ImGui::TableNextColumn();
            ImGui::Text("Использовать в импорте на сервер");

            const std::vector<std::string>& pageNamesToSkipOnServerImport =
                _Project->GetVariantExcelTables().GetPageNamesToSkipOnServerImport();
            for (const auto& path : paths)
            {
                ImGui::PushID(path.string().c_str());
                ImGui::TableNextColumn();
                ImGui::Text("%s", path.filename().string().c_str());
                ImGui::TableNextColumn();
                bool isInUse = std::find(pageNamesToSkipOnServerImport.begin(), pageNamesToSkipOnServerImport.end(),
                                         path.filename().string()) == pageNamesToSkipOnServerImport.end();
                if (ImGui::Checkbox("##ServerImport", &isInUse))
                {
                    _Project->GetVariantExcelTables().TogglePageNameToSkipOnServerImport(path.filename().string());
                    Project::Save(_Project);
                }

                // TODO: Add delete file button:
                // move xlsx to delete folder in project and rename file with _deleted,
                // move imgs for this xlsx (from img_raw) to delete folder in project and rename imgs with _deleted,
                // rename images for this xlsx img_raw folder (change numbering),
                // rename files in xlsxStartupPath (change numbering), rename imgs)

                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }

    void SetupProject::DrawCatalog(Ref<Project> _Project)
    {
        ImGui::Text("Оригинал каталога: %s", _Project->GetCatalogBaseFilename().c_str());
        if (ImGui::Button("Изменить оригинал каталога"))
        {
            if (std::string filename = FileDialogs::OpenFile(kFileDialogsPdfFilter); filename != std::string())
            {
                try
                {
                    std::filesystem::remove(_Project->GetCatalogFilename());
                    if (std::filesystem::copy_file(filename, _Project->GetCatalogFilename()))
                    {
                        _Project->SetCatalogBaseFilename(filename);
                    }
                }
                catch (const std::filesystem::filesystem_error& err)
                {
                    Overlay::Get()->Start(
                        Format("Не удалось изменить файл каталога: \n{} \nПричина: {}", filename, err.what()));
                    LOG_CORE_ERROR("File copy error ({}), filesystem error: {}", filename, err.what());
                }
            }
        }

        bool splitPages = _Project->GetCatalogSplitPages();
        if (ImGui::Checkbox("Разделить страницу на две картинки", &splitPages))
        {
            _Project->SetCatalogSplitPages(splitPages);
        }

        int multi = _Project->GetCatalogImgQuality();
        if (ImGui::SliderInt("Качество картинки", &multi, 1, 6))
        {
            _Project->SetCatalogImgQuality(multi);
        }

        ImGui::Spacing();

        if (ImGui::Button("Сгенерировать картинки из PDF"))
        {
            GenCatalogRawImages(_Project);
        }

        if (_Project->GetCatalogNeedRebuild() && _Project->GetCatalogBaseFilename() != std::string())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Каталог нужно перегенерировать");
        }
    }

    void SetupProject::DrawImgsByCutPattern(Ref<Project> _Project)
    {
        if (_Project->GetCatalogTopLeftPattern().IsExists())
        {
            ImGui::Text("Верхний левый паттерн");
            ImGui::Text("На странице: %d", _Project->GetCatalogTopLeftPattern().PageId);
        }

        if (_Project->GetCatalogBotRightPattern().IsExists())
        {
            ImGui::Text("Нижний правый паттерн");
            ImGui::Text("На странице: %d", _Project->GetCatalogBotRightPattern().PageId);
        }

        if (ImGui::Button("Обрезать каталог по паттернам"))
        {
            GenImgsByCutPattern(_Project);
        }

        if (_Project->GetIsCatalogGenerated() && _Project->GetImgsByCutPatternNeedRebuild())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Картинки нужно обрезать");
        }
    }

    void SetupProject::DrawGenRawExcel(Ref<Project> _Project)
    {
        bool useCutPatterntImgs = _Project->GetRawExcelUseCutPatternImgs();
        if (ImGui::Checkbox("Использовать обрезанные картинки", &useCutPatterntImgs))
        {
            _Project->SetRawExcelUseCutPatternImgs(useCutPatterntImgs);
        }

        if (ImGui::Button("Сгенерировать Excel"))
        {
            GenRawExcel(_Project);
        }

        if (_Project->GetIsImgsByCutPatternGenerated() && _Project->GetRawExcelNeedRebuild())
        {
            ImGui::SameLine();
            ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Файлы Excel нужно перегенерировать");
        }
    }

    void SetupProject::GenCatalogRawImages(Ref<Project> _Project)
    {
        if (_Project->GetCatalogBaseFilename() == std::string())
        {
            Overlay::Get()->Start("Каталог не выбран!");
            return;
        }

        FileSystemUtils::RemoveAllInFolder(_Project->GetPdfTablesWithOcrTypeRawImgPath());
        FileSystemUtils::RemoveAllInFolder(_Project->GetPdfTablesWithOcrTypeRawImgPrevPath());

        PythonCommand pythonCommand("./assets/scripts/prepare_img_raw.py");
        pythonCommand.AddArg(_Project->GetCatalogFilename());
        pythonCommand.AddArg(_Project->GetPdfTablesWithOcrTypeRawImgPath());
        pythonCommand.AddArg(_Project->GetPdfTablesWithOcrTypeRawImgPrevPath());
        pythonCommand.AddArg(_Project->GetCatalogImgQuality());
        pythonCommand.AddArg(_Project->GetCatalogSplitPages());

        ScriptPopup::Get()->AddToQueue(pythonCommand,
                                       { "Генерация картинок из PDF",
                                         []() {
                                             ImGui::Text("Работает скрипт преобразования pdf в картинки");
                                             ImGui::Text("Это может занять несколько минут");
                                             ImGui::Text("После его завершения можно закрыть это окно");
                                         },
                                         [=](int) {
                                             _Project->OnGenCatalogRawImgs();
                                             Project::Save(_Project);
                                             TextureManager::RemoveAll();
                                         } });
    }

    void SetupProject::GenImgsByCutPattern(Ref<Project> _Project)
    {
        if (!_Project->GetCatalogTopLeftPattern().IsExists())
        {
            Overlay::Get()->Start("Левый верхний паттерн отсутствует!");
            return;
        }

        if (!_Project->GetCatalogBotRightPattern().IsExists())
        {
            Overlay::Get()->Start("Правый нижний паттерн отсутствует!");
            return;
        }

        if (!_Project->GetIsCatalogGenerated())
        {
            Overlay::Get()->Start("Каталог не сгенерирован!");
            return;
        }

        FileSystemUtils::RemoveAllInFolder(_Project->GetPdfTablesWithOcrTypeCutByPatternImgsPath());
        FileSystemUtils::RemoveAllInFolder(_Project->GetPdfTablesWithOcrTypeCutByPatternImgsPrevPath());
        // TODO: Delete ImgsByCutPattern Versions ???

        PythonCommand pythonCommand("./assets/scripts/cut_by_pattern.py");
        pythonCommand.AddArg(_Project->GetPdfTablesWithOcrTypeRawImgPath());
        pythonCommand.AddArg(_Project->GetProjectFilename());
        pythonCommand.AddArg(_Project->GetTmpPath());
        pythonCommand.AddArg(_Project->GetPdfTablesWithOcrTypeCutByPatternImgsPath());
        pythonCommand.AddArg(_Project->GetPdfTablesWithOcrTypeCutByPatternImgsPrevPath());
        pythonCommand.AddArg(_Project->GetCatalogSplitPages());
        pythonCommand.AddArg("0.99");

        ScriptPopup::Get()->AddToQueue(pythonCommand, { "Обрезание картинок каталога",
                                                        []() {
                                                            ImGui::Text("Работает скрипт обрезания картинок");
                                                            ImGui::Text("Это может занять несколько минут");
                                                            ImGui::Text("После его завершения можно закрыть это окно");
                                                        },
                                                        [=](int) {
                                                            _Project->OnGenImgsByCutPattern();
                                                            Project::Save(_Project);
                                                            TextureManager::RemoveAll();
                                                        } });
    }

    void SetupProject::GenRawExcel(Ref<Project> _Project)
    {
        bool useCutPatterntImgs = _Project->GetRawExcelUseCutPatternImgs();
        if (!_Project->GetIsImgsByCutPatternGenerated() && useCutPatterntImgs)
        {
            Overlay::Get()->Start("Обрезаные картинки по паттерну не сгенерированы!");
            return;
        }

        if (!_Project->GetIsCatalogGenerated() && !useCutPatterntImgs)
        {
            Overlay::Get()->Start("Каталог не сгенерирован!");
            return;
        }

        FileSystemUtils::RemoveAllInFolder(_Project->GetPdfTablesWithOcrTypeRawExcelPath());
        // TODO: Delete RawExcel Versions ???

        PythonCommand pythonCommand("./assets/scripts/extract_tables_to_xlsx.py");
        pythonCommand.AddArg(useCutPatterntImgs ? _Project->GetPdfTablesWithOcrTypeCutByPatternImgsPath()
                                                : _Project->GetPdfTablesWithOcrTypeRawImgPath());
        pythonCommand.AddArg(_Project->GetProjectFilename());
        pythonCommand.AddArg(_Project->GetPdfTablesWithOcrTypeRawExcelPath());
        pythonCommand.AddArg(6);

        ScriptPopup::Get()->AddToQueue(pythonCommand, { "Генерация Excel из картинок",
                                                        []() {
                                                            ImGui::Text("Работает скрипт генерации Excel");
                                                            ImGui::Text("Это может занять несколько минут");
                                                            ImGui::Text("После его завершения можно закрыть это окно");
                                                        },
                                                        [=](int) {
                                                            _Project->OnGenRawExcel();
                                                            Project::Save(_Project);
                                                        } });
    }

}    // namespace LM
