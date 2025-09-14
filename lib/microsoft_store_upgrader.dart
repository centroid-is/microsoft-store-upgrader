import 'dart:async';
import 'package:flutter/services.dart';
import 'package:upgrader/upgrader.dart';
import 'package:version/version.dart';

/// Thin wrapper around our Windows host methods.
class _WinStoreApi {
  static const _ch = MethodChannel('dev.centroid.upgrader_windows_store');

  static Future<bool> installUpdate() async =>
      (await _ch.invokeMethod<bool>('installUpdate')) ?? false;

  /// Returns a Map like:
  /// { listingUrl: String, latestVersion: String?, releaseNotes: String? }
  static Future<Map<String, dynamic>> getStoreInfo({String? productId}) async {
    final res = await _ch.invokeMethod<dynamic>(
      'getStoreInfo',
      productId == null ? null : <String, dynamic>{'productId': productId},
    );
    if (res is Map) {
      return res.map((k, v) => MapEntry(k.toString(), v));
    }
    return const {};
  }

  static Future<void> openStore(String productId) async {
    await _ch.invokeMethod('openStore', {'productId': productId});
  }
}

/// An `upgrader` store implementation for Microsoft Store on Windows.
///
/// Provide `productId` (e.g. 9NBLGGH4X8GH) if you also want to support
/// opening the PDP by ID; otherwise we’ll resolve the listing URL from
/// the Store at runtime.
class UpgraderWindowsStore extends UpgraderStore {
  final String? productId;

  UpgraderWindowsStore({this.productId});

  @override
  Future<UpgraderVersionInfo> getVersionInfo({
    required UpgraderState state,
    required Version installedVersion,
    required String? country,
    required String? language,
  }) async {
    // Ask the Windows host for listing URL + latest version.
    final info = await _WinStoreApi.getStoreInfo(productId: productId);
    final listingUrl = (info['listingUrl'] as String?) ??
        (productId != null
            ? 'ms-windows-store://pdp/?ProductId=$productId'
            : null);

    // Latest version might be missing if no update exists.
    final latestStr = info['latestVersion'] as String?;
    final appStoreVersion =
        latestStr != null ? Version.parse(latestStr) : installedVersion;

    return UpgraderVersionInfo(
      appStoreListingURL: listingUrl,
      appStoreVersion: appStoreVersion,
      installedVersion: installedVersion,
      releaseNotes: info['releaseNotes'] as String?, // usually null on Windows
      isCriticalUpdate: null, // todo
    );
  }

  /// Optional helper your UI can call for one-click in-app update.
  static Future<bool> installUpdate() => _WinStoreApi.installUpdate();

  /// Optional helper to open the store explicitly.
  static Future<void> openStore(String productId) =>
      _WinStoreApi.openStore(productId);
}
