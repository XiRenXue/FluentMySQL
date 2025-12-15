#pragma once
#include "def.h"
#include <Windows.h>
#include <Richedit.h>
#include <functional>
#include <string_view>
#include <array>

#pragma comment(lib, "comctl32.lib")

namespace UIHandles
{
    inline HWND InputEdit = nullptr;
    inline HWND OutputEdit = nullptr;
    inline HWND StatusText = nullptr;
    inline HWND ConnectionDialog = nullptr;
    inline HWND HostEdit = nullptr;
    inline HWND PortEdit = nullptr;
    inline HWND UserEdit = nullptr;
    inline HWND PasswordEdit = nullptr;
    inline HWND DatabaseEdit = nullptr;
}

namespace RenderState
{
    inline HWND WindowHandle = nullptr;
    inline HFONT CurrentFont = nullptr;
    inline HFONT MonospaceFont = nullptr;
    inline int CurrentDPI = 96;
    inline HMODULE RichEditModule = nullptr;
}

[[nodiscard]] inline auto GetWindowDPI(HWND WindowHandle) noexcept -> int
{
    const HDC DeviceContext = GetDC(WindowHandle);
    if (!DeviceContext) [[unlikely]]
        return 96;
    const int DpiValue = GetDeviceCaps(DeviceContext, LOGPIXELSX);
    ReleaseDC(WindowHandle, DeviceContext);
    return DpiValue;
}

[[nodiscard]] inline auto ScaleForDPI(int Value, int DpiValue) noexcept -> int
{
    return MulDiv(Value, DpiValue, 96);
}

[[nodiscard]] inline auto CreateScaledFont(int DpiValue) noexcept -> HFONT
{
    constexpr int BaseFontSize = 12;
    const int FontHeight = -ScaleForDPI(BaseFontSize, DpiValue);
    return CreateFontW(FontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
}

[[nodiscard]] inline auto CreateMonospaceFont(int DpiValue) noexcept -> HFONT
{
    constexpr int BaseFontSize = 12;
    const int FontHeight = -ScaleForDPI(BaseFontSize, DpiValue);
    return CreateFontW(FontHeight, 0, 0, 0, FW_NORMAL, FALSE, FALSE, FALSE, DEFAULT_CHARSET, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS, CLEARTYPE_QUALITY, DEFAULT_PITCH | FF_DONTCARE, L"Microsoft YaHei");
}

inline auto SetRichEditFont(HWND RichEditHandle, HFONT FontHandle) noexcept -> void
{
    try
    {
        if (!RichEditHandle || !IsWindow(RichEditHandle) || !FontHandle) [[unlikely]]
            return;
        LOGFONTW LogFont{};
        if (GetObjectW(FontHandle, sizeof(LOGFONTW), &LogFont) == 0)
            return;
        CHARFORMAT2W CharFormat{};
        CharFormat.cbSize = sizeof(CHARFORMAT2W);
        CharFormat.dwMask = CFM_FACE | CFM_SIZE | CFM_CHARSET;
        CharFormat.yHeight = -LogFont.lfHeight * 1440 / 96;
        CharFormat.bCharSet = LogFont.lfCharSet;
        wcscpy_s(CharFormat.szFaceName, LogFont.lfFaceName);
        SendMessageW(RichEditHandle, EM_SETCHARFORMAT, SCF_ALL, reinterpret_cast<LPARAM>(&CharFormat));
    }
    catch (...)
    {
        OutputDebugStringA("SetRichEditFont: 异常\n");
    }
}

inline auto UpdateAllFonts(HWND ParentWindow, int DpiValue) noexcept -> void
{
    try
    {
        if (RenderState::CurrentFont)
        {
            DeleteObject(RenderState::CurrentFont);
            RenderState::CurrentFont = nullptr;
        }
        if (RenderState::MonospaceFont)
        {
            DeleteObject(RenderState::MonospaceFont);
            RenderState::MonospaceFont = nullptr;
        }
        RenderState::CurrentFont = CreateScaledFont(DpiValue);
        RenderState::MonospaceFont = CreateMonospaceFont(DpiValue);
        RenderState::CurrentDPI = DpiValue;
        EnumChildWindows(ParentWindow, [](HWND ChildWindow, LPARAM FontParam) -> BOOL 
            {
            wchar_t ClassName[256] = {};
            GetClassNameW(ChildWindow, ClassName, _countof(ClassName));
            if (wcscmp(ClassName, MSFTEDIT_CLASS) == 0)
                SetRichEditFont(ChildWindow, reinterpret_cast<HFONT>(FontParam));
            else
                SendMessageW(ChildWindow, WM_SETFONT, static_cast<WPARAM>(FontParam), TRUE);
            return TRUE;
            }, reinterpret_cast<LPARAM>(RenderState::CurrentFont));
        InvalidateRect(ParentWindow, nullptr, TRUE);
    }
    catch (...)
    {
        OutputDebugStringA("UpdateAllFonts: 异常\n");
    }
}

[[nodiscard]] inline auto Utf8ToWide(std::string_view Utf8String) noexcept -> std::wstring
{
    try
    {
        if (Utf8String.empty())
            return L"";
        const int WideLength = MultiByteToWideChar(CP_UTF8, 0, Utf8String.data(), static_cast<int>(Utf8String.size()), nullptr, 0);
        if (WideLength <= 0) [[unlikely]]
        {
            OutputDebugStringA(std::format("UTF8转换失败: GetLastError={}\n", GetLastError()).c_str());
            return L"";
        }
        std::wstring WideString(static_cast<std::size_t>(WideLength), L'\0');
        const int ConversionResult = MultiByteToWideChar(CP_UTF8, 0, Utf8String.data(), static_cast<int>(Utf8String.size()), WideString.data(), WideLength);
        if (ConversionResult == 0) [[unlikely]]
        {
            OutputDebugStringA(std::format("UTF8转换失败(步骤2): GetLastError={}\n", GetLastError()).c_str());
            return L"";
        }
        return WideString;
    }
    catch (const std::bad_alloc&)
    {
        OutputDebugStringA("UTF8转换: 内存分配失败\n");
        return L"";
    }
    catch (...)
    {
        OutputDebugStringA("UTF8转换: 未知异常\n");
        return L"";
    }
}

[[nodiscard]] inline auto WideToUtf8(std::wstring_view WideString) noexcept -> std::string
{
    try
    {
        if (WideString.empty())
            return "";
        const int Utf8Length = WideCharToMultiByte(CP_UTF8, 0, WideString.data(), static_cast<int>(WideString.size()), nullptr, 0, nullptr, nullptr);
        if (Utf8Length <= 0) [[unlikely]]
        {
            OutputDebugStringA(std::format("Wide转UTF8失败: GetLastError={}\n", GetLastError()).c_str());
            return "";
        }
        std::string Utf8String(static_cast<std::size_t>(Utf8Length), '\0');
        const int ConversionResult = WideCharToMultiByte(CP_UTF8, 0, WideString.data(), static_cast<int>(WideString.size()), Utf8String.data(), Utf8Length, nullptr, nullptr);
        if (ConversionResult == 0) [[unlikely]]
        {
            OutputDebugStringA(std::format("Wide转UTF8失败(步骤2): GetLastError={}\n", GetLastError()).c_str());
            return "";
        }
        return Utf8String;
    }
    catch (const std::bad_alloc&)
    {
        OutputDebugStringA("Wide转UTF8: 内存分配失败\n");
        return "";
    }
    catch (...)
    {
        OutputDebugStringA("Wide转UTF8: 未知异常\n");
        return "";
    }
}

[[nodiscard]] inline auto GetEditText(HWND EditWindow) noexcept -> std::string
{
    try
    {
        const int TextLength = GetWindowTextLengthW(EditWindow);
        if (TextLength == 0)
            return "";
        std::wstring TextBuffer(static_cast<std::size_t>(TextLength) + 1, L'\0');
        GetWindowTextW(EditWindow, TextBuffer.data(), TextLength + 1);
        TextBuffer.resize(TextLength);
        return WideToUtf8(TextBuffer);
    }
    catch (...)
    {
        OutputDebugStringA("GetEditText: 异常\n");
        return "";
    }
}

inline auto ClearEditText(HWND EditWindow) noexcept -> void
{
    try
    {
        if (!EditWindow || !IsWindow(EditWindow)) [[unlikely]]
            return;
        SendMessageW(EditWindow, WM_SETREDRAW, FALSE, 0);
        SendMessageW(EditWindow, WM_SETTEXT, 0, reinterpret_cast<LPARAM>(L""));
        SendMessageW(EditWindow, EM_SETSEL, 0, 0);
        SendMessageW(EditWindow, EM_SETMODIFY, FALSE, 0);
        SendMessageW(EditWindow, WM_VSCROLL, SB_TOP, 0);
        SendMessageW(EditWindow, WM_HSCROLL, SB_LEFT, 0);
        SendMessageW(EditWindow, WM_SETREDRAW, TRUE, 0);
        RedrawWindow(EditWindow, nullptr, nullptr, RDW_ERASE | RDW_FRAME | RDW_INVALIDATE | RDW_ALLCHILDREN);
    }
    catch (...)
    {
        OutputDebugStringA("ClearEditText: 异常\n");
    }
}

inline auto SetEditText(HWND EditWindow, std::string_view TextContent) noexcept -> void
{
    try
    {
        if (!EditWindow || !IsWindow(EditWindow)) [[unlikely]]
            return;
        const std::wstring WideText = Utf8ToWide(TextContent);
        const int CurrentLength = GetWindowTextLengthW(EditWindow);
        if (CurrentLength > 0)
        {
            SendMessageW(EditWindow, EM_SETSEL, CurrentLength, CurrentLength);
            SendMessageW(EditWindow, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L"\r\n---\r\n"));
        }
        const int NewLength = GetWindowTextLengthW(EditWindow);
        SendMessageW(EditWindow, EM_SETSEL, NewLength, NewLength);
        SendMessageW(EditWindow, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(WideText.c_str()));
        const int FinalLength = static_cast<int>(SendMessageW(EditWindow, WM_GETTEXTLENGTH, 0, 0));
        SendMessageW(EditWindow, EM_SETSEL, FinalLength, FinalLength);
        SendMessageW(EditWindow, EM_SCROLLCARET, 0, 0);
    }
    catch (...)
    {
        OutputDebugStringA("SetEditText: 异常\n");
    }
}

inline auto AppendEditText(HWND EditWindow, std::string_view TextContent) noexcept -> void
{
    try
    {
        if (!EditWindow || !IsWindow(EditWindow)) [[unlikely]]
            return;
        const std::wstring WideText = Utf8ToWide(TextContent);
        const int CurrentLength = static_cast<int>(SendMessageW(EditWindow, WM_GETTEXTLENGTH, 0, 0));
        SendMessageW(EditWindow, EM_SETSEL, CurrentLength, CurrentLength);
        SendMessageW(EditWindow, EM_REPLACESEL, FALSE,reinterpret_cast<LPARAM>(WideText.c_str()));
        SendMessageW(EditWindow, EM_SCROLLCARET, 0, 0);
    }
    catch (...)
    {
        OutputDebugStringA("AppendEditText: 异常\n");
    }
}

inline auto AppendEditTextWithTimestamp(HWND EditWindow, std::string_view TextContent) noexcept -> void
{
    try
    {
        if (!EditWindow || !IsWindow(EditWindow)) [[unlikely]]
            return;
        const std::wstring WideText = Utf8ToWide(TextContent);
        const int CurrentLength = static_cast<int>(SendMessageW(EditWindow, WM_GETTEXTLENGTH, 0, 0));
        if (CurrentLength > 0)
        {
            SendMessageW(EditWindow, EM_SETSEL, CurrentLength, CurrentLength);
            SendMessageW(EditWindow, EM_REPLACESEL, FALSE, reinterpret_cast<LPARAM>(L"\r\n"));
        }
        const int NewLength = static_cast<int>(SendMessageW(EditWindow, WM_GETTEXTLENGTH, 0, 0));
        SendMessageW(EditWindow, EM_SETSEL, NewLength, NewLength);
        SendMessageW(EditWindow, EM_REPLACESEL, FALSE,reinterpret_cast<LPARAM>(WideText.c_str()));
        SendMessageW(EditWindow, EM_SCROLLCARET, 0, 0);
    }
    catch (...)
    {
        OutputDebugStringA("AppendEditTextWithTimestamp: 异常\n");
    }
}

[[nodiscard]] inline auto CreateRichEditControl(HWND ParentWindow, int X, int Y, int Width, int Height, DWORD AdditionalStyles = 0, bool ReadOnly = false) noexcept -> HWND
{
    try
    {
        DWORD BaseStyles = WS_CHILD | WS_VISIBLE | WS_BORDER | ES_MULTILINE | ES_AUTOVSCROLL;
        if (ReadOnly)
            BaseStyles |= ES_READONLY;
        HWND RichEditHandle = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", BaseStyles | AdditionalStyles, X, Y, Width, Height, ParentWindow, nullptr, nullptr, nullptr);
        if (RichEditHandle) [[likely]]
        {
            SendMessageW(RichEditHandle, EM_SETEVENTMASK, 0, ENM_NONE);
            SendMessageW(RichEditHandle, EM_SETBKGNDCOLOR, 0, RGB(255, 255, 255));
            SendMessageW(RichEditHandle, EM_SETLANGOPTIONS, 0, 0);
            SendMessageW(RichEditHandle, EM_SETOPTIONS, ECOOP_OR, ECO_AUTOVSCROLL | ECO_AUTOHSCROLL | ECO_NOHIDESEL);
            if (RenderState::CurrentFont)
                SetRichEditFont(RichEditHandle, RenderState::CurrentFont);
        }
        return RichEditHandle;
    }
    catch (...)
    {
        OutputDebugStringA("CreateRichEditControl: 异常\n");
        return nullptr;
    }
}

inline auto CreateUIControls(HWND ParentWindow) -> void
{
    try
    {
        if (!RenderState::RichEditModule)
        {
            RenderState::RichEditModule = LoadLibraryW(L"Msftedit.dll");
            if (!RenderState::RichEditModule) [[unlikely]]
            {
                MessageBoxW(ParentWindow, L"无法加载 RichEdit 库", L"错误", MB_OK | MB_ICONERROR);
                return;
            }
        }
        const int DpiValue = GetWindowDPI(ParentWindow);
        RenderState::CurrentFont = CreateScaledFont(DpiValue);
        RenderState::MonospaceFont = CreateMonospaceFont(DpiValue);
        RenderState::CurrentDPI = DpiValue;
        RECT ClientRect;
        GetClientRect(ParentWindow, &ClientRect);
        const int ClientWidth = ClientRect.right - ClientRect.left;
        const int ClientHeight = ClientRect.bottom - ClientRect.top;
        const int MarginSize = ScaleForDPI(10, DpiValue);
        const int ButtonWidth = ScaleForDPI(100, DpiValue);
        const int ButtonHeight = ScaleForDPI(30, DpiValue);
        const int InputHeight = ScaleForDPI(150, DpiValue);
        const int StatusHeight = ScaleForDPI(25, DpiValue);
        const int LabelHeight = ScaleForDPI(20, DpiValue);
        const int ButtonSpacing = ScaleForDPI(5, DpiValue);
        int CurrentY = MarginSize;
        CreateWindowExW(WS_EX_TRANSPARENT, L"STATIC", L"SQL 命令 (按 F5 执行):", WS_CHILD | WS_VISIBLE, MarginSize, CurrentY, ClientWidth - MarginSize * 2, LabelHeight, ParentWindow, nullptr, nullptr, nullptr);
        CurrentY += LabelHeight + ScaleForDPI(5, DpiValue);
        UIHandles::InputEdit = CreateRichEditControl(ParentWindow, MarginSize, CurrentY, ClientWidth - MarginSize * 2, InputHeight, ES_WANTRETURN, false);
        if (!UIHandles::InputEdit) [[unlikely]]
        {
            MessageBoxW(ParentWindow, L"创建输入框失败", L"错误", MB_OK | MB_ICONERROR);
            return;
        }
        CurrentY += InputHeight + MarginSize;
        int ButtonX = MarginSize;
        const std::array<std::pair<std::wstring_view, int>, 5> ButtonConfigs = 
        { 
            {
                {L"执行 (F5)", 1001},
                {L"清空输入", 1002},
                {L"清空输出", 1003},
                {L"连接", 1004},
                {L"断开", 1005}
            } 
        };
        for (const auto& [ButtonText, ButtonID] : ButtonConfigs)
        {
            CreateWindowExW(0, L"BUTTON", ButtonText.data(), WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, ButtonX, CurrentY, ButtonWidth, ButtonHeight, ParentWindow, reinterpret_cast<HMENU>(static_cast<UINT_PTR>(ButtonID)), nullptr, nullptr);
            ButtonX += ButtonWidth + ButtonSpacing;
        }
        CurrentY += ButtonHeight + MarginSize;
        CreateWindowExW(WS_EX_TRANSPARENT, L"STATIC", L"", WS_CHILD | WS_VISIBLE | SS_ETCHEDHORZ, MarginSize, CurrentY, ClientWidth - MarginSize * 2, 2, ParentWindow, nullptr, nullptr, nullptr);
        CurrentY += ScaleForDPI(8, DpiValue);
        CreateWindowExW(WS_EX_TRANSPARENT, L"STATIC", L"输出结果:", WS_CHILD | WS_VISIBLE, MarginSize, CurrentY, ClientWidth - MarginSize * 2, LabelHeight, ParentWindow, nullptr, nullptr, nullptr);
        CurrentY += LabelHeight + ScaleForDPI(5, DpiValue);
        const int OutputHeight = ClientHeight - CurrentY - StatusHeight - MarginSize * 2;
        UIHandles::OutputEdit = CreateRichEditControl(ParentWindow, MarginSize, CurrentY, ClientWidth - MarginSize * 2, OutputHeight, WS_VSCROLL | WS_HSCROLL | ES_AUTOHSCROLL, true);
        if (!UIHandles::OutputEdit) [[unlikely]]
        {
            MessageBoxW(ParentWindow, L"创建输出框失败", L"错误", MB_OK | MB_ICONERROR);
            return;
        }
        CurrentY += OutputHeight + MarginSize;
        UIHandles::StatusText = CreateWindowExW(0, L"STATIC", L"MySQL 状态: 未连接", WS_CHILD | WS_VISIBLE | SS_SIMPLE, MarginSize, CurrentY, ClientWidth - MarginSize * 2, StatusHeight, ParentWindow, reinterpret_cast<HMENU>(9999), nullptr, nullptr);
        UpdateAllFonts(ParentWindow, DpiValue);
    }
    catch (const std::exception& Exception)
    {
        OutputDebugStringA(std::format("CreateUIControls 异常: {}\n", Exception.what()).c_str());
        MessageBoxA(ParentWindow, std::format("UI 创建失败:\n{}", Exception.what()).c_str(), "错误", MB_OK | MB_ICONERROR);
    }
}

inline auto LayoutUIControls(HWND WindowHandle) noexcept -> void
{
    try
    {
        if (!WindowHandle || !IsWindow(WindowHandle)) [[unlikely]]
            return;
        const int DpiValue = GetWindowDPI(WindowHandle);
        RECT ClientRect;
        GetClientRect(WindowHandle, &ClientRect);
        const int ClientWidth = ClientRect.right - ClientRect.left;
        const int ClientHeight = ClientRect.bottom - ClientRect.top;
        const int MarginSize = ScaleForDPI(10, DpiValue);
        const int ButtonWidth = ScaleForDPI(100, DpiValue);
        const int ButtonHeight = ScaleForDPI(30, DpiValue);
        const int InputHeight = ScaleForDPI(150, DpiValue);
        const int StatusHeight = ScaleForDPI(25, DpiValue);
        const int LabelHeight = ScaleForDPI(20, DpiValue);
        const int ButtonSpacing = ScaleForDPI(5, DpiValue);
        int CurrentY = MarginSize;
        HWND CurrentControl = GetWindow(WindowHandle, GW_CHILD);
        while (CurrentControl)
        {
            wchar_t ClassName[256] = {};
            GetClassNameW(CurrentControl, ClassName, _countof(ClassName));
            const int TextLength = GetWindowTextLengthW(CurrentControl);
            if (TextLength > 0)
            {
                std::wstring WindowText(static_cast<std::size_t>(TextLength) + 1, L'\0');
                GetWindowTextW(CurrentControl, WindowText.data(), TextLength + 1);
                if (wcscmp(ClassName, L"Static") == 0 && wcsstr(WindowText.c_str(), L"SQL 命令"))
                {
                    SetWindowPos(CurrentControl, nullptr, MarginSize, CurrentY, ClientWidth - MarginSize * 2, LabelHeight, SWP_NOZORDER);
                    break;
                }
            }
            CurrentControl = GetWindow(CurrentControl, GW_HWNDNEXT);
        }
        CurrentY += LabelHeight + ScaleForDPI(5, DpiValue);
        if (UIHandles::InputEdit && IsWindow(UIHandles::InputEdit))
            SetWindowPos(UIHandles::InputEdit, nullptr, MarginSize, CurrentY, ClientWidth - MarginSize * 2, InputHeight, SWP_NOZORDER);
        CurrentY += InputHeight + MarginSize;
        const int ButtonY = CurrentY;
        int ButtonX = MarginSize;
        CurrentControl = GetWindow(WindowHandle, GW_CHILD);
        while (CurrentControl)
        {
            wchar_t ClassName[256] = {};
            GetClassNameW(CurrentControl, ClassName, _countof(ClassName));
            if (wcscmp(ClassName, L"Button") == 0)
            {
                SetWindowPos(CurrentControl, nullptr, ButtonX, ButtonY, ButtonWidth, ButtonHeight, SWP_NOZORDER);
                ButtonX += ButtonWidth + ButtonSpacing;
            }
            CurrentControl = GetWindow(CurrentControl, GW_HWNDNEXT);
        }
        CurrentY += ButtonHeight + MarginSize;
        CurrentControl = GetWindow(WindowHandle, GW_CHILD);
        while (CurrentControl)
        {
            wchar_t ClassName[256] = {};
            GetClassNameW(CurrentControl, ClassName, _countof(ClassName));
            if (wcscmp(ClassName, L"Static") == 0)
            {
                const LONG WindowStyle = GetWindowLongW(CurrentControl, GWL_STYLE);
                if (WindowStyle & SS_ETCHEDHORZ)
                {
                    SetWindowPos(CurrentControl, nullptr, MarginSize, CurrentY, ClientWidth - MarginSize * 2, 2, SWP_NOZORDER);
                    break;
                }
            }
            CurrentControl = GetWindow(CurrentControl, GW_HWNDNEXT);
        }
        CurrentY += ScaleForDPI(8, DpiValue);
        CurrentControl = GetWindow(WindowHandle, GW_CHILD);
        while (CurrentControl)
        {
            wchar_t ClassName[256] = {};
            GetClassNameW(CurrentControl, ClassName, _countof(ClassName));
            const int TextLength = GetWindowTextLengthW(CurrentControl);
            if (TextLength > 0)
            {
                std::wstring WindowText(static_cast<std::size_t>(TextLength) + 1, L'\0');
                GetWindowTextW(CurrentControl, WindowText.data(), TextLength + 1);
                if (wcscmp(ClassName, L"Static") == 0 && wcsstr(WindowText.c_str(), L"输出结果:"))
                {
                    SetWindowPos(CurrentControl, nullptr, MarginSize, CurrentY, ClientWidth - MarginSize * 2, LabelHeight, SWP_NOZORDER);
                    break;
                }
            }
            CurrentControl = GetWindow(CurrentControl, GW_HWNDNEXT);
        }
        CurrentY += LabelHeight + ScaleForDPI(5, DpiValue);
        if (UIHandles::OutputEdit && IsWindow(UIHandles::OutputEdit))
        {
            const int OutputHeight = ClientHeight - CurrentY - StatusHeight - MarginSize * 2 - ScaleForDPI(5, DpiValue);
            if (OutputHeight > 50)
                SetWindowPos(UIHandles::OutputEdit, nullptr, MarginSize, CurrentY, ClientWidth - MarginSize * 2, OutputHeight, SWP_NOZORDER);
        }
        if (UIHandles::StatusText && IsWindow(UIHandles::StatusText))
        {
            const int StatusY = ClientHeight - StatusHeight - MarginSize;
            SetWindowPos(UIHandles::StatusText, nullptr, MarginSize, StatusY, ClientWidth - MarginSize * 2, StatusHeight, SWP_NOZORDER | SWP_NOCOPYBITS);
            InvalidateRect(UIHandles::StatusText, nullptr, TRUE);
            UpdateWindow(UIHandles::StatusText);
        }
        if (DpiValue != RenderState::CurrentDPI)
            UpdateAllFonts(WindowHandle, DpiValue);
        InvalidateRect(WindowHandle, nullptr, TRUE);
        UpdateWindow(WindowHandle);
    }
    catch (const std::exception& Exception)
    {
        OutputDebugStringA(std::format("LayoutUIControls 异常: {}\n", Exception.what()).c_str());
    }
    catch (...)
    {
        OutputDebugStringA("LayoutUIControls: 未知异常\n");
    }
}

struct ConnectionDialogData
{
    std::function<void(const char*, const char*, const char*, const char*, int)> OnConnect;
};

inline LRESULT CALLBACK ConnectionDialogWndProc(HWND DialogHandle, UINT Message, WPARAM WParam, LPARAM LParam)
{
    static ConnectionDialogData* DialogData = nullptr;
    switch (Message)
    {
    case WM_CREATE:
    {
        CREATESTRUCT* CreateStruct = reinterpret_cast<CREATESTRUCT*>(LParam);
        DialogData = static_cast<ConnectionDialogData*>(CreateStruct->lpCreateParams);
        const int DPI = GetWindowDPI(DialogHandle);
        const int LabelWidth = ScaleForDPI(100, DPI);
        const int InputWidth = ScaleForDPI(250, DPI);
        const int StartY = ScaleForDPI(20, DPI);
        const int Spacing = ScaleForDPI(40, DPI);
        const int LabelHeight = ScaleForDPI(20, DPI);
        const int InputHeight = ScaleForDPI(25, DPI);
        const int ButtonWidth = ScaleForDPI(120, DPI);
        const int ButtonHeight = ScaleForDPI(35, DPI);
        CreateWindowExW(WS_EX_TRANSPARENT, L"STATIC", L"主机地址:", WS_CHILD | WS_VISIBLE, ScaleForDPI(20, DPI), StartY, LabelWidth, LabelHeight, DialogHandle, nullptr, nullptr, nullptr);
        UIHandles::HostEdit = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"localhost", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, ScaleForDPI(130, DPI), StartY, InputWidth, InputHeight, DialogHandle, nullptr, nullptr, nullptr);
        CreateWindowExW(WS_EX_TRANSPARENT, L"STATIC", L"端口:", WS_CHILD | WS_VISIBLE, ScaleForDPI(20, DPI), StartY + Spacing, LabelWidth, LabelHeight, DialogHandle, nullptr, nullptr, nullptr);
        UIHandles::PortEdit = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"3306", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_NUMBER, ScaleForDPI(130, DPI), StartY + Spacing, InputWidth, InputHeight, DialogHandle, nullptr, nullptr, nullptr);
        CreateWindowExW(WS_EX_TRANSPARENT, L"STATIC", L"用户名:", WS_CHILD | WS_VISIBLE, ScaleForDPI(20, DPI), StartY + Spacing * 2, LabelWidth, LabelHeight, DialogHandle, nullptr, nullptr, nullptr);
        UIHandles::UserEdit = CreateWindowExW( WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"root", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, ScaleForDPI(130, DPI), StartY + Spacing * 2, InputWidth, InputHeight, DialogHandle, nullptr, nullptr, nullptr);
        CreateWindowExW(WS_EX_TRANSPARENT, L"STATIC", L"密码:", WS_CHILD | WS_VISIBLE, ScaleForDPI(20, DPI), StartY + Spacing * 3, LabelWidth, LabelHeight, DialogHandle, nullptr, nullptr, nullptr);
        UIHandles::PasswordEdit = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL | ES_PASSWORD, ScaleForDPI(130, DPI), StartY + Spacing * 3, InputWidth, InputHeight, DialogHandle, nullptr, nullptr, nullptr);
        CreateWindowExW(WS_EX_TRANSPARENT, L"STATIC", L"数据库(可选):", WS_CHILD | WS_VISIBLE, ScaleForDPI(20, DPI), StartY + Spacing * 4, LabelWidth, LabelHeight, DialogHandle, nullptr, nullptr, nullptr);
        UIHandles::DatabaseEdit = CreateWindowExW(WS_EX_CLIENTEDGE, MSFTEDIT_CLASS, L"", WS_CHILD | WS_VISIBLE | WS_BORDER | ES_AUTOHSCROLL, ScaleForDPI(130, DPI), StartY + Spacing * 4, InputWidth, InputHeight, DialogHandle, nullptr, nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"连接", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, ScaleForDPI(80, DPI), StartY + Spacing * 5 + ScaleForDPI(10, DPI), ButtonWidth, ButtonHeight, DialogHandle, reinterpret_cast<HMENU>(IDOK), nullptr, nullptr);
        CreateWindowExW(0, L"BUTTON", L"取消", WS_CHILD | WS_VISIBLE | BS_PUSHBUTTON, ScaleForDPI(220, DPI), StartY + Spacing * 5 + ScaleForDPI(10, DPI), ButtonWidth, ButtonHeight, DialogHandle, reinterpret_cast<HMENU>(IDCANCEL), nullptr, nullptr);
        HFONT DialogFont = CreateScaledFont(DPI);
        EnumChildWindows(DialogHandle, [](HWND Child, LPARAM LParam) -> BOOL 
            {
                wchar_t ClassName[256] = {};
                GetClassNameW(Child, ClassName, _countof(ClassName));

                if (wcscmp(ClassName, MSFTEDIT_CLASS) == 0)
                    SetRichEditFont(Child, reinterpret_cast<HFONT>(LParam));
                else
                    SendMessageW(Child, WM_SETFONT, static_cast<WPARAM>(LParam), TRUE);
                return TRUE;
            }, reinterpret_cast<LPARAM>(DialogFont));
        return 0;
    }
    case WM_CTLCOLORSTATIC:
    {
        HDC hdc = reinterpret_cast<HDC>(WParam);
        SetBkMode(hdc, TRANSPARENT);
        return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
    }
    case WM_COMMAND:
    {
        if (LOWORD(WParam) == IDOK && DialogData)
        {
            std::wstring HostWide(256, L'\0');
            std::wstring UserWide(256, L'\0');
            std::wstring PasswordWide(256, L'\0');
            std::wstring DatabaseWide(256, L'\0');
            wchar_t PortBuffer[10]{};
            GetWindowTextW(UIHandles::HostEdit, HostWide.data(), 256);
            GetWindowTextW(UIHandles::UserEdit, UserWide.data(), 256);
            GetWindowTextW(UIHandles::PasswordEdit, PasswordWide.data(), 256);
            GetWindowTextW(UIHandles::DatabaseEdit, DatabaseWide.data(), 256);
            GetWindowTextW(UIHandles::PortEdit, PortBuffer, 10);
            HostWide.resize(wcslen(HostWide.c_str()));
            UserWide.resize(wcslen(UserWide.c_str()));
            PasswordWide.resize(wcslen(PasswordWide.c_str()));
            DatabaseWide.resize(wcslen(DatabaseWide.c_str()));
            const std::string HostUtf8 = WideToUtf8(HostWide);
            const std::string UserUtf8 = WideToUtf8(UserWide);
            const std::string PasswordUtf8 = WideToUtf8(PasswordWide);
            const std::string DatabaseUtf8 = WideToUtf8(DatabaseWide);
            try
            {
                const int Port = std::stoi(PortBuffer);
                if (Port < 1 || Port > 65535)
                {
                    MessageBoxW(DialogHandle, L"端口号必须在 1-65535 之间", L"输入错误", MB_OK | MB_ICONERROR);
                    return 0;
                }
                if (DialogData->OnConnect)
                    DialogData->OnConnect(HostUtf8.c_str(), UserUtf8.c_str(), PasswordUtf8.c_str(), DatabaseUtf8.c_str(), Port);
            }
            catch (const std::exception&)
            {
                MessageBoxW(DialogHandle, L"端口号格式无效", L"输入错误", MB_OK | MB_ICONERROR);
                return 0;
            }
            DestroyWindow(DialogHandle);
            return 0;
        }
        else if (LOWORD(WParam) == IDCANCEL)
        {
            DestroyWindow(DialogHandle);
            return 0;
        }
        break;
    }
    case WM_CLOSE:
    {
        DestroyWindow(DialogHandle);
        return 0;
    }
    case WM_DESTROY:
    {
        HFONT DialogFont = reinterpret_cast<HFONT>(SendMessageW(DialogHandle, WM_GETFONT, 0, 0));
        if (DialogFont)
            DeleteObject(DialogFont);
        UIHandles::ConnectionDialog = nullptr;
        DialogData = nullptr;
        return 0;
    }
    }
    return DefWindowProcW(DialogHandle, Message, WParam, LParam);
}

inline auto ShowConnectionDialog(HWND ParentWindow, std::function<void(const char*, const char*, const char*, const char*, int)> OnConnect) -> void
{
    if (UIHandles::ConnectionDialog && IsWindow(UIHandles::ConnectionDialog))
    {
        SetForegroundWindow(UIHandles::ConnectionDialog);
        return;
    }
    static bool DialogClassRegistered = false;
    if (!DialogClassRegistered)
    {
        WNDCLASSEXW DialogClass{};
        DialogClass.cbSize = sizeof(WNDCLASSEXW);
        DialogClass.style = CS_HREDRAW | CS_VREDRAW;
        DialogClass.lpfnWndProc = ConnectionDialogWndProc;
        DialogClass.hInstance = GetModuleHandleW(nullptr);
        DialogClass.hCursor = LoadCursorW(nullptr, IDC_ARROW);
        DialogClass.hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1);
        DialogClass.lpszClassName = L"MySQLConnectionDialogClass";
        RegisterClassExW(&DialogClass);
        DialogClassRegistered = true;
    }
    static ConnectionDialogData DialogData;
    DialogData.OnConnect = OnConnect;
    const int DPI = GetWindowDPI(ParentWindow);
    const int DialogWidth = ScaleForDPI(420, DPI);
    const int DialogHeight = ScaleForDPI(350, DPI);
    const int ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
    const int ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
    const int DialogX = (ScreenWidth - DialogWidth) / 2;
    const int DialogY = (ScreenHeight - DialogHeight) / 2;
    UIHandles::ConnectionDialog = CreateWindowExW( WS_EX_DLGMODALFRAME | WS_EX_TOPMOST, L"MySQLConnectionDialogClass", L"MySQL 连接设置", WS_POPUP | WS_CAPTION | WS_SYSMENU | WS_VISIBLE, DialogX, DialogY, DialogWidth, DialogHeight, ParentWindow, nullptr, GetModuleHandleW(nullptr), &DialogData);
    if (UIHandles::ConnectionDialog)
    {
        EnableWindow(ParentWindow, FALSE);
        MSG Message;
        while (IsWindow(UIHandles::ConnectionDialog) && GetMessageW(&Message, nullptr, 0, 0))
        {
            TranslateMessage(&Message);
            DispatchMessageW(&Message);
        }
        EnableWindow(ParentWindow, TRUE);
        SetForegroundWindow(ParentWindow);
    }
}