//
//  Generated file. Do not edit.
//

// clang-format off

#include "generated_plugin_registrant.h"

#include <microsoft_store_upgrader/microsoft_store_upgrader_plugin.h>
#include <url_launcher_windows/url_launcher_windows.h>

void RegisterPlugins(flutter::PluginRegistry* registry) {
  MicrosoftStoreUpgraderPluginRegisterWithRegistrar(
      registry->GetRegistrarForPlugin("MicrosoftStoreUpgraderPlugin"));
  UrlLauncherWindowsRegisterWithRegistrar(
      registry->GetRegistrarForPlugin("UrlLauncherWindows"));
}
