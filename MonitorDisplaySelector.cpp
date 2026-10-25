#include "RE/Skyrim.h"
#include "SKSE/SKSE.h"

#include <Windows.h>

#include <chrono>
#include <filesystem>
#include <optional>
#include <string_view>
#include <thread>

using namespace std::literals;


// ============================================================================
// SKSE PLUGIN INFORMATION
// ============================================================================

SKSEPluginInfo(
    .Version = { 0, 3, 0, 0 },
    .Name = "Monitor Display Selector"sv,
    .Author = "Monitor Display Selector"sv,
    .StructCompatibility = SKSE::StructCompatibility::Independent,
    .RuntimeCompatibility = SKSE::VersionIndependence::AddressLibrary
)


// ============================================================================
// CONFIGURATION
// ============================================================================

namespace
{
    struct PluginConfig
    {
        int monitorX{};
        int monitorY{};
        int monitorWidth{};
        int monitorHeight{};
    };


    std::filesystem::path GetConfigPath()
    {
        return std::filesystem::path(L"Data") /
            L"SKSE" /
            L"Plugins" /
            L"MonitorDisplaySelector.ini";
    }


    std::optional<PluginConfig> LoadConfig()
    {
        const std::filesystem::path path =
            GetConfigPath();

        if (!std::filesystem::exists(path)) {
            return std::nullopt;
        }

        const std::wstring iniPath =
            path.wstring();

        constexpr int missing =
            0x7FFFFFFF;

        PluginConfig config{};

        config.monitorX =
            GetPrivateProfileIntW(
                L"Monitor",
                L"X",
                missing,
                iniPath.c_str()
            );

        config.monitorY =
            GetPrivateProfileIntW(
                L"Monitor",
                L"Y",
                missing,
                iniPath.c_str()
            );

        config.monitorWidth =
            GetPrivateProfileIntW(
                L"Monitor",
                L"Width",
                missing,
                iniPath.c_str()
            );

        config.monitorHeight =
            GetPrivateProfileIntW(
                L"Monitor",
                L"Height",
                missing,
                iniPath.c_str()
            );

        if (config.monitorX == missing ||
            config.monitorY == missing ||
            config.monitorWidth == missing ||
            config.monitorHeight == missing) {

            return std::nullopt;
        }

        if (config.monitorWidth <= 0 ||
            config.monitorHeight <= 0) {

            return std::nullopt;
        }

        return config;
    }


    // ============================================================================
    // SKYRIM WINDOW DETECTION
    // ============================================================================

    struct WindowSearchData
    {
        DWORD processID{};
        HWND skyrimWindow{};
    };


    BOOL CALLBACK FindSkyrimWindowCallback(
        HWND hwnd,
        LPARAM lParam)
    {
        auto* data =
            reinterpret_cast<WindowSearchData*>(
                lParam
                );

        DWORD windowProcessID = 0;

        GetWindowThreadProcessId(
            hwnd,
            &windowProcessID
        );

        if (windowProcessID !=
            data->processID) {

            return TRUE;
        }

        if (!IsWindowVisible(hwnd)) {
            return TRUE;
        }

        if (GetWindow(
            hwnd,
            GW_OWNER) != nullptr) {

            return TRUE;
        }

        wchar_t className[256]{};

        GetClassNameW(
            hwnd,
            className,
            static_cast<int>(
                std::size(className)
                )
        );

        if (_wcsicmp(
            className,
            L"Skyrim Special Edition") != 0) {

            return TRUE;
        }

        RECT rect{};

        if (!GetWindowRect(
            hwnd,
            &rect)) {

            return TRUE;
        }

        const LONG width =
            rect.right - rect.left;

        const LONG height =
            rect.bottom - rect.top;

        if (width < 640 ||
            height < 480) {

            return TRUE;
        }

        data->skyrimWindow = hwnd;

        return FALSE;
    }


    HWND FindSkyrimWindow()
    {
        WindowSearchData data{
            GetCurrentProcessId(),
            nullptr
        };

        EnumWindows(
            FindSkyrimWindowCallback,
            reinterpret_cast<LPARAM>(
                &data
                )
        );

        return data.skyrimWindow;
    }


    // ============================================================================
    // MONITOR DETECTION
    // ============================================================================

    struct MonitorSearchData
    {
        PluginConfig config{};
        RECT monitorRect{};
        bool found = false;
    };


    BOOL CALLBACK FindTargetMonitorCallback(
        HMONITOR monitor,
        HDC,
        LPRECT,
        LPARAM lParam)
    {
        auto* data =
            reinterpret_cast<MonitorSearchData*>(
                lParam
                );

        MONITORINFOEXW info{};
        info.cbSize = sizeof(info);

        if (!GetMonitorInfoW(
            monitor,
            reinterpret_cast<MONITORINFO*>(
                &info
                ))) {

            return TRUE;
        }

        const int x =
            info.rcMonitor.left;

        const int y =
            info.rcMonitor.top;

        const int width =
            info.rcMonitor.right -
            info.rcMonitor.left;

        const int height =
            info.rcMonitor.bottom -
            info.rcMonitor.top;

        if (x ==
            data->config.monitorX &&
            y ==
            data->config.monitorY &&
            width ==
            data->config.monitorWidth &&
            height ==
            data->config.monitorHeight) {

            data->monitorRect =
                info.rcMonitor;

            data->found = true;

            return FALSE;
        }

        return TRUE;
    }


    bool FindTargetMonitor(
        const PluginConfig& config,
        RECT& targetRect)
    {
        MonitorSearchData data{};
        data.config = config;

        EnumDisplayMonitors(
            nullptr,
            nullptr,
            FindTargetMonitorCallback,
            reinterpret_cast<LPARAM>(
                &data
                )
        );

        if (!data.found) {
            return false;
        }

        targetRect =
            data.monitorRect;

        return true;
    }


    // ============================================================================
    // MOVE SKYRIM
    // ============================================================================

    void MoveSkyrimToSelectedMonitor()
    {
        // No configuration = no action.
        //
        // This is intentional. The plugin must never guess where
        // Skyrim should be moved.

        const auto config =
            LoadConfig();

        if (!config) {
            return;
        }


        // Allow Skyrim's window to settle after kDataLoaded.

        std::this_thread::sleep_for(
            std::chrono::milliseconds(1500)
        );


        HWND skyrimWindow = nullptr;

        for (int attempt = 0;
            attempt < 20;
            ++attempt) {

            skyrimWindow =
                FindSkyrimWindow();

            if (skyrimWindow != nullptr) {
                break;
            }

            std::this_thread::sleep_for(
                std::chrono::milliseconds(250)
            );
        }

        if (skyrimWindow == nullptr) {
            return;
        }


        RECT targetRect{};

        if (!FindTargetMonitor(
            *config,
            targetRect)) {

            // The saved display configuration no longer
            // matches a currently connected logical monitor.
            //
            // Do nothing rather than guessing.

            return;
        }


        // Position only.
        //
        // We deliberately do NOT:
        //
        // - resize Skyrim
        // - alter Skyrim's window style
        // - change the Windows primary monitor
        // - change Windows display configuration
        // - modify SkyrimPrefs.ini
        // - modify SSEDisplayTweaks.ini
        // - manipulate other applications

        SetWindowPos(
            skyrimWindow,
            HWND_TOP,
            targetRect.left,
            targetRect.top,
            0,
            0,
            SWP_NOSIZE |
            SWP_NOACTIVATE |
            SWP_NOOWNERZORDER
        );
    }


    // ============================================================================
    // SKSE MESSAGE HANDLER
    // ============================================================================

    void OnSKSEMessage(
        SKSE::MessagingInterface::Message* message)
    {
        if (!message) {
            return;
        }

        if (message->type ==
            SKSE::MessagingInterface::kDataLoaded) {

            std::thread(
                MoveSkyrimToSelectedMonitor
            ).detach();
        }
    }
}


// ============================================================================
// SKSE ENTRY POINT
// ============================================================================

SKSEPluginLoad(const SKSE::LoadInterface* skse)
{
    SKSE::Init(skse);

    const auto messaging =
        SKSE::GetMessagingInterface();

    if (!messaging) {
        return false;
    }

    if (!messaging->RegisterListener(
        OnSKSEMessage)) {

        return false;
    }

    return true;
}