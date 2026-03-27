#include "Yg1ShopConstructionLevelConfigSetup.hpp"

#include "Engine/Utils/Log.hpp"
#include "Engine/Utils/json.hpp"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <fstream>
#include <string>
#include <unordered_map>

namespace LM
{

    Yg1ShopConstructionLevelConfigSetup::Yg1ShopConstructionLevelConfigSetup()
    {
        LoadConfig();
        CORE_ASSERT(!s_Instance, "Yg1ShopConstructionLevelConfigSetup instance already exists!");
        s_Instance = this;
    }

    Yg1ShopConstructionLevelConfigSetup::~Yg1ShopConstructionLevelConfigSetup() { SaveConfig(); }

    void Yg1ShopConstructionLevelConfigSetup::OnImGuiRender()
    {
        // Проверяем нужно ли сохранить с debounce
        if (m_HasChanges)
        {
            auto now = std::chrono::steady_clock::now();
            if (now - m_LastChangeTime >= kSaveDebounceDelay)
            {
                SaveConfig();
                m_HasChanges = false;
            }
        }

        if (ImGui::Begin("Настройка уровней конструкций для магазина"))
        {
            // Precompute counts to detect duplicates
            std::unordered_map<std::string, int> counts;
            for (const auto& c : m_Configs)
            {
                counts[c.Construction]++;
            }

            for (size_t i = 0; i < m_Configs.size(); ++i)
            {
                Yg1ShopConstructionLevel& config = m_Configs[i];

                bool isDuplicate = (!config.Construction.empty() && counts[config.Construction] > 1) ||
                                   config.Construction.rfind("ERROR", 0) == 0;

                ImGui::PushID(static_cast<int>(i));

                if (isDuplicate)
                {
                    ImGui::PushStyleColor(ImGuiCol_FrameBg, ImVec4(1.0f, 0.6f, 0.6f, 1.0f));
                    ImGui::PushStyleColor(ImGuiCol_FrameBgHovered, ImVec4(1.0f, 0.5f, 0.5f, 1.0f));
                }

                if (ImGui::InputText("Ключ конструкция (ctd_...)", &config.Construction))
                {
                    m_HasChanges = true;
                    m_LastChangeTime = std::chrono::steady_clock::now();
                }

                if (isDuplicate)
                {
                    ImGui::SameLine();
                    ImGui::TextColored(ImVec4(1.0f, 0.0f, 0.0f, 1.0f), "Дубликат ключа");
                }

                if (ImGui::InputText("Наименование перед обозначением (Пример: Пластина токарная)", &config.Name))
                {
                    m_HasChanges = true;
                    m_LastChangeTime = std::chrono::steady_clock::now();
                }
                if (ImGui::InputText("Раздел каталога уровень 1", &config.Level1))
                {
                    m_HasChanges = true;
                    m_LastChangeTime = std::chrono::steady_clock::now();
                }
                if (ImGui::InputText("Раздел каталога уровень 2", &config.Level2))
                {
                    m_HasChanges = true;
                    m_LastChangeTime = std::chrono::steady_clock::now();
                }
                if (ImGui::InputText("Раздел каталога уровень 3", &config.Level3))
                {
                    m_HasChanges = true;
                    m_LastChangeTime = std::chrono::steady_clock::now();
                }

                if (isDuplicate)
                {
                    ImGui::PopStyleColor(2);
                }

                ImGui::PopID();

                ImGui::Spacing();
                ImGui::Separator();
                ImGui::Spacing();
            }
            if (ImGui::Button("Добавить новую конструкцию"))
            {
                m_Configs.emplace_back();
                m_HasChanges = true;
                m_LastChangeTime = std::chrono::steady_clock::now();
            }
        }
        ImGui::End();
    }

    void Yg1ShopConstructionLevelConfigSetup::SaveConfig()
    {
        std::filesystem::path configPath(kYg1ShopConstructionLevelConfigSetupPath);
        std::filesystem::create_directories(configPath.parent_path());
        std::ofstream fout(kYg1ShopConstructionLevelConfigSetupPath);
        if (!fout.is_open())
        {
            LOG_CORE_WARN("Failed to save Yg1ShopConstructionLevel configuration file");
            return;
        }
        nlohmann::json json;

        // We want to preserve duplicate constructions on save. JSON objects cannot hold
        // duplicate keys, so for duplicates we save them with a prefix: ERROR<copyIndex>_<originalKey>
        std::unordered_map<std::string, int> seen;
        for (const auto& config : m_Configs)
        {
            if (config.Construction.empty())
            {
                continue;    // skip empty keys
            }

            nlohmann::json item;
            item["dop"] = config.Name;
            item["l1"] = config.Level1;
            if (!config.Level2.empty())
            {
                item["l2"] = config.Level2;
            }
            if (!config.Level3.empty())
            {
                item["l3"] = config.Level3;
            }

            // increment occurrence count
            int idx = ++seen[config.Construction];
            if (idx == 1)
            {
                // first occurrence, use original key
                json[config.Construction] = item;
            }
            else
            {
                // duplicate occurrence: save with ERROR<copyNumber>_ prefix
                int copyNumber = idx - 1;    // first duplicate -> ERROR1_
                std::string errorKey = "ERROR" + std::to_string(copyNumber) + "_" + config.Construction;
                json[errorKey] = item;
            }
        }

        fout << std::setw(4) << json;

        LOG_CORE_INFO("Yg1ShopConstructionLevel configuration saved successfully");
    }

    void Yg1ShopConstructionLevelConfigSetup::SaveConfigIfHasChanges()
    {
        if (m_HasChanges)
        {
            SaveConfig();
            m_HasChanges = false;
        }
    }

    void Yg1ShopConstructionLevelConfigSetup::LoadConfig()
    {
        std::ifstream infile(kYg1ShopConstructionLevelConfigSetupPath);
        if (!infile.is_open())
        {
            LOG_CORE_INFO("Config file not found, starting with empty configuration.");
            return;
        }

        nlohmann::json json;
        infile >> json;

        for (auto& [construction, data] : json.items())
        {
            Yg1ShopConstructionLevel config;
            config.Construction = construction;
            config.Name = data.value("dop", "");
            config.Level1 = data.value("l1", "");
            config.Level2 = data.value("l2", "");
            config.Level3 = data.value("l3", "");

            m_Configs.push_back(config);
        }
    }

}    // namespace LM
