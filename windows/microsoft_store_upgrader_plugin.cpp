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
#include <appmodel.h>

namespace {

using flutter::EncodableMap;
using flutter::EncodableValue;
using winrt::Windows::Services::Store::StoreContext;
using winrt::Windows::Services::Store::StorePackageUpdate;
using winrt::Windows::Services::Store::StorePackageUpdateResult;
using winrt::Windows::Services::Store::StorePackageUpdateState;
using winrt::Windows::Services::Store::StoreProductResult;
using winrt::Windows::ApplicationModel::Package;

static bool IsPackaged() {
  UINT32 len = 0;
  LONG rc = GetCurrentPackageFullName(&len, nullptr);
  return rc != APPMODEL_ERROR_NO_PACKAGE; // unpackaged => 15700
}

class MicrosoftStoreUpgraderPlugin : public flutter::Plugin {
 public:
  static void RegisterWithRegistrar(flutter::PluginRegistrarWindows* registrar) {
    auto plugin = std::make_unique<MicrosoftStoreUpgraderPlugin>();

    plugin->channel_ = std::make_unique<flutter::MethodChannel<flutter::EncodableValue>>(
        registrar->messenger(),
        "dev.centroid.upgrader_windows_store",
        &flutter::StandardMethodCodec::GetInstance());

    plugin->channel_->SetMethodCallHandler(
      [p = plugin.get()](auto const& call, auto result) {
        p->OnMethodCall(call, std::move(result));
      });

    registrar->AddPlugin(std::move(plugin)); // registrar owns lifetime till shutdown
  }

  MicrosoftStoreUpgraderPlugin() {
    winrt::init_apartment(winrt::apartment_type::single_threaded);
  }
  ~MicrosoftStoreUpgraderPlugin() {}

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

  void RunInstallUpdates(std::unique_ptr<flutter::MethodResult<EncodableValue>> result) {
    try {
      if (!IsPackaged()) {
        result->Error("not_packaged", "MSIX/Appx packaging required for StoreContext.");
        return;
      }

      using winrt::Windows::Foundation::Collections::IVectorView;
      using winrt::Windows::Services::Store::StoreContext;
      using winrt::Windows::Services::Store::StorePackageUpdate;
      using winrt::Windows::Services::Store::StorePackageUpdateResult;
      using winrt::Windows::Services::Store::StorePackageUpdateState;

      StoreContext ctx = StoreContext::GetDefault();

      // Block until we have the list of updates.
      IVectorView<StorePackageUpdate> updates =
          ctx.GetAppAndOptionalStorePackageUpdatesAsync().get();

      bool ok = false;
      if (updates && updates.Size() > 0) {
        // This will block until the Store UI finishes (user consent + install).
        StorePackageUpdateResult r =
            ctx.RequestDownloadAndInstallStorePackageUpdatesAsync(updates).get();
        ok = (r.OverallState() == StorePackageUpdateState::Completed);
      }

      result->Success(EncodableValue(ok));
    } catch (const winrt::hresult_error& e) {
      result->Error("winrt_error", winrt::to_string(e.message()));
    } catch (...) {
      result->Error("unknown_error", "Unexpected failure");
    }
  }

  void RunGetStoreInfo(std::unique_ptr<flutter::MethodResult<EncodableValue>> result) {
    try {
      if (!IsPackaged()) {
        result->Error("not_packaged", "MSIX/Appx packaging required for StoreContext.");
        return;
      }

      using winrt::Windows::Foundation::Collections::IVectorView;
      using winrt::Windows::Services::Store::StoreContext;
      using winrt::Windows::Services::Store::StorePackageUpdate;
      using winrt::Windows::Services::Store::StoreProductResult;

      StoreContext ctx = StoreContext::GetDefault();

      // Listing URL
      std::string listingUrl;
      StoreProductResult prod = ctx.GetStoreProductForCurrentAppAsync().get();
      if (prod && prod.Product() && prod.Product().LinkUri()) {
        listingUrl = winrt::to_string(prod.Product().LinkUri().RawUri());
      }

      // Latest version (if any updates exist)
      std::string latestVersion;
      IVectorView<StorePackageUpdate> updates =
          ctx.GetAppAndOptionalStorePackageUpdatesAsync().get();
      if (updates && updates.Size() > 0) {
        auto v = updates.GetAt(0).Package().Id().Version();
        latestVersion = std::to_string(v.Major) + "." + std::to_string(v.Minor) + "." +
                        std::to_string(v.Build) + "." + std::to_string(v.Revision);
      }

      EncodableMap map;
      if (!listingUrl.empty())
        map[EncodableValue("listingUrl")] = EncodableValue(listingUrl);
      if (!latestVersion.empty())
        map[EncodableValue("latestVersion")] = EncodableValue(latestVersion);
      // Windows Store API doesn’t expose release notes here.
      map[EncodableValue("releaseNotes")] = EncodableValue();

      result->Success(EncodableValue(std::move(map)));
    } catch (const winrt::hresult_error& e) {
      result->Error("winrt_error", winrt::to_string(e.message()));
    } catch (...) {
      result->Error("unknown_error", "Unexpected failure");
    }
  }

  private:
    std::unique_ptr<flutter::MethodChannel<flutter::EncodableValue>> channel_;
};

}  // namespace

void MicrosoftStoreUpgraderPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar) {
  MicrosoftStoreUpgraderPlugin::RegisterWithRegistrar(
      flutter::PluginRegistrarManager::GetInstance()
          ->GetRegistrar<flutter::PluginRegistrarWindows>(registrar));
}
