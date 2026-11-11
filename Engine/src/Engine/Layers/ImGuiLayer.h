#pragma once

#include "Engine/Layers/Layer.h"

#include "Engine/Events/KeyEvent.h"
#include "Engine/Events/MouseEvent.h"
#include "Engine/Events/WindowEvent.h"

#include <unordered_map>

struct ImFont;

namespace LM
{

    class ImGuiLayer : public Layer
    {
    public:
        ImGuiLayer();
        ~ImGuiLayer() = default;

        virtual void OnAttach() override;
        virtual void OnDetach() override;
        virtual void OnEvent(Event& e) override;

        static ImGuiLayer* Get() { return s_Instance; }

        bool PushConsolasFont();

        void Begin();
        void End();

        void BlockEvents(bool block) { m_BlockEvents = block; }

        void SetDarkThemeColors();

        uint32_t GetActiveWidgetID() const;

        void ChangeFontSize(bool _NeedUpdateFontTexture);

    private:
        void ApplyWindowEdgeMouseCursor();

    private:
        static inline ImGuiLayer* s_Instance = nullptr;

        bool m_BlockEvents = true;

        bool m_ChangeSize = false;
        int m_FontSize = 16;

        ImFont* m_ConsolasFont = nullptr;
    };

}    // namespace LM
