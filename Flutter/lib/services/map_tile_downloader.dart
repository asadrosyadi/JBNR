import 'package:flutter_map/flutter_map.dart';
import 'package:http/http.dart' as http;

import 'tile_disk_cache.dart';
import 'tile_math.dart';

const String tileUrlTemplate = 'https://tile.openstreetmap.org/{z}/{x}/{y}.png';
// OSM's tile usage policy (https://operations.osmfoundation.org/policies/tiles/)
// requires a distinctive User-Agent identifying the app - generic/default
// ones like "com.example.*" (the unedited Flutter template value this used
// to be) get blocked outright, since that's indistinguishable from mass
// unconfigured-app traffic OSM explicitly filters out.
const String tileUserAgentPackageName = 'id.etextilejacket.monitoring';

/// OSM's tile server responds to a rate-limited/blocked client with an
/// HTTP 200 and a normal-looking 256x256 PNG - it just draws a big
/// "Access blocked" warning into the image itself, marked by this
/// response header. Since the status code alone can't tell a blocked
/// response from a real tile, every fetch must check for this header
/// explicitly - otherwise the warning image gets decoded, displayed,
/// and (worse) permanently written into the offline cache as if it
/// were the real tile for that spot.
bool isOsmBlockedResponse(http.Response response) =>
    response.headers.containsKey('x-blocked');

/// Thrown by [MapTileDownloader.download] when OSM's tile server starts
/// returning blocked responses partway through - continuing to hammer
/// it with more requests would only prolong the block.
class OsmTileServerBlockedException implements Exception {
  const OsmTileServerBlockedException();

  @override
  String toString() =>
      "OpenStreetMap's tile server has temporarily blocked this app's requests "
      '(see https://operations.osmfoundation.org/policies/tiles/).';
}

/// Pre-fetches OpenStreetMap tiles for a bounded area straight into
/// [TileDiskCache] - the same cache [OfflineFirstTileProvider] reads
/// from and writes to automatically while browsing - so the area stays
/// available once the phone loses signal.
class MapTileDownloader {
  MapTileDownloader._();

  /// Number of parallel tile requests in flight at once.
  ///
  /// This was previously raised to 12 for speed, but that was enough to
  /// get this app's requests actually blocked by OSM's tile server (it
  /// starts responding with a marked "access blocked" image instead of
  /// real tiles - see [isOsmBlockedResponse]). Back to a conservative
  /// value close to OSM's own stated guidance of ~2 requests/second, to
  /// let any existing block clear and avoid triggering it again.
  static const _concurrency = 2;

  /// Number of tiles a bounds/zoom-range combination would need - call
  /// before [download] to warn the user if it's excessive.
  static int countTiles({
    required LatLngBounds bounds,
    required int minZoom,
    required int maxZoom,
  }) {
    var total = 0;
    for (var z = minZoom; z <= maxZoom; z++) {
      final range = TileMath.tileRange(bounds, z);
      total += (range.maxX - range.minX + 1) * (range.maxY - range.minY + 1);
    }
    return total;
  }

  /// Downloads every tile covering [bounds] for each zoom level in
  /// [minZoom]..[maxZoom] into the cache, using [_concurrency] requests
  /// in flight at once. Reports progress via [onProgress] as (downloaded,
  /// total) after every tile attempt. Stops early once [isCancelled]
  /// returns true, or throws [OsmTileServerBlockedException] if the
  /// server starts returning blocked responses.
  static Future<void> download({
    required LatLngBounds bounds,
    required int minZoom,
    required int maxZoom,
    required void Function(int done, int total) onProgress,
    bool Function()? isCancelled,
  }) async {
    final coords = <(int, int, int)>[];
    for (var z = minZoom; z <= maxZoom; z++) {
      final range = TileMath.tileRange(bounds, z);
      for (var x = range.minX; x <= range.maxX; x++) {
        for (var y = range.minY; y <= range.maxY; y++) {
          coords.add((z, x, y));
        }
      }
    }

    final total = coords.length;
    var done = 0;
    var nextIndex = 0;
    var blocked = false;
    onProgress(done, total);

    final client = http.Client();
    try {
      Future<void> worker() async {
        while (true) {
          if (blocked) return;
          if (isCancelled?.call() ?? false) return;
          if (nextIndex >= coords.length) return;
          final (z, x, y) = coords[nextIndex++];

          final url = tileUrlTemplate
              .replaceAll('{z}', '$z')
              .replaceAll('{x}', '$x')
              .replaceAll('{y}', '$y');

          try {
            final response = await client.get(
              Uri.parse(url),
              headers: {'User-Agent': tileUserAgentPackageName},
            );
            if (isOsmBlockedResponse(response)) {
              blocked = true;
            } else if (response.statusCode == 200) {
              await TileDiskCache.putTile(z, x, y, response.bodyBytes);
            }
          } catch (_) {
            // Skip tiles that fail (connection drop mid-download, server
            // hiccup, etc.) - the rest of the region should still be
            // usable even if a handful of tiles are missing.
          }

          done++;
          onProgress(done, total);
        }
      }

      await Future.wait(List.generate(_concurrency, (_) => worker()));
    } finally {
      client.close();
    }

    if (blocked) throw const OsmTileServerBlockedException();
  }
}
