#pragma once

#include "Engine/Core/Assert.h"

#include <chrono>
#include <filesystem>

namespace LM
{

    struct Yg1ShopConstructionLevel
    {
        std::string Construction;
        std::string Name;
        std::string Level1;
        std::string Level2;
        std::string Level3;
    };

    class Yg1ShopConstructionLevelConfigSetup
    {
    private:
        static inline const std::filesystem::path kYg1ShopConstructionLevelConfigSetupPath =
            "./assets/configs/yg1-shop_construction_level.json";

    public:
        Yg1ShopConstructionLevelConfigSetup();
        ~Yg1ShopConstructionLevelConfigSetup();

        void OnImGuiRender();

        void SaveConfig();
        void SaveConfigIfHasChanges();

        static Yg1ShopConstructionLevelConfigSetup& Get()
        {
            CORE_ASSERT(s_Instance, "Yg1ShopConstructionLevelConfigSetup instance is not initialized!");
            return *s_Instance;
        }

    private:
        void LoadConfig();

    private:
        static inline Yg1ShopConstructionLevelConfigSetup* s_Instance = nullptr;

        std::vector<Yg1ShopConstructionLevel> m_Configs;

        std::chrono::steady_clock::time_point m_LastChangeTime;
        bool m_HasChanges = false;
        static constexpr auto kSaveDebounceDelay = std::chrono::milliseconds(1000);
    };

}    // namespace LM
