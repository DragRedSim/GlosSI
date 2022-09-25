/*
Copyright 2021-2022 Peter Repukat - FlatspotSoftware

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/
#pragma once

#include <fstream>
#include <regex>
#include <string>
#include <nlohmann/json.hpp>
#include <spdlog/spdlog.h>
#include <locale>
#include <codecvt>

#ifdef WIN32
#define NOMINMAX
#include <Windows.h>
#endif


namespace Settings {

inline struct Launch {
    bool launch = false;
    std::wstring launchPath;
    std::wstring launchAppArgs;
    bool closeOnExit = true;
    bool waitForChildProcs = true;
    bool isUWP = false;
} launch;

inline struct Devices {
    bool hideDevices = true;
    bool realDeviceIds = false;
} devices;

inline struct Window {
    bool windowMode = false;
    int maxFps = 0;
    float scale = 0.f;
    bool disableOverlay = false;
} window;

inline struct Controller {
    int maxControllers = 1;
    bool allowDesktopConfig = false;
    bool emulateDS4 = false;
} controller;

inline bool extendedLogging = false;

inline std::filesystem::path settings_path_ = "";


inline bool checkIsUwp(const std::wstring& launch_path)
{
    if (launch_path.find(L"://") != std::wstring::npos) {
        return false;
    }
    std::wsmatch m;
    if (!std::regex_search(launch_path, m, std::wregex(L"^.{1,3}:"))) {
        return true;
    }
    return false;
}

#ifdef WIN32
inline bool isWin10 = false;

 typedef LONG NTSTATUS, *PNTSTATUS;
#define STATUS_SUCCESS (0x00000000)

typedef NTSTATUS(WINAPI* RtlGetVersionPtr)(PRTL_OSVERSIONINFOW);

inline RTL_OSVERSIONINFOW GetRealOSVersion()
{
    HMODULE hMod = ::GetModuleHandleW(L"ntdll.dll");
    if (hMod) {
        RtlGetVersionPtr fxPtr = (RtlGetVersionPtr)::GetProcAddress(hMod, "RtlGetVersion");
        if (fxPtr != nullptr) {
            RTL_OSVERSIONINFOW rovi = {0};
            rovi.dwOSVersionInfoSize = sizeof(rovi);
            if (STATUS_SUCCESS == fxPtr(&rovi)) {
                return rovi;
            }
        }
    }
    RTL_OSVERSIONINFOW rovi = {0};
    return rovi;
}

inline void checkWinVer()
{
    auto VN = GetRealOSVersion();
    isWin10 = VN.dwBuildNumber < 22000;

    if (isWin10) {
        spdlog::info("Running on Windows 10; Winver: {}.{}.{}", VN.dwMajorVersion, VN.dwMinorVersion, VN.dwBuildNumber);
    } else {
        spdlog::info("Running on Windows 11; Winver: {}.{}.{}", VN.dwMajorVersion, VN.dwMinorVersion, VN.dwBuildNumber);
    }

}
#endif

inline void Parse(std::wstring arg1)
{
    if (!arg1.ends_with(L".json")) {
        arg1 += L".json";
    }
    std::filesystem::path path(arg1);
    if (path.has_extension() && !std::filesystem::exists(path)) {
        path = std::filesystem::temp_directory_path()
                   .parent_path()
                   .parent_path()
                   .parent_path();

        path /= "Roaming";
        path /= "GlosSI";
        path /= "Targets";
        path /= arg1;
    }
    std::ifstream json_file;
    json_file.open(path);
    if (!json_file.is_open()) {
        spdlog::error(L"Couldn't open settings file {}", path.wstring());
        return;
    }
    settings_path_ = path;

        auto safeParseValue = [](const auto& object, const auto& key, auto& value) {
        try {
            if (object.is_null() || object.empty() || object.at(key).empty() || object.at(key).is_null()) {
                return;
            }
            value = object[key];
        }
        catch (const nlohmann::json::exception& e) {
            spdlog::warn("Err parsing \"{}\"; {}", key, e.what());
        }
        catch (const std::exception& e) {
            spdlog::warn("Err parsing \"{}\"; {}", key, e.what());
        }
    };

    auto safeWStringParse = [&safeParseValue](const auto& object, const auto& key, std::wstring& value) {
        std::string meh;
        safeParseValue(object, key, meh);
        if (!meh.empty()) {
            // This assumes that char is utf8 and wchar_t is utf16, which is guaranteed on Windows.
            value = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().from_bytes(meh);
        }
    };

    const auto json = nlohmann::json::parse(json_file);
    int version;
    safeParseValue(json, "version" ,version);
    if (version != 1) { // TODO: versioning stuff
        spdlog::warn("Config version doesn't match application version.");
    }

    // TODO: make this as much generic as fits in about the same amount of code if one would parse every value separately.

    if (auto launchconf = json["launch"]; launchconf.is_object()) {
        safeParseValue(launchconf, "launch", launch.launch);
        safeWStringParse(launchconf, "launchPath", launch.launchPath);
        safeWStringParse(launchconf, "launchAppArgs", launch.launchAppArgs);
        safeParseValue(launchconf, "closeOnExit", launch.closeOnExit);
        safeParseValue(launchconf, "waitForChildProcs", launch.waitForChildProcs);
    }

    if (auto devconf = json["devices"]; devconf.is_object()) {
        safeParseValue(devconf, "hideDevices", devices.hideDevices);
        safeParseValue(devconf, "realDeviceIds", devices.realDeviceIds);
    }

    if (auto winconf = json["window"]; winconf.is_object()) {
        safeParseValue(winconf, "windowMode", window.windowMode);
        safeParseValue(winconf, "maxFps", window.maxFps);
        safeParseValue(winconf, "scale", window.scale);
        safeParseValue(winconf, "disableOverlay", window.disableOverlay);
    }

    if (auto controllerConf = json["controller"]; controllerConf.is_object()) {
        safeParseValue(controllerConf, "maxControllers", controller.maxControllers);
        safeParseValue(controllerConf, "allowDesktopConfig", controller.allowDesktopConfig);
        safeParseValue(controllerConf, "emulateDS4", controller.emulateDS4);
    }

    safeParseValue(json, "extendedLogging", extendedLogging);

    json_file.close();

    spdlog::debug("Read config file \"{}\"; config: {}", path.string(), json.dump());

    if (launch.launch) {
        launch.isUWP = checkIsUwp(launch.launchPath);
    }
}

inline void StoreSettings()
{
    nlohmann::json json;
    json["version"] = 1;
    json["launch"]["launch"] = launch.launch;
    json["launch"]["launchPath"] = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(launch.launchPath);
    json["launch"]["launchAppArgs"] = std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>>().to_bytes(launch.launchAppArgs);
    json["launch"]["closeOnExit"] = launch.closeOnExit;
    json["launch"]["waitForChildProcs"] = launch.waitForChildProcs;
    json["devices"]["hideDevices"] = devices.hideDevices;
    json["devices"]["realDeviceIds"] = devices.realDeviceIds;
    json["window"]["windowMode"] = window.windowMode;
    json["window"]["maxFps"] = window.maxFps;
    json["window"]["scale"] = window.scale;
    json["window"]["disableOverlay"] = window.disableOverlay;
    json["controller"]["maxControllers"] = controller.maxControllers;
    json["controller"]["allowDesktopConfig"] = controller.allowDesktopConfig;
    json["controller"]["emulateDS4"] = controller.emulateDS4;

    json["extendedLogging"] = extendedLogging;

    std::ofstream json_file;
    json_file.open(settings_path_);
    if (!json_file.is_open()) {
        spdlog::error(L"Couldn't open settings file {}", settings_path_.wstring());
        return;
    }
    json_file << json.dump(4);
    json_file.close();
}

} // namespace Settings
