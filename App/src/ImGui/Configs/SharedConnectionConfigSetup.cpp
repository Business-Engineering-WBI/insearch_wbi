#include "SharedConnectionConfigSetup.hpp"

#include "Engine/Utils/Log.hpp"
#include "Engine/Utils/json.hpp"

#include "imgui.h"
#include "misc/cpp/imgui_stdlib.h"

#include <fstream>
#include <string>

namespace LM
{

    SharedConnectionConfigSetup::SharedConnectionConfigSetup()
    {
        LoadConfig();
        CORE_ASSERT(!s_Instance, "SharedConnectionConfigSetup instance already exists!");
        s_Instance = this;
    }

    SharedConnectionConfigSetup::~SharedConnectionConfigSetup() { SaveConfig(); }

    void SharedConnectionConfigSetup::OnImGuiRender()
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

        if (ImGui::Begin("Настройка соединений"))
        {
            if (ImGui::BeginCombo("Конфигурация", m_CurrentConfigName.c_str()))
            {
                for (const auto& config : m_Configs)
                {
                    bool isSelected = (config.Name == m_CurrentConfigName);
                    if (ImGui::Selectable(config.Name.c_str(), isSelected))
                    {
                        m_CurrentConfigName = config.Name;
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (isSelected)
                    {
                        ImGui::SetItemDefaultFocus();
                    }
                }
                ImGui::EndCombo();
            }
            if (ImGui::Button("Добавить новую конфигурацию"))
            {
                SharedConnectionConfig newConfig;
                newConfig.Name = "Новая конфигурация";
                m_Configs.push_back(newConfig);
                m_CurrentConfigName = newConfig.Name;
                m_HasChanges = true;
                m_LastChangeTime = std::chrono::steady_clock::now();
            }

            for (auto& config : m_Configs)
            {
                if (config.Name == m_CurrentConfigName)
                {
                    ImGui::Separator();
                    ImGui::Text("Текущая конфигурация будет использована в скриптах");

                    if (ImGui::InputText("Имя конфигурации", &config.Name))
                    {
                        m_HasChanges = true;
                        m_CurrentConfigName = config.Name;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    ImGui::Separator();

                    ImGui::Text("Оставьте 'SSH Хост' пустым, если не требуется SSH туннелирование.");

                    if (ImGui::InputText("SSH Хост", &config.SshHost))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (ImGui::InputInt("SSH Порт", &config.SshPort))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (ImGui::InputText("SSH Пользователь", &config.SshUser))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (ImGui::InputText("SSH Пароль", &config.SshPassword))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }

                    ImGui::Separator();

                    if (ImGui::InputText("DB Хост", &config.DbHost))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (ImGui::InputInt("DB Порт", &config.DbPort))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (ImGui::InputText("DB Пользователь", &config.DbUser))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (ImGui::InputText("DB Пароль", &config.DbPassword))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }

                    ImGui::Separator();

                    if (ImGui::InputText("Путь к изображениям на сервере", &config.ServerImgsPath))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }

                    ImGui::Separator();
                    ImGui::Text("Настройки FTP магазина");

                    if (ImGui::InputText("FTP Хост", &config.ShopFtpHost))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (ImGui::InputInt("FTP Порт", &config.ShopFtpPort))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (ImGui::InputText("FTP Пользователь", &config.ShopFtpUser))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (ImGui::InputText("FTP Пароль", &config.ShopFtpPassword))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                    if (ImGui::InputText("FTP Путь к изображениям", &config.ShopFtpImgsPath))
                    {
                        m_HasChanges = true;
                        m_LastChangeTime = std::chrono::steady_clock::now();
                    }
                }
            }
        }
        ImGui::End();
    }

    void SharedConnectionConfigSetup::SaveConfig()
    {
        std::filesystem::path configPath(kSharedConnectionConfigSetupPath);
        std::filesystem::create_directories(configPath.parent_path());
        std::ofstream fout(kSharedConnectionConfigSetupPath);
        if (!fout.is_open())
        {
            LOG_CORE_WARN("Failed to save shared connection configuration file");
            return;
        }
        nlohmann::json json;

        nlohmann::json jsonConfigs = nlohmann::json::array();
        for (const auto& config : m_Configs)
        {
            nlohmann::json item;
            item["name"] = config.Name;

            item["ssh_host"] = config.SshHost;
            item["ssh_port"] = config.SshPort;
            item["ssh_user"] = config.SshUser;
            item["ssh_password"] = config.SshPassword;

            item["db_host"] = config.DbHost;
            item["db_port"] = config.DbPort;
            item["db_user"] = config.DbUser;
            item["db_password"] = config.DbPassword;

            item["server_imgs_path"] = config.ServerImgsPath;

            item["shop_ftp_host"] = config.ShopFtpHost;
            item["shop_ftp_port"] = config.ShopFtpPort;
            item["shop_ftp_user"] = config.ShopFtpUser;
            item["shop_ftp_password"] = config.ShopFtpPassword;
            item["shop_ftp_imgs_path"] = config.ShopFtpImgsPath;

            jsonConfigs.push_back(item);
        }

        json["configs"] = jsonConfigs;
        json["current_config_name"] = m_CurrentConfigName;

        fout << std::setw(4) << json;
    }

    void SharedConnectionConfigSetup::LoadConfig()
    {
        std::ifstream infile(kSharedConnectionConfigSetupPath);
        if (!infile.is_open())
        {
            LOG_CORE_INFO("Config file not found, starting with empty configuration.");
            return;
        }

        nlohmann::json json;
        infile >> json;

        for (const auto& item : json["configs"])
        {
            SharedConnectionConfig config;
            config.Name = item.value("name", "");

            config.SshHost = item.value("ssh_host", "");
            config.SshPort = item.value("ssh_port", 22);
            config.SshUser = item.value("ssh_user", "");
            config.SshPassword = item.value("ssh_password", "");

            config.DbHost = item.value("db_host", "");
            config.DbPort = item.value("db_port", 5432);
            config.DbUser = item.value("db_user", "");
            config.DbPassword = item.value("db_password", "");

            config.ServerImgsPath = item.value("server_imgs_path", "");

            config.ShopFtpHost = item.value("shop_ftp_host", "");
            config.ShopFtpPort = item.value("shop_ftp_port", 21);
            config.ShopFtpUser = item.value("shop_ftp_user", "");
            config.ShopFtpPassword = item.value("shop_ftp_password", "");
            config.ShopFtpImgsPath = item.value("shop_ftp_imgs_path", "");

            m_Configs.push_back(config);
        }

        m_CurrentConfigName = json.value("current_config_name", m_Configs.empty() ? "" : m_Configs[0].Name);
    }

}    // namespace LM
