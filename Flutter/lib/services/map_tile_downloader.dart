import 'package:flutter_map/flutter_map.dart';
import 'package:http/http.dart' as http;

import 'tile_disk_cache.dart';
import 'tile_math.dart';

const String tileUrlTemplate = 'https://tile.openstreetmap.org/{z}/{x}/{y}.png';
const String tileUserAgentPackageName = 'id.etextilejacket.monitoring';

bool isOsmBlockedResponse(http.Response response) =>
    response.headers.containsKey('x-blocked');

class OsmTileServerBlockedException implements Exception {
  const OsmTileServerBlockedException();

  @override
  String toString() =>
      "OpenStreetMap's tile server has temporarily blocked this app's requests "
      '(see https://operations.osmfoundation.org/policies/tiles/).';
}

class MapTileDownloader {
  MapTileDownloader._();

  static const _concurrency = 2;

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
