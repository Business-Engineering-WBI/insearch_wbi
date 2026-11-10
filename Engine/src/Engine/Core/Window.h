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

    class Window
    {
    public:
        using EventCallbackFn = std::function<void(Event&)>;

        virtual ~Window() = default;

        virtual uint32_t GetWidth() const = 0;
        virtual uint32_t GetHeight() const = 0;
        virtual float GetMonitorScale() const = 0;
        virtual bool IsWindowMaximized() const = 0;

        virtual void Maximize() = 0;
        virtual void Minimize() = 0;
        virtual void Restore() = 0;

        virtual void OnUpdate() = 0;

        virtual void SetEventCallback(const EventCallbackFn& callback) = 0;

        virtual void* GetNativeWindow() const = 0;

        static Ref<Window> Create(const WindowProps& _Props = WindowProps());
    };

}    // namespace LM
