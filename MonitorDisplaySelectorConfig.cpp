#include <Windows.h>

#include <algorithm>
#include <filesystem>
#include <optional>
#include <string>
#include <vector>


namespace
{
    constexpr wchar_t kMainWindowClass[] =
        L"MonitorDisplaySelectorConfigWindow";

    constexpr wchar_t kIdentifyWindowClass[] =
        L"MonitorDisplaySelectorIdentifyWindow";

    constexpr wchar_t kWindowTitle[] =
        L"Monitor Display Selector";

    constexpr int ID_MONITOR_LIST = 1001;
    constexpr int ID_IDENTIFY = 1002;
    constexpr int ID_SAVE = 1003;
    constexpr int ID_STATUS = 1004;


    struct MonitorData
    {
        HMONITOR handle{};
        RECT rect{};
        std::wstring device;
        bool primary = false;
    };


    struct SavedMonitor
    {
        int x{};
        int y{};
        int width{};
        int height{};
    };


    std::vector<MonitorData> g_monitors;

    HWND g_monitorList = nullptr;
    HWND g_status = nullptr;


    // ============================================================================
    // GENERAL HELPERS
    // ============================================================================

    HMENU ControlID(int id)
    {
        return reinterpret_cast<HMENU>(
            static_cast<INT_PTR>(id)
            );
    }


    std::filesystem::path GetExecutableDirectory()
    {
        // Use a large buffer rather than MAX_PATH so the configurator
        // continues to work when installed in a long mod-manager path.

        std::wstring buffer(
            32768,
            L'\0'
        );

        const DWORD length =
            GetModuleFileNameW(
                nullptr,
                buffer.data(),
                static_cast<DWORD>(
                    buffer.size()
                    )
            );

        if (length == 0 ||
            length >= buffer.size()) {

            return {};
        }

        buffer.resize(length);

        return std::filesystem::path(
            buffer
        ).parent_path();
    }


    std::filesystem::path GetConfigPath()
    {
        const std::filesystem::path directory =
            GetExecutableDirectory();

        if (directory.empty()) {
            return {};
        }

        return directory /
            L"MonitorDisplaySelector.ini";
    }


    int MonitorWidth(
        const MonitorData& monitor)
    {
        return monitor.rect.right -
            monitor.rect.left;
    }


    int MonitorHeight(
        const MonitorData& monitor)
    {
        return monitor.rect.bottom -
            monitor.rect.top;
    }


    // ============================================================================
    // MONITOR ENUMERATION
    // ============================================================================

    BOOL CALLBACK MonitorEnumProc(
        HMONITOR monitor,
        HDC,
        LPRECT,
        LPARAM)
    {
        MONITORINFOEXW info{};
        info.cbSize =
            sizeof(info);

        if (!GetMonitorInfoW(
            monitor,
            reinterpret_cast<MONITORINFO*>(
                &info
                ))) {

            return TRUE;
        }


        MonitorData data{};

        data.handle =
            monitor;

        data.rect =
            info.rcMonitor;

        data.device =
            info.szDevice;

        data.primary =
            (info.dwFlags &
                MONITORINFOF_PRIMARY) != 0;


        g_monitors.push_back(
            data
        );

        return TRUE;
    }


    void EnumerateMonitors()
    {
        g_monitors.clear();


        EnumDisplayMonitors(
            nullptr,
            nullptr,
            MonitorEnumProc,
            0
        );


        // Keep numbering predictable by sorting monitors
        // according to their physical desktop arrangement:
        //
        // top-to-bottom, then left-to-right.

        std::sort(
            g_monitors.begin(),
            g_monitors.end(),
            [](const MonitorData& a,
                const MonitorData& b)
            {
                if (a.rect.top !=
                    b.rect.top) {

                    return a.rect.top <
                        b.rect.top;
                }

                return a.rect.left <
                    b.rect.left;
            }
        );
    }


    // ============================================================================
    // SAVED CONFIGURATION
    // ============================================================================

    std::optional<SavedMonitor>
        LoadSavedMonitor()
    {
        const std::filesystem::path path =
            GetConfigPath();


        if (path.empty()) {
            return std::nullopt;
        }


        if (!std::filesystem::exists(
            path)) {

            return std::nullopt;
        }


        const std::wstring ini =
            path.wstring();


        constexpr int missing =
            0x7FFFFFFF;


        SavedMonitor saved{};


        saved.x =
            GetPrivateProfileIntW(
                L"Monitor",
                L"X",
                missing,
                ini.c_str()
            );


        saved.y =
            GetPrivateProfileIntW(
                L"Monitor",
                L"Y",
                missing,
                ini.c_str()
            );


        saved.width =
            GetPrivateProfileIntW(
                L"Monitor",
                L"Width",
                missing,
                ini.c_str()
            );


        saved.height =
            GetPrivateProfileIntW(
                L"Monitor",
                L"Height",
                missing,
                ini.c_str()
            );


        if (saved.x == missing ||
            saved.y == missing ||
            saved.width == missing ||
            saved.height == missing) {

            return std::nullopt;
        }


        if (saved.width <= 0 ||
            saved.height <= 0) {

            return std::nullopt;
        }


        return saved;
    }


    std::optional<std::size_t>
        FindSavedMonitorIndex()
    {
        const auto saved =
            LoadSavedMonitor();


        if (!saved) {
            return std::nullopt;
        }


        for (std::size_t i = 0;
            i < g_monitors.size();
            ++i) {

            const MonitorData& monitor =
                g_monitors[i];


            if (monitor.rect.left ==
                saved->x &&
                monitor.rect.top ==
                saved->y &&
                MonitorWidth(monitor) ==
                saved->width &&
                MonitorHeight(monitor) ==
                saved->height) {

                return i;
            }
        }


        return std::nullopt;
    }


    // ============================================================================
    // MONITOR DROPDOWN
    // ============================================================================

    std::wstring BuildMonitorLabel(
        std::size_t index,
        const MonitorData& monitor)
    {
        std::wstring text =
            L"Monitor " +
            std::to_wstring(
                index + 1
            ) +
            L"  -  " +
            std::to_wstring(
                MonitorWidth(monitor)
            ) +
            L" x " +
            std::to_wstring(
                MonitorHeight(monitor)
            );


        if (monitor.primary) {

            text +=
                L"  -  Primary";
        }


        text +=
            L"  -  Position (" +
            std::to_wstring(
                monitor.rect.left
            ) +
            L", " +
            std::to_wstring(
                monitor.rect.top
            ) +
            L")";


        return text;
    }


    void PopulateMonitorList()
    {
        SendMessageW(
            g_monitorList,
            CB_RESETCONTENT,
            0,
            0
        );


        for (std::size_t i = 0;
            i < g_monitors.size();
            ++i) {

            const std::wstring label =
                BuildMonitorLabel(
                    i,
                    g_monitors[i]
                );


            SendMessageW(
                g_monitorList,
                CB_ADDSTRING,
                0,
                reinterpret_cast<LPARAM>(
                    label.c_str()
                    )
            );
        }


        if (g_monitors.empty()) {

            SetWindowTextW(
                g_status,
                L"No displays were detected."
            );

            return;
        }


        // If an existing configuration matches a currently
        // connected monitor, automatically select it.

        const auto savedIndex =
            FindSavedMonitorIndex();


        if (savedIndex) {

            SendMessageW(
                g_monitorList,
                CB_SETCURSEL,
                static_cast<WPARAM>(
                    *savedIndex
                    ),
                0
            );


            SetWindowTextW(
                g_status,
                L"Current saved display detected."
            );


            return;
        }


        // No valid configuration exists.
        //
        // Visually select the Windows primary monitor as a
        // convenient starting point, but DO NOT save it.
        //
        // Nothing is written until the user explicitly presses
        // Save Selection.

        std::size_t primaryIndex = 0;


        for (std::size_t i = 0;
            i < g_monitors.size();
            ++i) {

            if (g_monitors[i].primary) {

                primaryIndex = i;

                break;
            }
        }


        SendMessageW(
            g_monitorList,
            CB_SETCURSEL,
            static_cast<WPARAM>(
                primaryIndex
                ),
            0
        );


        SetWindowTextW(
            g_status,
            L"Choose a display and click Save."
        );
    }


    // ============================================================================
    // IDENTIFY DISPLAY WINDOWS
    // ============================================================================

    LRESULT CALLBACK IdentifyWindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            auto* create =
                reinterpret_cast<
                CREATESTRUCTW*
                >(lParam);


            SetWindowLongPtrW(
                hwnd,
                GWLP_USERDATA,
                reinterpret_cast<
                LONG_PTR
                >(
                    create->lpCreateParams
                    )
            );


            // Automatically close the display number
            // after 2.5 seconds.

            SetTimer(
                hwnd,
                1,
                2500,
                nullptr
            );


            return 0;
        }


        case WM_TIMER:
        {
            KillTimer(
                hwnd,
                1
            );


            DestroyWindow(
                hwnd
            );


            return 0;
        }


        case WM_PAINT:
        {
            PAINTSTRUCT ps{};


            HDC dc =
                BeginPaint(
                    hwnd,
                    &ps
                );


            RECT client{};

            GetClientRect(
                hwnd,
                &client
            );


            HBRUSH background =
                CreateSolidBrush(
                    RGB(
                        20,
                        20,
                        20
                    )
                );


            FillRect(
                dc,
                &client,
                background
            );


            DeleteObject(
                background
            );


            SetBkMode(
                dc,
                TRANSPARENT
            );


            SetTextColor(
                dc,
                RGB(
                    255,
                    255,
                    255
                )
            );


            const auto index =
                static_cast<std::size_t>(
                    GetWindowLongPtrW(
                        hwnd,
                        GWLP_USERDATA
                    )
                    );


            const std::wstring number =
                std::to_wstring(
                    index + 1
                );


            HFONT font =
                CreateFontW(
                    220,
                    0,
                    0,
                    0,
                    FW_BOLD,
                    FALSE,
                    FALSE,
                    FALSE,
                    DEFAULT_CHARSET,
                    OUT_DEFAULT_PRECIS,
                    CLIP_DEFAULT_PRECIS,
                    CLEARTYPE_QUALITY,
                    DEFAULT_PITCH,
                    L"Segoe UI"
                );


            HFONT oldFont =
                static_cast<HFONT>(
                    SelectObject(
                        dc,
                        font
                    )
                    );


            DrawTextW(
                dc,
                number.c_str(),
                -1,
                &client,
                DT_CENTER |
                DT_VCENTER |
                DT_SINGLELINE
            );


            SelectObject(
                dc,
                oldFont
            );


            DeleteObject(
                font
            );


            EndPaint(
                hwnd,
                &ps
            );


            return 0;
        }
        }


        return DefWindowProcW(
            hwnd,
            message,
            wParam,
            lParam
        );
    }


    // ============================================================================
    // IDENTIFY DISPLAYS
    // ============================================================================

    void IdentifyDisplays(
        HINSTANCE instance)
    {
        for (std::size_t i = 0;
            i < g_monitors.size();
            ++i) {

            const MonitorData& monitor =
                g_monitors[i];


            HWND overlay =
                CreateWindowExW(
                    WS_EX_TOPMOST |
                    WS_EX_TOOLWINDOW,
                    kIdentifyWindowClass,
                    L"",
                    WS_POPUP,
                    monitor.rect.left,
                    monitor.rect.top,
                    MonitorWidth(monitor),
                    MonitorHeight(monitor),
                    nullptr,
                    nullptr,
                    instance,
                    reinterpret_cast<void*>(
                        i
                        )
                );


            if (overlay) {

                ShowWindow(
                    overlay,
                    SW_SHOW
                );


                UpdateWindow(
                    overlay
                );
            }
        }
    }


    // ============================================================================
    // CONFIGURATION WRITING
    // ============================================================================

    bool WriteValue(
        const wchar_t* key,
        int value,
        const std::wstring& path)
    {
        const std::wstring text =
            std::to_wstring(
                value
            );


        return
            WritePrivateProfileStringW(
                L"Monitor",
                key,
                text.c_str(),
                path.c_str()
            ) != FALSE;
    }


    bool SaveSelectedMonitor()
    {
        const LRESULT selection =
            SendMessageW(
                g_monitorList,
                CB_GETCURSEL,
                0,
                0
            );


        if (selection == CB_ERR) {
            return false;
        }


        const std::size_t index =
            static_cast<std::size_t>(
                selection
                );


        if (index >=
            g_monitors.size()) {

            return false;
        }


        const MonitorData& monitor =
            g_monitors[index];


        const std::filesystem::path configPath =
            GetConfigPath();


        if (configPath.empty()) {
            return false;
        }


        const std::wstring path =
            configPath.wstring();


        bool success = true;


        success &=
            WriteValue(
                L"X",
                monitor.rect.left,
                path
            );


        success &=
            WriteValue(
                L"Y",
                monitor.rect.top,
                path
            );


        success &=
            WriteValue(
                L"Width",
                MonitorWidth(monitor),
                path
            );


        success &=
            WriteValue(
                L"Height",
                MonitorHeight(monitor),
                path
            );


        return success;
    }


    // ============================================================================
    // MAIN WINDOW
    // ============================================================================

    LRESULT CALLBACK MainWindowProc(
        HWND hwnd,
        UINT message,
        WPARAM wParam,
        LPARAM lParam)
    {
        switch (message)
        {
        case WM_CREATE:
        {
            HINSTANCE instance =
                reinterpret_cast<HINSTANCE>(
                    GetWindowLongPtrW(
                        hwnd,
                        GWLP_HINSTANCE
                    )
                    );


            CreateWindowW(
                L"STATIC",
                L"Select the display Skyrim should use:",
                WS_CHILD |
                WS_VISIBLE,
                24,
                22,
                550,
                24,
                hwnd,
                nullptr,
                instance,
                nullptr
            );


            g_monitorList =
                CreateWindowW(
                    L"COMBOBOX",
                    L"",
                    WS_CHILD |
                    WS_VISIBLE |
                    WS_TABSTOP |
                    CBS_DROPDOWNLIST |
                    WS_VSCROLL,
                    24,
                    55,
                    550,
                    300,
                    hwnd,
                    ControlID(
                        ID_MONITOR_LIST
                    ),
                    instance,
                    nullptr
                );


            CreateWindowW(
                L"BUTTON",
                L"Identify Displays",
                WS_CHILD |
                WS_VISIBLE |
                WS_TABSTOP |
                BS_PUSHBUTTON,
                24,
                108,
                260,
                38,
                hwnd,
                ControlID(
                    ID_IDENTIFY
                ),
                instance,
                nullptr
            );


            CreateWindowW(
                L"BUTTON",
                L"Save Selection",
                WS_CHILD |
                WS_VISIBLE |
                WS_TABSTOP |
                BS_DEFPUSHBUTTON,
                314,
                108,
                260,
                38,
                hwnd,
                ControlID(
                    ID_SAVE
                ),
                instance,
                nullptr
            );


            g_status =
                CreateWindowW(
                    L"STATIC",
                    L"",
                    WS_CHILD |
                    WS_VISIBLE,
                    24,
                    168,
                    550,
                    40,
                    hwnd,
                    ControlID(
                        ID_STATUS
                    ),
                    instance,
                    nullptr
                );


            EnumerateMonitors();

            PopulateMonitorList();


            return 0;
        }


        case WM_COMMAND:
        {
            const int command =
                LOWORD(
                    wParam
                );


            if (command ==
                ID_IDENTIFY) {

                HINSTANCE instance =
                    reinterpret_cast<HINSTANCE>(
                        GetWindowLongPtrW(
                            hwnd,
                            GWLP_HINSTANCE
                        )
                        );


                IdentifyDisplays(
                    instance
                );


                return 0;
            }


            if (command ==
                ID_SAVE) {

                if (SaveSelectedMonitor()) {

                    SetWindowTextW(
                        g_status,
                        L"Selection saved successfully."
                    );


                    const std::filesystem::path configPath =
                        GetConfigPath();


                    std::wstring messageText =
                        L"Your monitor selection has been saved.\n\n"
                        L"Skyrim will use this display the next time it starts.";


                    if (!configPath.empty()) {

                        messageText +=
                            L"\n\nConfiguration:\n";

                        messageText +=
                            configPath.wstring();
                    }


                    MessageBoxW(
                        hwnd,
                        messageText.c_str(),
                        L"Monitor Display Selector",
                        MB_OK |
                        MB_ICONINFORMATION
                    );
                }
                else {

                    const std::filesystem::path configPath =
                        GetConfigPath();


                    std::wstring messageText =
                        L"The monitor selection could not be saved.";


                    if (!configPath.empty()) {

                        messageText +=
                            L"\n\nAttempted configuration path:\n";

                        messageText +=
                            configPath.wstring();
                    }


                    MessageBoxW(
                        hwnd,
                        messageText.c_str(),
                        L"Monitor Display Selector",
                        MB_OK |
                        MB_ICONERROR
                    );
                }


                return 0;
            }


            break;
        }


        case WM_CLOSE:
        {
            DestroyWindow(
                hwnd
            );

            return 0;
        }


        case WM_DESTROY:
        {
            PostQuitMessage(
                0
            );

            return 0;
        }
        }


        return DefWindowProcW(
            hwnd,
            message,
            wParam,
            lParam
        );
    }
}


// ============================================================================
// APPLICATION ENTRY POINT
// ============================================================================

int WINAPI wWinMain(
    HINSTANCE instance,
    HINSTANCE,
    PWSTR,
    int showCommand)
{
    // ------------------------------------------------------------------------
    // Main configurator window class
    // ------------------------------------------------------------------------

    WNDCLASSEXW mainClass{};

    mainClass.cbSize =
        sizeof(mainClass);

    mainClass.lpfnWndProc =
        MainWindowProc;

    mainClass.hInstance =
        instance;

    mainClass.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW
        );

    mainClass.hbrBackground =
        reinterpret_cast<HBRUSH>(
            static_cast<INT_PTR>(
                COLOR_WINDOW + 1
                )
            );

    mainClass.lpszClassName =
        kMainWindowClass;


    if (!RegisterClassExW(
        &mainClass)) {

        return 1;
    }


    // ------------------------------------------------------------------------
    // Display-identification overlay class
    // ------------------------------------------------------------------------

    WNDCLASSEXW identifyClass{};

    identifyClass.cbSize =
        sizeof(identifyClass);

    identifyClass.lpfnWndProc =
        IdentifyWindowProc;

    identifyClass.hInstance =
        instance;

    identifyClass.hCursor =
        LoadCursorW(
            nullptr,
            IDC_ARROW
        );

    identifyClass.hbrBackground =
        reinterpret_cast<HBRUSH>(
            static_cast<INT_PTR>(
                COLOR_WINDOW + 1
                )
            );

    identifyClass.lpszClassName =
        kIdentifyWindowClass;


    if (!RegisterClassExW(
        &identifyClass)) {

        return 1;
    }


    // ------------------------------------------------------------------------
    // Create main window
    // ------------------------------------------------------------------------

    HWND hwnd =
        CreateWindowExW(
            0,
            kMainWindowClass,
            kWindowTitle,
            WS_OVERLAPPED |
            WS_CAPTION |
            WS_SYSMENU |
            WS_MINIMIZEBOX,
            CW_USEDEFAULT,
            CW_USEDEFAULT,
            620,
            270,
            nullptr,
            nullptr,
            instance,
            nullptr
        );


    if (!hwnd) {
        return 1;
    }


    ShowWindow(
        hwnd,
        showCommand
    );


    UpdateWindow(
        hwnd
    );


    // ------------------------------------------------------------------------
    // Message loop
    // ------------------------------------------------------------------------

    MSG message{};


    while (GetMessageW(
        &message,
        nullptr,
        0,
        0) > 0) {

        TranslateMessage(
            &message
        );


        DispatchMessageW(
            &message
        );
    }


    return static_cast<int>(
        message.wParam
        );
}