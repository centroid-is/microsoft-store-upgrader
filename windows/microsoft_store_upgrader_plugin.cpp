#include "include/microsoft_store_upgrader/microsoft_store_upgrader_plugin.h"

#include "flutter/method_channel.h"
#include "flutter/plugin_registrar_windows.h"
#include "flutter/standard_method_codec.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

// WinRT
#include <winrt/base.h>
#include <winrt/Windows.Foundation.h>
#include <winrt/Windows.Foundation.Collections.h>
#include <winrt/Windows.Services.Store.h>
#include <winrt/Windows.ApplicationModel.h>
#include <shellapi.h>

namespace {

using flutter::EncodableMap;
using flutter::EncodableValue;
using winrt::Windows::Services::Store::StoreContext;
using winrt::Windows::Services::Store::StorePackageUpdate;
using winrt::Windows::Services::Store::StorePackageUpdateResult;
using winrt::Windows::Services::Store::StorePackageUpdateState;
using winrt::Windows::Services::Store::StoreProductResult;
using winrt::Windows::ApplicationModel::Package;

class UpgraderWindowsStorePlugin : public std::enable_shared_from_this<UpgraderWindowsStorePlugin> {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows *registrar) {
    auto channel = std::make_shared<flutter::MethodChannel<EncodableValue>>(
        registrar->messenger(), "dev.centroid.upgrader_windows_store",
        &flutter::StandardMethodCodec::GetInstance());

    auto plugin = std::make_shared<UpgraderWindowsStorePlugin>();
    channel->SetMethodCallHandler(
        [plugin](const auto& call, auto result) { plugin->OnMethodCall(call, std::move(result)); });
  }

  UpgraderWindowsStorePlugin() {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
  }

 private:
  void OnMethodCall(const flutter::MethodCall<EncodableValue>& call,
                    std::unique_ptr<flutter::MethodResult<EncodableValue>> result) {
    try {
      const auto& method = call.method_name();

      if (method == "installUpdate") {
        RunInstallUpdates(std::move(result));
        return;
      }

      if (method == "openStore") {
        std::wstring productId;
        if (const auto* map = std::get_if<EncodableMap>(call.arguments())) {
          if (auto it = map->find(EncodableValue("productId")); it != map->end()) {
            if (const auto* s = std::get_if<std::string>(&it->second)) {
              productId.assign(s->begin(), s->end());
            }
          }
        }
        if (productId.empty()) {
          result->Error("bad_args", "productId is required");
          return;
        }
        const std::wstring uri = L"ms-windows-store://pdp/?ProductId=" + productId;
        ShellExecuteW(nullptr, L"open", uri.c_str(), nullptr, nullptr, SW_SHOWNORMAL);
        result->Success();
        return;
      }

      if (method == "getStoreInfo") {
        // Optional: accept a productId but it’s not required.
        RunGetStoreInfo(std::move(result));
        return;
      }

      result->NotImplemented();
    } catch (const winrt::hresult_error& e) {
      result->Error("winrt_error", winrt::to_string(e.message()));
    } catch (...) {
      result->Error("unknown_error", "Unexpected failure");
    }
  }

  // --- async helpers (coroutines) ---

  void RunInstallUpdates(std::unique_ptr<flutter::MethodResult<EncodableValue>> result) {
    auto self = shared_from_this();
    auto task = [self, result = std::move(result)]() mutable -> winrt::fire_and_forget {
      winrt::apartment_context ui;
      bool ok = false;
      bool hadError = false;
      std::string err;

      try {
        StoreContext ctx = StoreContext::GetDefault();
        auto updates = co_await ctx.GetAppAndOptionalStorePackageUpdatesAsync();
        if (updates.Size() > 0) {
          co_await ui; // must be on UI for request dialog
          StorePackageUpdateResult r =
              co_await ctx.RequestDownloadAndInstallStorePackageUpdatesAsync(updates);
          ok = (r.OverallState() == StorePackageUpdateState::Completed);
        }
      } catch (const winrt::hresult_error& e) {
        hadError = true; err = winrt::to_string(e.message());
      } catch (...) {
        hadError = true; err = "Unexpected failure";
      }

      co_await ui;
      if (hadError) { result->Error("winrt_error", err); co_return; }
      result->Success(EncodableValue(ok));
    };
    task();
  }

  void RunGetStoreInfo(std::unique_ptr<flutter::MethodResult<EncodableValue>> result) {
    auto self = shared_from_this();
    auto task = [self, result = std::move(result)]() mutable -> winrt::fire_and_forget {
      winrt::apartment_context ui;

      // Defaults
      std::string listingUrl;
      std::string latestVersion; // x.y.z.w if an update is known

      bool hadError = false;
      std::string err;

      try {
        StoreContext ctx = StoreContext::GetDefault();

        // Listing URL via StoreProduct.LinkUri
        StoreProductResult prod = co_await ctx.GetStoreProductForCurrentAppAsync();
        if (prod.Product() && prod.Product().LinkUri()) {
          auto uri = prod.Product().LinkUri().RawUri();
          listingUrl = winrt::to_string(uri);
        }
        // If updates exist, report the target version.
        auto updates = co_await ctx.GetAppAndOptionalStorePackageUpdatesAsync();
        if (updates.Size() > 0) {
          // Use version of the first updated package.
          auto pkg = updates.GetAt(0).Package(); // StorePackageUpdate.Package
          auto v = pkg.Id().Version();           // PackageVersion {Major, Minor, Build, Revision}
          latestVersion =
              std::to_string(v.Major) + "." + std::to_string(v.Minor) + "." +
              std::to_string(v.Build) + "." + std::to_string(v.Revision);
        }
      } catch (const winrt::hresult_error& e) {
        hadError = true; err = winrt::to_string(e.message());
      } catch (...) {
        hadError = true; err = "Unexpected failure";
      }

      co_await ui;
      if (hadError) { result->Error("winrt_error", err); co_return; }

      EncodableMap map;
      if (!listingUrl.empty()) map[EncodableValue("listingUrl")] = EncodableValue(listingUrl);
      if (!latestVersion.empty()) map[EncodableValue("latestVersion")] = EncodableValue(latestVersion);
      // Windows Store doesn’t expose release notes via this API.
      map[EncodableValue("releaseNotes")] = EncodableValue();

      result->Success(EncodableValue(map));
    };
    task();
  }
};

}  // namespace

void UpgraderWindowsStorePluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  UpgraderWindowsStorePlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
