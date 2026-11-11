#include "GLFWWindow.h"

#include "Engine/Core/Application.h"
#include "Engine/Events/KeyEvent.h"
#include "Engine/Events/MouseEvent.h"
#include "Engine/Events/WindowEvent.h"
#include "Engine/Utils/Log.hpp"
#include "GLFW/glfw3.h"
#include <cmath>
#include <string>
#include <vector>

#ifdef _WIN32
    #define GLFW_EXPOSE_NATIVE_WIN32
    #include "GLFW/glfw3native.h"

    #define UNICODE
    #define _UNICODE
    #include <Windows.h>
    #include <Windowsx.h>
    // #include <dwmapi.h>
    // #pragma comment(lib, "dwmapi.lib")
#else
    #define GLFW_EXPOSE_NATIVE_X11
    #include "GLFW/glfw3native.h"
    #include <X11/Xlib.h>
    #include <X11/Xutil.h>    // XClassHint
    #include <stb_image.h>
#endif

namespace LM
{

#ifdef _WIN32
    WNDPROC original_proc;
#endif
    float g_MonitorScale = 1.0f;

    // Аналог зон ресайза из WM_NCHITTEST (HTLEFT/HTTOP/...): возвращает направление
    // ресайза для точки курсора (координаты — от левого верхнего угла клиентской
    // области окна) либо Outside. В максимизированном состоянии ресайз за края
    // запрещён, поэтому всегда Outside.
    WindowResizeDirection hitTestResizeDirection(GLFWwindow* _Window, double _CursorX, double _CursorY)
    {
        if (_Window == nullptr || glfwGetWindowAttrib(_Window, GLFW_MAXIMIZED) == GLFW_TRUE)
        {
            return WindowResizeDirection::Outside;
        }

        int windowWidth = 0;
        int windowHeight = 0;
        glfwGetWindowSize(_Window, &windowWidth, &windowHeight);

        const int border = static_cast<int>(std::lround(8.0f * g_MonitorScale));

        const bool bottom = (_CursorY >= windowHeight - border);
        const bool top = (_CursorY <= border);
        const bool left = (_CursorX <= border);
        const bool right = (_CursorX >= windowWidth - border);

        if (bottom)
        {
            if (left) { return WindowResizeDirection::BottomLeft; }
            if (right) { return WindowResizeDirection::BottomRight; }
            return WindowResizeDirection::Bottom;
        }
        if (top)
        {
            if (left) { return WindowResizeDirection::TopLeft; }
            if (right) { return WindowResizeDirection::TopRight; }
            return WindowResizeDirection::Top;
        }
        if (left) { return WindowResizeDirection::Left; }
        if (right) { return WindowResizeDirection::Right; }

        return WindowResizeDirection::Outside;
    }

#ifdef _WIN32
    bool IsWindowMaximizedAlt(HWND hWnd)
    {
        WINDOWPLACEMENT wp;
        wp.length = sizeof(WINDOWPLACEMENT);
        if (GetWindowPlacement(hWnd, &wp))
        {
            return wp.showCmd == SW_MAXIMIZE;
        }
        return false;
    }

    glm::ivec4 calcOffset(HWND hWnd)
    {
        if (IsZoomed(hWnd) != 0)
        {
            LONG offset = std::lround(8.0f * g_MonitorScale);
            return { offset, offset, offset, offset };
        }
        else
        {
            LONG offset = std::lround(1.0f * g_MonitorScale);
            return { 1, offset, offset, offset };
        }
    }

    void DrawBorder(HWND hWnd)
    {
        HDC hdc = GetWindowDC(hWnd);
        RECT rect;
        GetWindowRect(hWnd, &rect);
        OffsetRect(&rect, -rect.left, -rect.top);

        HPEN hPen = CreatePen(PS_SOLID, std::lround(2.0f * g_MonitorScale), RGB(64, 64, 64));
        HPEN oldPen = (HPEN)SelectObject(hdc, hPen);
        HBRUSH hBrush = (HBRUSH)GetStockObject(NULL_BRUSH);
        HBRUSH oldBrush = (HBRUSH)SelectObject(hdc, hBrush);

        Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);

        SelectObject(hdc, oldPen);
        SelectObject(hdc, oldBrush);
        DeleteObject(hPen);

        ReleaseDC(hWnd, hdc);
    }

    LRESULT CALLBACK WindowProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
    {
        // if (uMsg != 124 && uMsg != 125)
        // {
        //     LOG_CORE_WARN("WindowProc {}", uMsg);
        // }

        switch (uMsg)
        {
            case WM_NCCALCSIZE: {
                if (wParam == TRUE && lParam != NULL)
                {
                    NCCALCSIZE_PARAMS* pParams = reinterpret_cast<NCCALCSIZE_PARAMS*>(lParam);
                    glm::ivec4 offset = calcOffset(hWnd);
                    pParams->rgrc[0].top += offset[0];
                    pParams->rgrc[0].right -= offset[1];
                    pParams->rgrc[0].bottom -= offset[2];
                    pParams->rgrc[0].left += offset[3];
                }
                return 0;
            }
            case WM_NCACTIVATE: return 1;
            // case WM_ACTIVATE: {
            //     LRESULT res = DefWindowProc(hWnd, uMsg, wParam, lParam);
            //     // LONG res = CallWindowProc(original_proc, hWnd, uMsg, wParam, lParam);
            //     DrawBorder(hWnd);
            //     return res;
            // }
            case WM_NCPAINT: {
                LOG_CORE_WARN("NC PT");
                DrawBorder(hWnd);
                return 0;
            }
            case WM_PAINT: {

                PAINTSTRUCT ps;
                BeginPaint(hWnd, &ps);
                EndPaint(hWnd, nullptr);

                DrawBorder(hWnd);
                return 0;
            }
            case WM_NCHITTEST: {
                const int borderWidth = 8;

                POINT mousePos = { GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam) };

                RECT windowRect;
                GetWindowRect(hWnd, &windowRect);

                // В максимизированном состоянии ресайз за края запрещён (как и в
                // X11-ветке), остаются только HTCAPTION ниже и клиентская область.
                if (IsZoomed(hWnd) == 0)
                {
                    if (mousePos.y >= windowRect.bottom - borderWidth)
                    {
                        if (mousePos.x <= windowRect.left + borderWidth)
                        {
                            return HTBOTTOMLEFT;
                        }
                        else if (mousePos.x >= windowRect.right - borderWidth)
                        {
                            return HTBOTTOMRIGHT;
                        }
                        else
                        {
                            return HTBOTTOM;
                        }
                    }
                    else if (mousePos.y <= windowRect.top + borderWidth)
                    {
                        if (mousePos.x <= windowRect.left + borderWidth)
                        {
                            return HTTOPLEFT;
                        }
                        else if (mousePos.x >= windowRect.right - borderWidth)
                        {
                            return HTTOPRIGHT;
                        }
                        else
                        {
                            return HTTOP;
                        }
                    }
                    else if (mousePos.x <= windowRect.left + borderWidth)
                    {
                        return HTLEFT;
                    }
                    else if (mousePos.x >= windowRect.right - borderWidth)
                    {
                        return HTRIGHT;
                    }
                }

                ScreenToClient(hWnd, &mousePos);
                if (mousePos.y < Application::Get().GetMainMenuFrameHeight() &&
                    !Application::Get().GetIsMainMenuAnyItemHovered())
                {
                    return HTCAPTION;
                }

                return HTCLIENT;
            }
        }

        // return DefWindowProc(hWnd, uMsg, wParam, lParam);
        return CallWindowProc(original_proc, hWnd, uMsg, wParam, lParam);
    }
    void disableTitlebar(GLFWwindow* window)
    {
        HWND hWnd = glfwGetWin32Window(window);

        LONG_PTR lStyle = GetWindowLongPtr(hWnd, GWL_STYLE);
        lStyle |= WS_THICKFRAME;
        // lStyle &= ~WS_CAPTION;
        // lStyle &= ~WS_BORDER;
        // lStyle &= ~WS_DLGFRAME;
        SetWindowLongPtr(hWnd, GWL_STYLE, lStyle);
        // MARGINS margins = {0, 0, 0, 0};
        // DwmExtendFrameIntoClientArea(hWnd, &margins);

        RECT windowRect;
        GetWindowRect(hWnd, &windowRect);
        int width = windowRect.right - windowRect.left;
        int height = windowRect.bottom - windowRect.top;

        original_proc = (WNDPROC)GetWindowLongPtr(hWnd, GWLP_WNDPROC);
        original_proc = (WNDPROC)SetWindowLongPtr(hWnd, GWLP_WNDPROC, reinterpret_cast<LONG_PTR>(WindowProc));
        SetWindowPos(hWnd, NULL, 0, 0, width, height, SWP_FRAMECHANGED | SWP_NOMOVE);
    }
#else
    namespace
    {
        Display* g_X11Display = nullptr;
        ::Window g_X11Window = None;    // ::Window — X11-тип; LM::Window здесь не подходит
        ::Window g_X11Root = None;
        Atom g_NetWmMoveResize = None;
        bool g_X11NativeUsable = false;
        bool g_SyntheticReleasePending = false;

        // Константы направлений из спецификации EWMH _NET_WM_MOVERESIZE.
        constexpr int kSizeTopLeft = 0;
        constexpr int kSizeTop = 1;
        constexpr int kSizeTopRight = 2;
        constexpr int kSizeRight = 3;
        constexpr int kSizeBottomRight = 4;
        constexpr int kSizeBottom = 5;
        constexpr int kSizeBottomLeft = 6;
        constexpr int kSizeLeft = 7;
        constexpr int kMove = 8;
    }    // namespace

    // Аналог disableTitlebar(): кеширует нативные дескрипторы и рисует границу окна.
    // WM может сбросить ширину границы при reparent, поэтому это best-effort —
    // никакая другая логика от неё не зависит (в максимизации граница не видна и на Windows).
    void setupX11Window(GLFWwindow* window)
    {
        if (glfwGetPlatform() != GLFW_PLATFORM_X11)
        {
            LOG_CORE_WARN("X11 platform is not active, custom border and window dragging are disabled");
            return;
        }

        g_X11Display = glfwGetX11Display();
        g_X11Window = glfwGetX11Window(window);
        g_X11Root = DefaultRootWindow(g_X11Display);
        g_NetWmMoveResize = XInternAtom(g_X11Display, "_NET_WM_MOVERESIZE", False);
        g_X11NativeUsable = (g_X11Display != nullptr) && (g_X11Window != None) && (g_NetWmMoveResize != None);

        if (!g_X11NativeUsable)
        {
            return;
        }

        // Аналог DrawBorder(): рамка 2px RGB(64, 64, 64).
        const unsigned int borderWidth = static_cast<unsigned int>(std::lround(2.0f * g_MonitorScale));
        XSetWindowBorderWidth(g_X11Display, g_X11Window, borderWidth);
        XSetWindowBorder(g_X11Display, g_X11Window, 0x00404040UL);
        XFlush(g_X11Display);
    }

    // Аналог ресурса GLFW_ICON из App/res/app.rc: на Linux иконку нельзя вшить
    // в бинарник, поэтому грузим PNG-кадры и ставим их как _NET_WM_ICON (иконка
    // окна, таскбар, Alt-Tab), а также перезаписываем WM_CLASS (GLFW пишет туда
    // заголовок окна) — по нему GNOME связывает запущенное окно с .desktop-
    // лаунчером (StartupWMClass), см. scripts/linux-desktop/install.sh.
    void setupX11WindowIcon(GLFWwindow* window, const std::vector<std::string>& _IconPaths)
    {
        if (glfwGetPlatform() != GLFW_PLATFORM_X11)
        {
            return;    // о не-X11 платформе уже предупредил setupX11Window
        }

        Display* display = glfwGetX11Display();
        if (display == nullptr)
        {
            return;
        }

        // res_class обязан совпадать со StartupWMClass в insearch-creator.desktop.
        char instanceName[] = "insearch-creator";
        char className[] = "InSearchCreator";
        XClassHint classHint = {};
        classHint.res_name = instanceName;
        classHint.res_class = className;
        XSetClassHint(display, glfwGetX11Window(window), &classHint);

        std::vector<GLFWimage> images;
        std::vector<unsigned char*> pixelsBuffers;
        images.reserve(_IconPaths.size());
        for (const std::string& path : _IconPaths)
        {
            int width = 0;
            int height = 0;
            unsigned char* pixels = stbi_load(path.c_str(), &width, &height, nullptr, STBI_rgb_alpha);
            if (pixels == nullptr)
            {
                LOG_CORE_WARN("Can't load window icon: {}", path);
                continue;
            }
            pixelsBuffers.push_back(pixels);
            images.push_back(GLFWimage { width, height, pixels });
        }

        if (!images.empty())
        {
            glfwSetWindowIcon(window, static_cast<int>(images.size()), images.data());
        }

        // glfwSetWindowIcon копирует данные, буферы можно освободить.
        for (unsigned char* pixels : pixelsBuffers)
        {
            stbi_image_free(pixels);
        }
        XFlush(display);
    }

    // Аналог WM_NCHITTEST: возвращает направление move/resize (EWMH) для позиции
    // курсора в окне либо -1, если точка не в зоне взаимодействия.
    int x11HitTestMoveResize(GLFWwindow* _Window, double _CursorX, double _CursorY)
    {
        switch (hitTestResizeDirection(_Window, _CursorX, _CursorY))
        {
            case WindowResizeDirection::TopLeft: return kSizeTopLeft;
            case WindowResizeDirection::Top: return kSizeTop;
            case WindowResizeDirection::TopRight: return kSizeTopRight;
            case WindowResizeDirection::Right: return kSizeRight;
            case WindowResizeDirection::BottomRight: return kSizeBottomRight;
            case WindowResizeDirection::Bottom: return kSizeBottom;
            case WindowResizeDirection::BottomLeft: return kSizeBottomLeft;
            case WindowResizeDirection::Left: return kSizeLeft;
            case WindowResizeDirection::Outside: break;
        }

        // Аналог HTCAPTION: перетаскивание окна за пустое место главной меню-полосы.
        // В максимизированном состоянии работает штатно: WM снимет максимизацию
        // при начале перемещения.
        if (_CursorY < Application::Get().GetMainMenuFrameHeight() &&
            !Application::Get().GetIsMainMenuAnyItemHovered())
        {
            return kMove;
        }

        return -1;
    }

    bool sendX11MoveResize(int _Direction)
    {
        if (!g_X11NativeUsable)
        {
            return false;
        }

        ::Window rootReturn = None;
        ::Window childReturn = None;
        int rootX = 0;
        int rootY = 0;
        int winX = 0;
        int winY = 0;
        unsigned int buttonsMask = 0;
        XQueryPointer(g_X11Display, g_X11Window, &rootReturn, &childReturn, &rootX, &rootY, &winX, &winY,
                      &buttonsMask);

        // Неявный grab указателя от ButtonPress обязательно снимается до запроса,
        // иначе WM не начнёт интерактивный move/resize (та же техника, что в SDL).
        XUngrabPointer(g_X11Display, CurrentTime);

        XEvent event = {};
        event.xclient.type = ClientMessage;
        event.xclient.serial = 0;
        event.xclient.send_event = True;
        event.xclient.window = g_X11Window;
        event.xclient.message_type = g_NetWmMoveResize;
        event.xclient.format = 32;
        event.xclient.data.l[0] = rootX;
        event.xclient.data.l[1] = rootY;
        event.xclient.data.l[2] = _Direction;
        event.xclient.data.l[3] = Button1;
        event.xclient.data.l[4] = 1;    // source indicator: обычное приложение

        XSendEvent(g_X11Display, g_X11Root, False, SubstructureRedirectMask | SubstructureNotifyMask, &event);
        XFlush(g_X11Display);
        return true;
    }

    void tryStartX11MoveResize(GLFWwindow* window)
    {
        if (!g_X11NativeUsable)
        {
            return;
        }

        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(window, &cursorX, &cursorY);

        const int direction = x11HitTestMoveResize(window, cursorX, cursorY);
        if (direction < 0)
        {
            return;
        }

        // Resize за край в максимизированном состоянии запрещён:
        // hitTestResizeDirection возвращает Outside для краёв, так что при
        // maximized сюда доходит только kMove за меню-полосу.
        if (sendX11MoveResize(direction))
        {
            // WM перехватил grab; физический ButtonRelease до GLFW не дойдёт.
            // Планируем синтетический release, чтобы ImGui не «залип» с зажатой ЛКМ.
            g_SyntheticReleasePending = true;
        }
    }

    // Проигрывает LEFT RELEASE через всю установленную цепочку колбэков
    // (imgui_impl_glfw -> лямбда Engine), чтобы обе системы ввода узнали об
    // отпускании кнопки после WM-операции. Вызывается из OnUpdate() после
    // glfwPollEvents(), а не из press-колбэка: imgui_impl_glfw вызывает
    // prev-колбэк ДО своего io.AddMouseButtonEvent, поэтому release, посланный
    // изнутри press, был бы перекрыт настоящим press-событием.
    void flushSyntheticMouseRelease(GLFWwindow* window)
    {
        GLFWmousebuttonfun callback = glfwSetMouseButtonCallback(window, nullptr);
        glfwSetMouseButtonCallback(window, callback);
        if (callback != nullptr)
        {
            callback(window, GLFW_MOUSE_BUTTON_LEFT, GLFW_RELEASE, 0);
        }
    }
#endif

    bool glfw_get_window_monitor(GLFWmonitor** monitor, GLFWwindow* window)
    {
        bool success = false;

        int window_rectangle[4] = { 0 };
        glfwGetWindowPos(window, &window_rectangle[0], &window_rectangle[1]);
        glfwGetWindowSize(window, &window_rectangle[2], &window_rectangle[3]);

        int monitors_size = 0;
        GLFWmonitor** monitors = glfwGetMonitors(&monitors_size);

        GLFWmonitor* closest_monitor = NULL;
        int max_overlap_area = 0;

        for (int i = 0; i < monitors_size; ++i)
        {
            int monitor_position[2] = { 0 };
            glfwGetMonitorPos(monitors[i], &monitor_position[0], &monitor_position[1]);

            const GLFWvidmode* monitor_video_mode = glfwGetVideoMode(monitors[i]);
            if (monitor_video_mode == nullptr)
            {
                continue;
            }

            int monitor_rectangle[4] = {
                monitor_position[0],
                monitor_position[1],
                monitor_video_mode->width,
                monitor_video_mode->height,
            };

            if (!(((window_rectangle[0] + window_rectangle[2]) < monitor_rectangle[0]) ||
                  (window_rectangle[0] > (monitor_rectangle[0] + monitor_rectangle[2])) ||
                  ((window_rectangle[1] + window_rectangle[3]) < monitor_rectangle[1]) ||
                  (window_rectangle[1] > (monitor_rectangle[1] + monitor_rectangle[3]))))
            {
                int intersection_rectangle[4] = { 0 };

                // x, width
                if (window_rectangle[0] < monitor_rectangle[0])
                {
                    intersection_rectangle[0] = monitor_rectangle[0];

                    if ((window_rectangle[0] + window_rectangle[2]) < (monitor_rectangle[0] + monitor_rectangle[2]))
                    {
                        intersection_rectangle[2] =
                            (window_rectangle[0] + window_rectangle[2]) - intersection_rectangle[0];
                    }
                    else
                    {
                        intersection_rectangle[2] = monitor_rectangle[2];
                    }
                }
                else
                {
                    intersection_rectangle[0] = window_rectangle[0];

                    if ((monitor_rectangle[0] + monitor_rectangle[2]) < (window_rectangle[0] + window_rectangle[2]))
                    {
                        intersection_rectangle[2] =
                            (monitor_rectangle[0] + monitor_rectangle[2]) - intersection_rectangle[0];
                    }
                    else
                    {
                        intersection_rectangle[2] = window_rectangle[2];
                    }
                }

                // y, height
                if (window_rectangle[1] < monitor_rectangle[1])
                {
                    intersection_rectangle[1] = monitor_rectangle[1];

                    if ((window_rectangle[1] + window_rectangle[3]) < (monitor_rectangle[1] + monitor_rectangle[3]))
                    {
                        intersection_rectangle[3] =
                            (window_rectangle[1] + window_rectangle[3]) - intersection_rectangle[1];
                    }
                    else
                    {
                        intersection_rectangle[3] = monitor_rectangle[3];
                    }
                }
                else
                {
                    intersection_rectangle[1] = window_rectangle[1];

                    if ((monitor_rectangle[1] + monitor_rectangle[3]) < (window_rectangle[1] + window_rectangle[3]))
                    {
                        intersection_rectangle[3] =
                            (monitor_rectangle[1] + monitor_rectangle[3]) - intersection_rectangle[1];
                    }
                    else
                    {
                        intersection_rectangle[3] = window_rectangle[3];
                    }
                }

                // int overlap_area = intersection_rectangle[3] * intersection_rectangle[4];
                int overlap_area = intersection_rectangle[2] * intersection_rectangle[3];

                if (overlap_area > max_overlap_area)
                {
                    closest_monitor = monitors[i];
                    max_overlap_area = overlap_area;
                }
            }
        }

        if (closest_monitor)
        {
            *monitor = closest_monitor;
            success = true;
        }

        // true: monitor contains the monitor the window is most on
        // false: monitor is unmodified
        return success;
    }

    GLFWWindow::GLFWWindow(const WindowProps& _Props) : m_Data(_Props) { Init(); }

    GLFWWindow::~GLFWWindow()
    {
        glfwDestroyWindow(m_Window);
        // glfwTerminate();

        // delete m_Window;
    }

    bool GLFWWindow::IsWindowMaximized() const
    {
        int maximized = glfwGetWindowAttrib(m_Window, GLFW_MAXIMIZED);
        return maximized == GLFW_TRUE;
    }

    WindowResizeDirection GLFWWindow::GetResizeDirectionUnderCursor() const
    {
        double cursorX = 0.0;
        double cursorY = 0.0;
        glfwGetCursorPos(m_Window, &cursorX, &cursorY);
        return hitTestResizeDirection(m_Window, cursorX, cursorY);
    }

    void GLFWWindow::Maximize() { glfwMaximizeWindow(m_Window); }

    void GLFWWindow::Minimize() { glfwIconifyWindow(m_Window); }

    void GLFWWindow::Restore() { glfwRestoreWindow(m_Window); }

    void GLFWWindow::OnUpdate()
    {
        glfwMakeContextCurrent(m_Window);
        glfwPollEvents();
#ifndef _WIN32
        if (g_SyntheticReleasePending)
        {
            g_SyntheticReleasePending = false;
            flushSyntheticMouseRelease(m_Window);
        }
#endif
        glfwSwapBuffers(m_Window);
    }

    bool GLFWWindow::Init()
    {
#ifndef _WIN32
        // Форсим X11-бэкенд: под Wayland-сессией (дефолт GNOME) это работает
        // через XWayland, сохраняя доступным нативное управление окном.
        glfwInitHint(GLFW_PLATFORM, GLFW_PLATFORM_X11);
#endif
        if (!glfwInit())
        {
            LOG_CORE_ERROR("Failed to initialize GLFW!");
            return false;
        }
        glfwWindowHint(GLFW_OPENGL_DEBUG_CONTEXT, GLFW_TRUE);
#ifndef _WIN32
        // Окно без рамки, но под управлением WM (GLFW ставит _MOTIF_WM_HINTS
        // decorations=0). Аналог Win32-ветки, где WM_NCCALCSIZE съедает
        // неклиентскую часть рамки.
        glfwWindowHint(GLFW_DECORATED, GLFW_FALSE);
#endif
        m_Window = glfwCreateWindow(m_Data.Width, m_Data.Height, m_Data.Title.c_str(), NULL, NULL);

        if (!m_Window)
        {
            LOG_CORE_ERROR("Failed to create window!");
            return false;
        }

        GLFWmonitor* nowMonitor = NULL;
        if (!glfw_get_window_monitor(&nowMonitor, m_Window))
        {
            nowMonitor = glfwGetPrimaryMonitor();
        }
        glfwGetMonitorContentScale(nowMonitor, NULL, &m_Data.MonitorScale);
        g_MonitorScale = m_Data.MonitorScale;
#ifdef _WIN32
        disableTitlebar(m_Window);
#else
        setupX11Window(m_Window);
        setupX11WindowIcon(m_Window, m_Data.IconPaths);
#endif
        glfwMaximizeWindow(m_Window);

        LOG_CORE_INFO("Current monitor: {} scale: {}", static_cast<void*>(nowMonitor), m_Data.MonitorScale);

        glfwMakeContextCurrent(m_Window);
        glfwSetWindowUserPointer(m_Window, &m_Data);

        SetCallbacks();
        // glfwSetErrorCallback(glfw_error_callback);
        // glfwSetFramebufferSizeCallback(m_Window, window_resize);
        // glfwSetWindowSizeCallback(m_Window, window_resize);
        // glfwSetKeyCallback(m_Window, key_callback);
        // glfwSetMouseButtonCallback(m_Window, mouse_button_callback);
        // glfwSetCursorPosCallback(m_Window, cursor_position_callback);
        // glfwSetScrollCallback(m_Window, scroll_callback);
        // glfwSetCharCallback(m_Window, character_input);
        // glfwSetDropCallback(m_Window, droping_paths);

        // glfwSetErrorCallback
        // glfwSetFramebufferSizeCallback
        // glfwSetDropCallback

        glfwSwapInterval(1);

        LOG_CORE_INFO("OpenGL version: {}", reinterpret_cast<const char*>(glGetString(GL_VERSION)));
        LOG_CORE_INFO("OpenGL vendor: {}", reinterpret_cast<const char*>(glGetString(GL_VENDOR)));

        return true;
    }

    void GLFWWindow::SetCallbacks()
    {
        // glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* _Window, int _Width, int _Height)
        //{
        //	WindowData* Data = (WindowData*)glfwGetWindowUserPointer(_Window);
        //	Data->Width = _Width;
        //	Data->Height = _Height;
        //	Data->EQueue->Add(CreateRef<WindowResizeEvent>(_Width, _Height));
        // });

        // Set GLFW callbacks
        glfwSetWindowPosCallback(m_Window, [](GLFWwindow* window, int xpos, int ypos) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            GLFWmonitor* nowMonitor = NULL;
            if (!glfw_get_window_monitor(&nowMonitor, window))
            {
                nowMonitor = glfwGetPrimaryMonitor();
            }
            float newScale = 1.0f;
            glfwGetMonitorContentScale(nowMonitor, NULL, &newScale);

            if (newScale != data.MonitorScale)
            {
                data.MonitorScale = newScale;
                g_MonitorScale = newScale;

                WindowMonitorScaleChangedEvent event(newScale);
                data.EventCallback(event);
            }
        });

        glfwSetWindowSizeCallback(m_Window, [](GLFWwindow* window, int width, int height) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            data.Width = width;
            data.Height = height;

            WindowResizeEvent event(width, height);
            data.EventCallback(event);

            GLFWmonitor* nowMonitor = NULL;
            if (!glfw_get_window_monitor(&nowMonitor, window))
            {
                nowMonitor = glfwGetPrimaryMonitor();
            }
            float newScale = 1.0f;
            glfwGetMonitorContentScale(nowMonitor, NULL, &newScale);

            if (newScale != data.MonitorScale)
            {
                data.MonitorScale = newScale;
                g_MonitorScale = newScale;

                WindowMonitorScaleChangedEvent event(newScale);
                data.EventCallback(event);
            }
        });

        glfwSetWindowCloseCallback(m_Window, [](GLFWwindow* window) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);
            WindowCloseEvent event;
            data.EventCallback(event);
        });

        glfwSetKeyCallback(m_Window, [](GLFWwindow* window, int key, int scancode, int action, int mods) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            switch (action)
            {
                case GLFW_PRESS: {
                    KeyPressedEvent event(key, 0);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_RELEASE: {
                    KeyReleasedEvent event(key);
                    data.EventCallback(event);
                    break;
                }
                case GLFW_REPEAT: {
                    KeyPressedEvent event(key, true);
                    data.EventCallback(event);
                    break;
                }
            }
        });

        glfwSetCharCallback(m_Window, [](GLFWwindow* window, unsigned int keycode) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            KeyTypedEvent event(keycode);
            data.EventCallback(event);
        });

        glfwSetMouseButtonCallback(m_Window, [](GLFWwindow* window, int button, int action, int mods) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            switch (action)
            {
                case GLFW_PRESS: {
                    MouseButtonPressedEvent event(button);
                    data.EventCallback(event);
#ifndef _WIN32
                    if (button == GLFW_MOUSE_BUTTON_LEFT)
                    {
                        tryStartX11MoveResize(window);
                    }
#endif
                    break;
                }
                case GLFW_RELEASE: {
                    MouseButtonReleasedEvent event(button);
                    data.EventCallback(event);
                    break;
                }
            }
        });

        glfwSetScrollCallback(m_Window, [](GLFWwindow* window, double xOffset, double yOffset) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            MouseScrolledEvent event((float)xOffset, (float)yOffset);
            data.EventCallback(event);
        });

        glfwSetCursorPosCallback(m_Window, [](GLFWwindow* window, double xPos, double yPos) {
            WindowData& data = *(WindowData*)glfwGetWindowUserPointer(window);

            MouseMovedEvent event((float)xPos, (float)yPos);
            data.EventCallback(event);
        });
    }

}    // namespace LM
