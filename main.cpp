#include "render.hpp"
#include "def.h"
#include "database.h"
#include <sstream>
#include <algorithm>
#include <expected>
#include <vector>
#include <ranges>
#include <eh.h>
#include <chrono>
#include <iomanip>

inline MySQLWrapper MySQLConnection;
inline bool IsMySQLConnected = false;

struct ConnectionConfig
{
    static inline std::array<char, 256> Host{ "localhost" };
    static inline std::array<char, 256> User{ "root" };
    static inline std::array<char, 256> Password{};
    static inline std::array<char, 256> Database{};
    static inline int Port = 3306;
};

[[nodiscard]] auto GetCurrentTimestamp() -> std::string
{
    const auto Now = std::chrono::system_clock::now();
    const auto TimeT = std::chrono::system_clock::to_time_t(Now);
    const auto Ms = std::chrono::duration_cast<std::chrono::milliseconds>(Now.time_since_epoch()) % 1000;
    std::tm LocalTime{};
    localtime_s(&LocalTime, &TimeT);
    std::ostringstream Oss;
    Oss << std::put_time(&LocalTime, "[%Y-%m-%d %H:%M:%S") << '.' << std::setfill('0') << std::setw(3) << Ms.count() << "]\r\n";
    return Oss.str();
}

class TableFormatter
{
private:
    std::ostringstream OutputStream;
    std::vector<std::size_t> ColumnWidths;
    auto CalculateColumnWidths(const MySQLResult& ResultData) -> void
    {
        ColumnWidths.clear();
        ColumnWidths.reserve(ResultData.ColumnNames.size());
        for (const auto& ColumnName : ResultData.ColumnNames)
            ColumnWidths.push_back(ColumnName.length());
        for (const auto& RowData : ResultData.Rows)
        {
            for (auto [Index, Width] : std::views::enumerate(ColumnWidths))
            {
                if (static_cast<std::size_t>(Index) < RowData.Fields.size())
                    Width = max(Width, RowData.Fields[Index].length());
            }
        }
    }
    auto AppendSeparator() -> void
    {
        OutputStream << "+";
        for (std::size_t Index = 0; Index < ColumnWidths.size(); ++Index)
        {
            OutputStream << std::string(ColumnWidths[Index] + 2, '-');
            if (Index < ColumnWidths.size() - 1)
                OutputStream << "+";
        }
        OutputStream << "+\r\n";
    }
    auto AppendRow(const std::vector<std::string>& FieldsData) -> void
    {
        OutputStream << "|";
        for (std::size_t Index = 0; Index < FieldsData.size() && Index < ColumnWidths.size(); ++Index)
            OutputStream << std::format(" {:<{}} |", FieldsData[Index], ColumnWidths[Index]);
        OutputStream << "\r\n";
    }
public:
    [[nodiscard]] auto Format(const MySQLResult& ResultData) -> std::string
    {
        if (!ResultData.Success)
            return std::format("错误: {}\r\n", ResultData.ErrorMessage);
        if (ResultData.ColumnNames.empty())
            return std::format("查询成功, 影响 {} 行\r\n", ResultData.AffectedRows);
        CalculateColumnWidths(ResultData);
        AppendSeparator();
        AppendRow(ResultData.ColumnNames);
        AppendSeparator();
        for (const auto& RowData : ResultData.Rows)
            AppendRow(RowData.Fields);
        AppendSeparator();
        OutputStream << std::format("共 {} 行\r\n", ResultData.Rows.size());
        return OutputStream.str();
    }
};

[[nodiscard]] auto FormatQueryResult(const MySQLResult& ResultData) -> std::string
{
    return TableFormatter{}.Format(ResultData);
}

auto UpdateStatusDisplay() -> void
{
    if (UIHandles::StatusText && IsWindow(UIHandles::StatusText))
    {
        constexpr std::wstring_view ConnectedText = L"MySQL 状态: 已连接";
        constexpr std::wstring_view DisconnectedText = L"MySQL 状态: 未连接";
        const auto StatusText = IsMySQLConnected ? ConnectedText : DisconnectedText;
        SetWindowTextW(UIHandles::StatusText, StatusText.data());
        InvalidateRect(UIHandles::StatusText, nullptr, TRUE);
        UpdateWindow(UIHandles::StatusText);
    }
}

auto ExecuteSQL() -> void
{
    const std::string InputSQL = GetEditText(UIHandles::InputEdit);
    if (!IsMySQLConnected) [[unlikely]]
    {
        const std::string OutputMessage = GetCurrentTimestamp() + "未连接到数据库，请先连接。";
        AppendEditTextWithTimestamp(UIHandles::OutputEdit, OutputMessage);
        return;
    }
    if (InputSQL.empty()) [[unlikely]]
    {
        const std::string OutputMessage = GetCurrentTimestamp() + "请输入 SQL 命令。";
        AppendEditTextWithTimestamp(UIHandles::OutputEdit, OutputMessage);
        return;
    }
    const MySQLResult ResultData = MySQLConnection.Query(InputSQL);
    const std::string FormattedResult = FormatQueryResult(ResultData);
    const std::string OutputText = GetCurrentTimestamp() + FormattedResult;
    AppendEditTextWithTimestamp(UIHandles::OutputEdit, OutputText);
}

auto HandleConnect(const char* HostPtr, const char* UserPtr, const char* PasswordPtr, const char* DatabasePtr, int PortNumber) -> void
{
    std::ranges::copy_n(HostPtr, std::min<std::size_t>(255, strlen(HostPtr)), ConnectionConfig::Host.begin());
    std::ranges::copy_n(UserPtr, std::min<std::size_t>(255, strlen(UserPtr)), ConnectionConfig::User.begin());
    std::ranges::copy_n(PasswordPtr, std::min<std::size_t>(255, strlen(PasswordPtr)), ConnectionConfig::Password.begin());
    std::ranges::copy_n(DatabasePtr, std::min<std::size_t>(255, strlen(DatabasePtr)), ConnectionConfig::Database.begin());
    ConnectionConfig::Port = PortNumber;
    const MySQLConfig ConfigData
    {
        .Host = ConnectionConfig::Host.data(),
        .User = ConnectionConfig::User.data(),
        .Password = ConnectionConfig::Password.data(),
        .Database = ConnectionConfig::Database.data(),
        .Port = static_cast<unsigned int>(ConnectionConfig::Port)
    };
    IsMySQLConnected = MySQLConnection.Connect(ConfigData);
    std::string OutputMessage = GetCurrentTimestamp();
    if (IsMySQLConnected)
    {
        OutputMessage += "成功连接到 MySQL!\r\n";
        OutputMessage += std::format("主机: {}:{}\r\n", ConfigData.Host, ConfigData.Port);
        OutputMessage += std::format("用户: {}\r\n", ConfigData.User);
        if (!std::string_view{ ConfigData.Database }.empty())
            OutputMessage += std::format("数据库: {}\r\n", ConfigData.Database);
    }
    else
        OutputMessage += std::format("连接失败:\r\n{}\r\n", MySQLConnection.GetLastError());
    AppendEditTextWithTimestamp(UIHandles::OutputEdit, OutputMessage);
    UpdateStatusDisplay();
}

auto HandleDisconnect() -> void
{
    if (IsMySQLConnected)
    {
        MySQLConnection.Disconnect();
        IsMySQLConnected = false;
        const std::string OutputMessage = GetCurrentTimestamp() + "已断开 MySQL 连接";
        AppendEditTextWithTimestamp(UIHandles::OutputEdit, OutputMessage);
        UpdateStatusDisplay();
    }
}

[[nodiscard]] LRESULT CALLBACK MainWindowProc(HWND WindowHandle, UINT Message, WPARAM WParam, LPARAM LParam) noexcept
{
    try
    {
        switch (Message)
        {
        case WM_CREATE:
        {
            CreateUIControls(WindowHandle);
            RenderState::WindowHandle = WindowHandle;
            return 0;
        }
        case WM_GETMINMAXINFO:
        {
            auto* MinMaxInfoPtr = reinterpret_cast<MINMAXINFO*>(LParam);
            const int DpiValue = GetWindowDPI(WindowHandle);
            MinMaxInfoPtr->ptMinTrackSize.x = ScaleForDPI(600, DpiValue);
            MinMaxInfoPtr->ptMinTrackSize.y = ScaleForDPI(500, DpiValue);
            return 0;
        }
        case WM_CTLCOLORSTATIC:
        {
            const HDC DeviceContext = reinterpret_cast<HDC>(WParam);
            const HWND ControlHandle = reinterpret_cast<HWND>(LParam);
            if (ControlHandle == UIHandles::StatusText)
            {
                SetBkColor(DeviceContext, RGB(255, 255, 255));
                SetTextColor(DeviceContext, RGB(0, 0, 0));
                return reinterpret_cast<INT_PTR>(GetStockObject(WHITE_BRUSH));
            }
            SetBkMode(DeviceContext, TRANSPARENT);
            return reinterpret_cast<INT_PTR>(GetStockObject(NULL_BRUSH));
        }
        case WM_COMMAND:
        {
            const int ControlID = LOWORD(WParam);
            switch (ControlID)
            {
            case 1001: ExecuteSQL(); break;
            case 1002:
            {
                ClearEditText(UIHandles::InputEdit);
                SetFocus(UIHandles::InputEdit);
                break;
            }
            case 1003: ClearEditText(UIHandles::OutputEdit); break;
            case 1004: ShowConnectionDialog(WindowHandle, HandleConnect); break;
            case 1005: HandleDisconnect(); break;
            default: break;
            }
            return 0;
        }
        case WM_KEYDOWN:
        {
            if (WParam == VK_F5)
            {
                ExecuteSQL();
                return 0;
            }
            break;
        }
        case WM_SIZE:
        {
            LayoutUIControls(WindowHandle);
            return 0;
        }
        case WM_DPICHANGED:
        {
            const int NewDPI = HIWORD(WParam);
            UpdateAllFonts(WindowHandle, NewDPI);
            const auto* SuggestedRectPtr = reinterpret_cast<const RECT*>(LParam);
            SetWindowPos(WindowHandle, nullptr, SuggestedRectPtr->left, SuggestedRectPtr->top, SuggestedRectPtr->right - SuggestedRectPtr->left, SuggestedRectPtr->bottom - SuggestedRectPtr->top, SWP_NOZORDER | SWP_NOACTIVATE);
            LayoutUIControls(WindowHandle);
            return 0;
        }
        case WM_CLOSE:
        {
            DestroyWindow(WindowHandle);
            return 0;
        }
        case WM_DESTROY:
        {
            PostQuitMessage(0);
            return 0;
        }
        default:
            break;
        }
        return DefWindowProcW(WindowHandle, Message, WParam, LParam);
    }
    catch (const std::exception& Exception)
    {
        OutputDebugStringA(std::format("MainWindowProc 异常: {}\n", Exception.what()).c_str());
        const std::string ErrorMessage = GetCurrentTimestamp() + std::format("窗口处理异常: {}", Exception.what());
        AppendEditTextWithTimestamp(UIHandles::OutputEdit, ErrorMessage);
        return DefWindowProcW(WindowHandle, Message, WParam, LParam);
    }
    catch (...)
    {
        OutputDebugStringA("MainWindowProc: 未知异常\n");
        return DefWindowProcW(WindowHandle, Message, WParam, LParam);
    }
}

void SEHTranslator(unsigned int ExceptionCode, EXCEPTION_POINTERS* ExceptionPointers)
{
    const auto ErrorMessage = std::format("结构化异常 0x{:08X} at address 0x{:p}", ExceptionCode, ExceptionPointers->ExceptionRecord->ExceptionAddress);
    throw std::runtime_error(ErrorMessage);
}

#pragma warning(push)
#pragma warning(disable: 28251)
int APIENTRY wWinMain([[maybe_unused]] _In_ HINSTANCE InstanceHandle, [[maybe_unused]] _In_opt_ HINSTANCE PreviousInstance, [[maybe_unused]] _In_ LPWSTR CommandLine, _In_ int ShowCommand)
{
#pragma warning(push)
#pragma warning(disable: 4535)
    _set_se_translator(SEHTranslator);
#pragma warning(pop)
    try
    {
        const HICON AppIcon = static_cast<HICON>(LoadImageW(InstanceHandle, MAKEINTRESOURCEW(101), IMAGE_ICON, 0, 0, LR_DEFAULTSIZE));
        WNDCLASSEXW WindowClass
        {
            .cbSize = sizeof(WNDCLASSEXW),
            .style = CS_HREDRAW | CS_VREDRAW,
            .lpfnWndProc = MainWindowProc,
            .cbClsExtra = 0,
            .cbWndExtra = 0,
            .hInstance = InstanceHandle,
            .hIcon = AppIcon,
            .hCursor = LoadCursorW(nullptr, IDC_ARROW),
            .hbrBackground = reinterpret_cast<HBRUSH>(COLOR_WINDOW + 1),
            .lpszMenuName = nullptr,
            .lpszClassName = L"MySQLClientWindowClass",
            .hIconSm = AppIcon
        };
        if (!RegisterClassExW(&WindowClass)) [[unlikely]]
        {
            MessageBoxW(nullptr, L"窗口类注册失败", L"错误", MB_ICONERROR);
            return 1;
        }
        constexpr int WindowWidth = 700;
        constexpr int WindowHeight = 900;
        const int ScreenWidth = GetSystemMetrics(SM_CXSCREEN);
        const int ScreenHeight = GetSystemMetrics(SM_CYSCREEN);
        const int PositionX = (ScreenWidth - WindowWidth) / 2;
        const int PositionY = (ScreenHeight - WindowHeight) / 2;
        constexpr DWORD WindowStyle = WS_OVERLAPPEDWINDOW;
        RenderState::WindowHandle = CreateWindowExW(0, WindowClass.lpszClassName, L"MySQL Local Client - Update 2025.12.14 by xiren xue", WindowStyle, PositionX, PositionY, WindowWidth, WindowHeight, nullptr, nullptr, InstanceHandle, nullptr);
        if (!RenderState::WindowHandle) [[unlikely]]
        {
            MessageBoxW(nullptr, L"窗口创建失败", L"错误", MB_ICONERROR);
            return 1;
        }
        ShowWindow(RenderState::WindowHandle, ShowCommand);
        UpdateWindow(RenderState::WindowHandle);
        MSG MessageData{};
        while (GetMessageW(&MessageData, nullptr, 0, 0))
        {
            TranslateMessage(&MessageData);
            DispatchMessageW(&MessageData);
        }
        if (IsMySQLConnected)
        {
            MySQLConnection.Disconnect();
            IsMySQLConnected = false;
        }
        return static_cast<int>(MessageData.wParam);
    }
    catch (const std::exception& Exception)
    {
        MessageBoxA(nullptr, std::format("程序异常:\n{}", Exception.what()).c_str(), "致命错误", MB_OK | MB_ICONERROR);
        return -1;
    }
    catch (...)
    {
        MessageBoxW(nullptr, L"发生未知异常，程序将退出", L"致命错误", MB_OK | MB_ICONERROR);
        return -2;
    }
}
#pragma warning(pop)