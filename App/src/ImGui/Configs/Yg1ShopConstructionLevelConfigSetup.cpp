#include "Yg1ShopConstructionLevelConfigSetup.hpp"

#include "Engine/Utils/Log.hpp"
#include "Engine/Utils/json.hpp"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <fstream>
#include <string>

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
            for (size_t i = 0; i < m_Configs.size(); ++i)
            {
                Yg1ShopConstructionLevel& config = m_Configs[i];

                ImGui::PushID(static_cast<int>(i));

                if (ImGui::InputText("Конструкция", &config.Construction))
                {
                    m_HasChanges = true;
                    m_LastChangeTime = std::chrono::steady_clock::now();
                }

                if (ImGui::InputText("Название", &config.Name))
                {
                    m_HasChanges = true;
                    m_LastChangeTime = std::chrono::steady_clock::now();
                }
                if (ImGui::InputText("Уровень 1", &config.Level1))
                {
                    m_HasChanges = true;
                    m_LastChangeTime = std::chrono::steady_clock::now();
                }
                if (ImGui::InputText("Уровень 2", &config.Level2))
                {
                    m_HasChanges = true;
                    m_LastChangeTime = std::chrono::steady_clock::now();
                }
                if (ImGui::InputText("Уровень 3", &config.Level3))
                {
                    m_HasChanges = true;
                    m_LastChangeTime = std::chrono::steady_clock::now();
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

        for (const auto& config : m_Configs)
        {
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
            json[config.Construction] = item;
        }

        fout << std::setw(4) << json;
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
