#pragma once

#include <cstdint>
#include <functional>
#include <string>
#include <vector>

#include "Engine/Core/Base.h"
#include "Engine/Events/Event.h"

namespace LM
{
    struct WindowProps
    {
        std::string Title = "Engine";
        uint32_t Width = 1280u;
        uint32_t Height = 720u;
        // PNG-иконки окна/таскбара (несколько размеров). Используются на Linux;
        // на Windows иконка берётся из ресурса GLFW_ICON внутри .exe (App/res/app.rc).
        std::vector<std::string> IconPaths;
    };

    // Направление ресайза окна за край (аналог HTLEFT/HTTOP... из Win32 и
    // направлений EWMH _NET_WM_MOVERESIZE). Outside — точка вне зоны ресайза.
    // (None как имя нельзя: X11 X.h определяет макрос None.)
    enum class WindowResizeDirection
    {
        Outside,
        TopLeft,
        Top,
        TopRight,
        Right,
        BottomRight,
        Bottom,
        BottomLeft,
        Left,
    };

    class Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        virtual ~Window() = default;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual float GetMonitorScale() const = 0;
        virtual bool IsWindowMaximized() const = 0;

        // Направление ресайза окна за край под текущей позицией курсора (в
        // координатах клиентской области окна). Outside, если курсор не в зоне
        // ресайза либо ресайз за края сейчас недоступен (например, окно
        // максимизировано).
        virtual WindowResizeDirection GetResizeDirectionUnderCursor() const = 0;

        virtual void Maximize() = 0;
        virtual void Minimize() = 0;
        virtual void Restore() = 0;

        virtual void OnUpdate() = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;

        virtual void* GetNativeWindow() const = 0;

        static Ref<Window> Create(const WindowProps& _Props = WindowProps());
    };

}    // namespace LM
