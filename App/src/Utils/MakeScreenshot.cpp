#include "MakeScreenshot.hpp"

#ifdef _WIN32

#include <fstream>

#define UNICODE
#define _UNICODE

#include <string>
#include <windows.h>
#include <windowsx.h>

#include <gdiplus.h>

#pragma comment(lib, "gdiplus.lib")

namespace LM
{

    namespace
    {
        HINSTANCE g_hInst;
        POINT g_ptStart = { 0, 0 };
        POINT g_ptEnd = { 0, 0 };
        bool g_selecting = false;
        std::wstring g_savePath = L"screenshot.png";
        bool g_IsSaveResultOk = false;
    }    // namespace

    std::wstring StringToWString(const std::string& s)
    {
        if (s.empty())
        {
            return std::wstring();
        }
        int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), NULL, 0);
        std::wstring result(size_needed, 0);
        MultiByteToWideChar(CP_UTF8, 0, s.c_str(), (int)s.size(), &result[0], size_needed);
        return result;
    }

    void SaveBitmap(HBITMAP hBmp, const std::wstring& path)
    {
        Gdiplus::Bitmap bmp(hBmp, nullptr);
        CLSID clsid;
        CLSIDFromString(L"{557CF406-1A04-11D3-9A73-0000F81EF32E}", &clsid);    // PNG
        bmp.Save(path.c_str(), &clsid, nullptr);
    }

    void CaptureRect(const RECT& rc, const std::wstring& filename)
    {
        HDC hdcScreen = GetDC(nullptr);
        HDC hdcMem = CreateCompatibleDC(hdcScreen);

        int w = rc.right - rc.left;
        int h = rc.bottom - rc.top;

        HBITMAP hbm = CreateCompatibleBitmap(hdcScreen, w, h);
        SelectObject(hdcMem, hbm);

        BitBlt(hdcMem, 0, 0, w, h, hdcScreen, rc.left, rc.top, SRCCOPY);

        SaveBitmap(hbm, filename);

        DeleteObject(hbm);
        DeleteDC(hdcMem);
        ReleaseDC(nullptr, hdcScreen);
    }

    LRESULT CALLBACK WndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        switch (msg)
        {
            case WM_LBUTTONDOWN: {
                g_selecting = true;
                g_ptStart.x = GET_X_LPARAM(lParam);
                g_ptStart.y = GET_Y_LPARAM(lParam);
                g_ptEnd = g_ptStart;
                SetCapture(hwnd);
                return 0;
            }
            case WM_MOUSEMOVE: {
                if (g_selecting)
                {
                    g_ptEnd.x = GET_X_LPARAM(lParam);
                    g_ptEnd.y = GET_Y_LPARAM(lParam);
                    InvalidateRect(hwnd, nullptr, TRUE);
                }
                return 0;
            }
            case WM_LBUTTONUP: {
                if (g_selecting)
                {
                    g_selecting = false;
                    ReleaseCapture();

                    RECT rc;
                    rc.left = min(g_ptStart.x, g_ptEnd.x);
                    rc.top = min(g_ptStart.y, g_ptEnd.y);
                    rc.right = max(g_ptStart.x, g_ptEnd.x);
                    rc.bottom = max(g_ptStart.y, g_ptEnd.y);

                    DestroyWindow(hwnd);
                    PostQuitMessage(0);

                    CaptureRect(rc, g_savePath);
                    g_IsSaveResultOk = true;
                }
                return 0;
            }
            case WM_PAINT: {
                PAINTSTRUCT ps;
                HDC hdc = BeginPaint(hwnd, &ps);

                RECT rcClient;
                GetClientRect(hwnd, &rcClient);

                HBRUSH hBrush = CreateSolidBrush(RGB(0, 0, 0));
                FillRect(hdc, &rcClient, hBrush);
                DeleteObject(hBrush);

                if (g_selecting)
                {
                    RECT rcSel;
                    rcSel.left = min(g_ptStart.x, g_ptEnd.x);
                    rcSel.top = min(g_ptStart.y, g_ptEnd.y);
                    rcSel.right = max(g_ptStart.x, g_ptEnd.x);
                    rcSel.bottom = max(g_ptStart.y, g_ptEnd.y);

                    InvertRect(hdc, &rcSel);
                }

                EndPaint(hwnd, &ps);
                return 0;
            }
            case WM_KEYDOWN: {
                if (wParam == VK_ESCAPE)
                {
                    g_selecting = false;
                    DestroyWindow(hwnd);
                    PostQuitMessage(0);
                }
                return 0;
            }
        }

        return DefWindowProc(hwnd, msg, wParam, lParam);
    }

    void RunSnippingOverlay(const std::wstring& savePath)
    {
        g_savePath = savePath;

        WNDCLASS wc = {};
        wc.lpfnWndProc = WndProc;
        wc.hInstance = g_hInst;
        wc.lpszClassName = L"SnipOverlay";
        wc.hCursor = LoadCursor(nullptr, IDC_CROSS);

        RegisterClass(&wc);

        int x = GetSystemMetrics(SM_XVIRTUALSCREEN);
        int y = GetSystemMetrics(SM_YVIRTUALSCREEN);
        int w = GetSystemMetrics(SM_CXVIRTUALSCREEN);
        int h = GetSystemMetrics(SM_CYVIRTUALSCREEN);

        HWND hwnd = CreateWindowEx(WS_EX_TOPMOST | WS_EX_LAYERED, wc.lpszClassName, L"", WS_POPUP, x, y, w, h, nullptr,
                                   nullptr, g_hInst, nullptr);

        SetLayeredWindowAttributes(hwnd, 0, 64, LWA_ALPHA);
        ShowWindow(hwnd, SW_SHOW);

        MSG msg;
        while (GetMessage(&msg, nullptr, 0, 0))
        {
            TranslateMessage(&msg);
            DispatchMessage(&msg);
        }
    }

    bool MakeScreenshot(const std::string& outputPath)
    {
        g_IsSaveResultOk = false;
        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr);

        RunSnippingOverlay(StringToWString(outputPath));

        Gdiplus::GdiplusShutdown(gdiplusToken);

        return g_IsSaveResultOk;
    }

    bool MakeScreenshotFromClipboard(const std::string& _OutputPath)
    {
        g_IsSaveResultOk = false;
        if (!OpenClipboard(nullptr))
        {
            return false;
        }

        HBITMAP hBitmap = (HBITMAP)GetClipboardData(CF_BITMAP);
        if (!hBitmap)
        {
            CloseClipboard();
            return false;
        }

        Gdiplus::GdiplusStartupInput gdiplusStartupInput;
        ULONG_PTR gdiplusToken;
        if (Gdiplus::GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, nullptr) != Gdiplus::Ok)
        {
            CloseClipboard();
            return false;
        }

        {
            Gdiplus::Bitmap bmp(hBitmap, nullptr);

            CLSID clsid;
            {
                UINT num = 0, size = 0;
                Gdiplus::GetImageEncodersSize(&num, &size);
                if (size == 0)
                {
                    Gdiplus::GdiplusShutdown(gdiplusToken);
                    CloseClipboard();
                    return false;
                }
                auto* pImageCodecInfo = (Gdiplus::ImageCodecInfo*)(malloc(size));
                GetImageEncoders(num, size, pImageCodecInfo);
                for (UINT j = 0; j < num; ++j)
                {
                    if (wcscmp(pImageCodecInfo[j].MimeType, L"image/png") == 0)
                    {
                        clsid = pImageCodecInfo[j].Clsid;
                        break;
                    }
                }
                free(pImageCodecInfo);
            }

            if (bmp.Save(StringToWString(_OutputPath).c_str(), &clsid, nullptr) != Gdiplus::Ok)
            {
                Gdiplus::GdiplusShutdown(gdiplusToken);
                CloseClipboard();
                return false;
            }
        }

        Gdiplus::GdiplusShutdown(gdiplusToken);
        CloseClipboard();

        return true;
    }

}    // namespace LM

#else    // Linux / X11

#include <algorithm>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <string>
#include <vector>

#include <sys/select.h>

#include <X11/Xlib.h>
#include <X11/Xutil.h>
#include <X11/cursorfont.h>
#include <X11/keysym.h>

#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include "stb_image_write.h"

namespace LM
{

    namespace
    {

        // Дефолтный обработчик ошибок X11 вызывает exit(1); ожидаемые ошибки
        // (BadMatch от XGetImage, отсутствующий selection owner) не должны убивать приложение.
        int NoopXErrorHandler(Display*, XErrorEvent*) { return 0; }

        struct DisplayCloser
        {
            Display* Handle;

            explicit DisplayCloser(Display* _Handle) : Handle(_Handle) {}
            ~DisplayCloser()
            {
                if (Handle != nullptr)
                {
                    XCloseDisplay(Handle);
                }
            }

            DisplayCloser(const DisplayCloser&) = delete;
            DisplayCloser& operator=(const DisplayCloser&) = delete;
        };

        // Сдвиг и маска одного цветового канала TrueColor-изображения.
        struct ChannelMask
        {
            int Shift = 0;
            unsigned long Mask = 0;
        };

        ChannelMask MakeChannelMask(unsigned long _Mask)
        {
            ChannelMask mask;
            if (_Mask != 0)
            {
                mask.Shift = __builtin_ctzl(_Mask);
                mask.Mask = _Mask >> mask.Shift;
            }
            return mask;
        }

        unsigned char ChannelTo8Bit(unsigned long _Pixel, const ChannelMask& _Mask)
        {
            if (_Mask.Mask == 0)
            {
                return 0;
            }
            const unsigned long value = (_Pixel >> _Mask.Shift) & _Mask.Mask;
            return static_cast<unsigned char>((value * 255UL) / _Mask.Mask);
        }

        unsigned long ChannelFrom8Bit(unsigned char _Value, const ChannelMask& _Mask)
        {
            if (_Mask.Mask == 0)
            {
                return 0;
            }
            return ((static_cast<unsigned long>(_Value) * _Mask.Mask) / 255UL) << _Mask.Shift;
        }

        unsigned long DarkenPixel(unsigned long _Pixel, const ChannelMask& _Red, const ChannelMask& _Green,
                                  const ChannelMask& _Blue)
        {
            // Паритет с Win32 SetLayeredWindowAttributes(alpha = 64): 25% чёрного поверх кадра.
            const unsigned char r = static_cast<unsigned char>(ChannelTo8Bit(_Pixel, _Red) * 191 / 255);
            const unsigned char g = static_cast<unsigned char>(ChannelTo8Bit(_Pixel, _Green) * 191 / 255);
            const unsigned char b = static_cast<unsigned char>(ChannelTo8Bit(_Pixel, _Blue) * 191 / 255);
            return ChannelFrom8Bit(r, _Red) | ChannelFrom8Bit(g, _Green) | ChannelFrom8Bit(b, _Blue);
        }

        // Ожидание события конкретного типа с дедлайном: в X нет нативных таймаутов,
        // поэтому poll через XCheckTypedEvent + select() на fd соединения.
        bool WaitForX11Event(Display* _Display, int _EventType, XEvent* _OutEvent, int _TimeoutMs)
        {
            const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(_TimeoutMs);
            const int connectionFd = ConnectionNumber(_Display);

            for (;;)
            {
                if (XCheckTypedEvent(_Display, _EventType, _OutEvent))
                {
                    return true;
                }

                const auto remaining =
                    std::chrono::duration_cast<std::chrono::milliseconds>(deadline - std::chrono::steady_clock::now());
                if (remaining <= std::chrono::milliseconds(0))
                {
                    return false;
                }

                const auto slice = std::min(remaining, std::chrono::milliseconds(100));
                timeval timeout;
                timeout.tv_sec = static_cast<time_t>(slice.count() / 1000);
                timeout.tv_usec = static_cast<suseconds_t>((slice.count() % 1000) * 1000);

                fd_set readFds;
                FD_ZERO(&readFds);
                FD_SET(connectionFd, &readFds);
                if (select(connectionFd + 1, &readFds, nullptr, nullptr, &timeout) < 0)
                {
                    return false;
                }
            }
        }

        // Читает результат запроса селекции из свойства окна, включая
        // инкрементальную передачу INCR. Свойство всегда забирается (delete=True).
        bool ReadSelectionProperty(Display* _Display, Window _Window, Atom _Property,
                                   std::vector<unsigned char>& _Data)
        {
            const Atom incrAtom = XInternAtom(_Display, "INCR", False);

            Atom actualType = None;
            int actualFormat = 0;
            unsigned long itemCount = 0;
            unsigned long bytesAfter = 0;
            unsigned char* buffer = nullptr;

            // Нулевой probe-запрос: узнаём тип свойства (INCR или готовые данные).
            if (XGetWindowProperty(_Display, _Window, _Property, 0, 0, False, AnyPropertyType, &actualType,
                                   &actualFormat, &itemCount, &bytesAfter, &buffer) != Success)
            {
                if (buffer != nullptr) { XFree(buffer); }
                return false;
            }
            if (buffer != nullptr) { XFree(buffer); buffer = nullptr; }

            if (actualType == incrAtom)
            {
                for (;;)
                {
                    XEvent event = {};
                    if (!WaitForX11Event(_Display, PropertyNotify, &event, 2000))
                    {
                        return false;
                    }
                    if (event.xproperty.window != _Window || event.xproperty.atom != _Property ||
                        event.xproperty.state != PropertyNewValue)
                    {
                        continue;
                    }

                    if (XGetWindowProperty(_Display, _Window, _Property, 0, 16384, True, AnyPropertyType, &actualType,
                                           &actualFormat, &itemCount, &bytesAfter, &buffer) != Success)
                    {
                        if (buffer != nullptr) { XFree(buffer); }
                        return false;
                    }
                    if (itemCount == 0)
                    {
                        // Пустой чанк — конец INCR-передачи.
                        if (buffer != nullptr) { XFree(buffer); }
                        break;
                    }
                    if (buffer != nullptr)
                    {
                        _Data.insert(_Data.end(), buffer,
                                     buffer + itemCount * static_cast<unsigned long>(actualFormat) / 8);
                        XFree(buffer);
                        buffer = nullptr;
                    }
                }
            }
            else if (actualType != None)
            {
                if (XGetWindowProperty(_Display, _Window, _Property, 0, 0x1FFFFFFF, True, AnyPropertyType, &actualType,
                                       &actualFormat, &itemCount, &bytesAfter, &buffer) != Success)
                {
                    if (buffer != nullptr) { XFree(buffer); }
                    return false;
                }
                if (buffer != nullptr && itemCount > 0)
                {
                    _Data.assign(buffer, buffer + itemCount * static_cast<unsigned long>(actualFormat) / 8);
                }
                if (buffer != nullptr) { XFree(buffer); }
            }
            else
            {
                return false;
            }

            return true;
        }

    }    // namespace

    // Скриншот выбранной области поверх «замороженного экрана»: весь рабочий стол
    // заранее снимается в XImage, его затемнённая копия показывается фоном
    // полноэкранного оверлея. Работает и без композитора (в отличие от полупрозрачного
    // ARGB-окна), а сохранённый кадр не содержит сам оверлей (паритет с Win32,
    // где DestroyWindow вызывается до CaptureRect).
    //
    // Пиксели читаются/пишутся только через XGetPixel/XPutPixel: данные XImage не
    // выровнены под 32-битные обращения, а в сборке активен UBSan(alignment).
    //
    // TODO(linux): под Wayland-сессией root XWayland содержит только X11-клиентов —
    // нативные Wayland-окна (включая панель GNOME) в скриншот не попадают, а X-grab'ы
    // не перехватывают их ввод. Полная точность возможна в Xorg-сессии либо через
    // XDG desktop portal / gnome-screenshot.
    bool MakeScreenshot(const std::string& _OutputPath)
    {
        Display* display = XOpenDisplay(nullptr);
        if (display == nullptr)
        {
            return false;
        }
        DisplayCloser displayCloser(display);
        const auto previousErrorHandler = XSetErrorHandler(NoopXErrorHandler);

        const int screen = DefaultScreen(display);
        const Window root = RootWindow(display, screen);
        const int screenWidth = DisplayWidth(display, screen);
        const int screenHeight = DisplayHeight(display, screen);

        XImage* shot = nullptr;
        XImage* darkened = nullptr;
        Pixmap backgroundPixmap = None;
        GC pixmapGC = None;
        GC invertGC = None;
        Cursor crosshairCursor = None;
        Window overlayWindow = None;
        bool pointerGrabbed = false;
        bool keyboardGrabbed = false;
        bool selectionDone = false;
        bool saved = false;

        int selX0 = 0;
        int selY0 = 0;
        int selX1 = 0;
        int selY1 = 0;

        shot = XGetImage(display, root, 0, 0, screenWidth, screenHeight, AllPlanes, ZPixmap);
        if (shot != nullptr && shot->format == ZPixmap && shot->data != nullptr)
        {
            // Второй снимок — копия для затемнения: Xlib сама владеет его буфером,
            // XDestroyImage корректно освободит оба (никаких malloc/XFree пересечений).
            darkened = XGetImage(display, root, 0, 0, screenWidth, screenHeight, AllPlanes, ZPixmap);
        }

        if (shot != nullptr && darkened != nullptr && darkened->data != nullptr)
        {
            const ChannelMask redMask = MakeChannelMask(shot->red_mask);
            const ChannelMask greenMask = MakeChannelMask(shot->green_mask);
            const ChannelMask blueMask = MakeChannelMask(shot->blue_mask);

            for (int y = 0; y < darkened->height; ++y)
            {
                for (int x = 0; x < darkened->width; ++x)
                {
                    const unsigned long pixel = XGetPixel(shot, x, y);
                    XPutPixel(darkened, x, y, DarkenPixel(pixel, redMask, greenMask, blueMask));
                }
            }

            backgroundPixmap = XCreatePixmap(display, root, static_cast<unsigned int>(screenWidth),
                                             static_cast<unsigned int>(screenHeight),
                                             static_cast<unsigned int>(darkened->depth));
            pixmapGC = XCreateGC(display, backgroundPixmap, 0, nullptr);
            XPutImage(display, backgroundPixmap, pixmapGC, darkened, 0, 0, 0, 0,
                      static_cast<unsigned int>(screenWidth), static_cast<unsigned int>(screenHeight));

            crosshairCursor = XCreateFontCursor(display, XC_crosshair);

            XSetWindowAttributes attributes = {};
            attributes.override_redirect = True;                 // поверх всех WM-окон
            attributes.background_pixmap = backgroundPixmap;     // сервер сам тайлит фон, Expose не нужен
            attributes.cursor = crosshairCursor;
            attributes.event_mask =
                ExposureMask | KeyPressMask | ButtonPressMask | ButtonMotionMask | ButtonReleaseMask;

            overlayWindow = XCreateWindow(display, root, 0, 0, static_cast<unsigned int>(screenWidth),
                                          static_cast<unsigned int>(screenHeight), 0,
                                          static_cast<unsigned int>(darkened->depth), InputOutput,
                                          DefaultVisual(display, screen),
                                          CWOverrideRedirect | CWBackPixmap | CWCursor | CWEventMask, &attributes);

            // GXinvert — X-аналог Win32 InvertRect: повторная отрисовка того же
            // прямоугольника стирает его.
            invertGC = XCreateGC(display, overlayWindow, 0, nullptr);
            XSetFunction(display, invertGC, GXinvert);
            XSetSubwindowMode(display, invertGC, IncludeInferiors);
            XSetLineAttributes(display, invertGC, 2, LineSolid, CapButt, JoinMiter);

            XMapRaised(display, overlayWindow);
            XSync(display, False);

            pointerGrabbed =
                (XGrabPointer(display, overlayWindow, True, ButtonPressMask | ButtonMotionMask | ButtonReleaseMask,
                              GrabModeAsync, GrabModeAsync, None, crosshairCursor, CurrentTime) == GrabSuccess);
            keyboardGrabbed = (XGrabKeyboard(display, overlayWindow, True, GrabModeAsync, GrabModeAsync,
                                             CurrentTime) == GrabSuccess);
        }

        if (pointerGrabbed)
        {
            bool selecting = false;
            bool rectOnScreen = false;
            bool loopDone = false;

            const auto drawSelectionRect = [&]()
            {
                const int x = std::min(selX0, selX1);
                const int y = std::min(selY0, selY1);
                const int width = std::abs(selX1 - selX0);
                const int height = std::abs(selY1 - selY0);
                XDrawRectangle(display, overlayWindow, invertGC, x, y, static_cast<unsigned int>(width),
                               static_cast<unsigned int>(height));
            };

            while (!loopDone)
            {
                XEvent event = {};
                XNextEvent(display, &event);

                switch (event.type)
                {
                    case Expose:
                        if (selecting && rectOnScreen && event.xexpose.count == 0)
                        {
                            drawSelectionRect();
                        }
                        break;
                    case ButtonPress:
                        if (event.xbutton.button == Button1 && !selecting)
                        {
                            selecting = true;
                            selX0 = selX1 = event.xbutton.x;
                            selY0 = selY1 = event.xbutton.y;
                            drawSelectionRect();
                            rectOnScreen = true;
                        }
                        else if (event.xbutton.button == Button3)
                        {
                            loopDone = true;    // отмена
                        }
                        break;
                    case MotionNotify:
                        if (selecting)
                        {
                            if (rectOnScreen)
                            {
                                drawSelectionRect();    // стереть предыдущий прямоугольник
                                rectOnScreen = false;
                            }
                            selX1 = event.xmotion.x;
                            selY1 = event.xmotion.y;
                            drawSelectionRect();
                            rectOnScreen = true;
                        }
                        break;
                    case ButtonRelease:
                        if (event.xbutton.button == Button1 && selecting)
                        {
                            selectionDone = true;
                            loopDone = true;
                        }
                        break;
                    case KeyPress:
                        if (XLookupKeysym(&event.xkey, 0) == XK_Escape)
                        {
                            loopDone = true;    // отмена
                        }
                        break;
                    default:
                        break;
                }
            }

            if (rectOnScreen)
            {
                drawSelectionRect();    // стереть рамку до закрытия оверлея
            }
        }

        // Разбор оверлея до сохранения (паритет с Win32) и до кропа — в кадр не должно
        // попасть ничего лишнего.
        if (keyboardGrabbed) { XUngrabKeyboard(display, CurrentTime); }
        if (pointerGrabbed) { XUngrabPointer(display, CurrentTime); }
        if (overlayWindow != None) { XDestroyWindow(display, overlayWindow); }
        if (invertGC != None) { XFreeGC(display, invertGC); }
        if (pixmapGC != None) { XFreeGC(display, pixmapGC); }
        if (backgroundPixmap != None) { XFreePixmap(display, backgroundPixmap); }
        if (crosshairCursor != None) { XFreeCursor(display, crosshairCursor); }
        if (darkened != nullptr) { XDestroyImage(darkened); }
        XSync(display, False);

        if (selectionDone && shot != nullptr)
        {
            const ChannelMask redMask = MakeChannelMask(shot->red_mask);
            const ChannelMask greenMask = MakeChannelMask(shot->green_mask);
            const ChannelMask blueMask = MakeChannelMask(shot->blue_mask);

            const int x = std::min(selX0, selX1);
            const int y = std::min(selY0, selY1);
            const int width = std::abs(selX1 - selX0);
            const int height = std::abs(selY1 - selY0);

            if (width > 0 && height > 0)
            {
                std::vector<unsigned char> rgba(static_cast<size_t>(width) * static_cast<size_t>(height) * 4);
                for (int dy = 0; dy < height; ++dy)
                {
                    for (int dx = 0; dx < width; ++dx)
                    {
                        const unsigned long pixel = XGetPixel(shot, x + dx, y + dy);
                        unsigned char* out = &rgba[(static_cast<size_t>(dy) * static_cast<size_t>(width) + dx) * 4];
                        out[0] = ChannelTo8Bit(pixel, redMask);
                        out[1] = ChannelTo8Bit(pixel, greenMask);
                        out[2] = ChannelTo8Bit(pixel, blueMask);
                        out[3] = 255;
                    }
                }

                saved = stbi_write_png(_OutputPath.c_str(), width, height, 4, rgba.data(), width * 4) != 0;
            }
        }

        if (shot != nullptr) { XDestroyImage(shot); }
        XSetErrorHandler(previousErrorHandler);
        return saved;
    }

    bool MakeScreenshotFromClipboard(const std::string& _OutputPath)
    {
        Display* display = XOpenDisplay(nullptr);
        if (display == nullptr)
        {
            return false;
        }
        DisplayCloser displayCloser(display);
        const auto previousErrorHandler = XSetErrorHandler(NoopXErrorHandler);

        bool saved = false;
        Window requestor = None;

        const Atom clipboardAtom = XInternAtom(display, "CLIPBOARD", False);
        const Atom dataPropertyAtom = XInternAtom(display, "XSEL_DATA", False);

        // Без владельца селекции запрашивать нечего — быстрый выход без таймаутов.
        if (XGetSelectionOwner(display, clipboardAtom) != None)
        {
            // Окно-запросчик 1x1, не отображается; нужно лишь для получения SelectionNotify.
            requestor = XCreateSimpleWindow(display, DefaultRootWindow(display), -1, -1, 1, 1, 0, 0, 0);
            XSelectInput(display, requestor, PropertyChangeMask);

            // Покрываем типичные источники: скриншот-тулзы кладут PNG,
            // GTK/Qt — BMP, браузеры — PNG/JPEG.
            const char* const targets[] = { "image/png", "image/bmp", "image/jpeg" };
            for (const char* targetName : targets)
            {
                const Atom targetAtom = XInternAtom(display, targetName, False);
                XConvertSelection(display, clipboardAtom, targetAtom, dataPropertyAtom, requestor, CurrentTime);
                XFlush(display);

                XEvent event = {};
                if (!WaitForX11Event(display, SelectionNotify, &event, 1000))
                {
                    continue;    // владелец не ответил — пробуем следующий таргет
                }
                if (event.xselection.property == None || event.xselection.requestor != requestor ||
                    event.xselection.target != targetAtom)
                {
                    continue;    // таргет недоступен
                }

                std::vector<unsigned char> data;
                if (!ReadSelectionProperty(display, requestor, dataPropertyAtom, data) || data.empty())
                {
                    continue;
                }

                static const unsigned char kPngSignature[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
                if (data.size() >= sizeof(kPngSignature) &&
                    std::memcmp(data.data(), kPngSignature, sizeof(kPngSignature)) == 0)
                {
                    // Уже PNG — записываем байты как есть, без перекодирования.
                    std::ofstream file(_OutputPath, std::ios::binary);
                    if (file)
                    {
                        file.write(reinterpret_cast<const char*>(data.data()),
                                   static_cast<std::streamsize>(data.size()));
                        saved = file.good();
                    }
                }
                else
                {
                    int width = 0;
                    int height = 0;
                    int channels = 0;
                    unsigned char* pixels = stbi_load_from_memory(data.data(), static_cast<int>(data.size()), &width,
                                                                  &height, &channels, STBI_rgb_alpha);
                    if (pixels != nullptr)
                    {
                        saved = stbi_write_png(_OutputPath.c_str(), width, height, 4, pixels, width * 4) != 0;
                        stbi_image_free(pixels);
                    }
                }

                if (saved)
                {
                    break;
                }
            }
        }

        if (requestor != None) { XDestroyWindow(display, requestor); }
        XSetErrorHandler(previousErrorHandler);
        return saved;
    }

}    // namespace LM

#endif    // _WIN32
