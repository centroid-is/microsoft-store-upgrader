#ifndef FLUTTER_PLUGIN_MICROSOFT_STORE_UPGRADER_PLUGIN_H_
#define FLUTTER_PLUGIN_MICROSOFT_STORE_UPGRADER_PLUGIN_H_

#include <flutter/plugin_registrar_windows.h>

#ifdef MICROSOFT_STORE_UPGRADER_PLUGIN_IMPL
#define MICROSOFT_STORE_UPGRADER_PLUGIN_EXPORT __declspec(dllexport)
#else
#define MICROSOFT_STORE_UPGRADER_PLUGIN_EXPORT __declspec(dllimport)
#endif

#ifdef __cplusplus
extern "C" {
#endif

MICROSOFT_STORE_UPGRADER_PLUGIN_EXPORT void
MicrosoftStoreUpgraderPluginRegisterWithRegistrar(
    FlutterDesktopPluginRegistrarRef registrar);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // FLUTTER_PLUGIN_MICROSOFT_STORE_UPGRADER_PLUGIN_H_
