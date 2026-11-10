#include <Engine/Core/EntryPoint.h>

#include "Engine/Utils/utf8.h"

#include "EditorLayer.h"

namespace LM
{

    class App : public Application
    {
    public:
        App(const ApplicationSpecification& spec) : Application(spec) { PushLayer(new EditorLayer()); }
    };

    Application* CreateApplication(ApplicationCommandLineArgs args)
    {
        std::cout << "Create Application start" << std::endl;

        ApplicationSpecification spec;
        spec.Name = "InSearch Editor";
        spec.CommandLineArgs = args;
        spec.WorkingDirectory = RES_FOLDER;
        // Кадры той же иконки, что вшита в .exe через App/res/app.ico (PNG извлечены
        // из него скриптом scripts/linux-desktop/ico-to-png.py). На Windows список
        // игнорируется — там иконку окна и .exe задаёт ресурс GLFW_ICON.
        spec.WindowIconPaths = {
            "./assets/textures/app_icon/app_icon_16.png",
            "./assets/textures/app_icon/app_icon_24.png",
            "./assets/textures/app_icon/app_icon_32.png",
            "./assets/textures/app_icon/app_icon_48.png",
            "./assets/textures/app_icon/app_icon_64.png",
            "./assets/textures/app_icon/app_icon_128.png",
            "./assets/textures/app_icon/app_icon_256.png",
        };

        return new App(spec);
    }

}    // namespace LM
