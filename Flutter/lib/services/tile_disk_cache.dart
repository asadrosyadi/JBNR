import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_map/flutter_map.dart';
import 'package:path_provider/path_provider.dart';

import 'tile_math.dart';

/// A simple file-based tile cache: each tile is its own PNG file under
/// `<z>/<x>/<y>.png`. Unlike flutter_map's built-in cache (which keys
/// tiles by an opaque hash of the request URL), tiles here are
/// addressed directly by their (z, x, y) coordinates - so a downloaded
/// city's bounds+zoom range maps straight back to the exact set of
/// files it owns, making it possible to delete just that one city
/// without touching any other downloaded city or casually-browsed
/// tiles.
class TileDiskCache {
  TileDiskCache._();

  static Future<Directory> _root() async {
    final base = await getApplicationCacheDirectory();
    return Directory('${base.path}/map_tiles');
  }

  static Future<File> _fileFor(int z, int x, int y) async {
    final root = await _root();
    return File('${root.path}/$z/$x/$y.png');
  }

  static Future<Uint8List?> getTile(int z, int x, int y) async {
    final file = await _fileFor(z, x, y);
    if (!await file.exists()) return null;
    try {
      return await file.readAsBytes();
    } catch (_) {
      return null;
    }
  }

  static Future<void> putTile(int z, int x, int y, Uint8List bytes) async {
    try {
      final file = await _fileFor(z, x, y);
      await file.parent.create(recursive: true);
      await file.writeAsBytes(bytes, flush: true);
    } catch (_) {
      // Best-effort - a failed cache write shouldn't break map display.
    }
  }

  /// Deletes only the tiles covering [bounds] across [minZoom]..[maxZoom]
  /// - other downloaded areas and casually-cached tiles are untouched.
  static Future<void> deleteRegion({
    required LatLngBounds bounds,
    required int minZoom,
    required int maxZoom,
  }) async {
    for (var z = minZoom; z <= maxZoom; z++) {
      final range = TileMath.tileRange(bounds, z);
      for (var x = range.minX; x <= range.maxX; x++) {
        for (var y = range.minY; y <= range.maxY; y++) {
          final file = await _fileFor(z, x, y);
          if (await file.exists()) {
            try {
              await file.delete();
            } catch (_) {}
          }
        }
      }
    }
  }

  /// Total size (in bytes) of every tile cached so far, across every
  /// downloaded city and casual browsing.
  static Future<int> sizeInBytes() async {
    final root = await _root();
    if (!root.existsSync()) return 0;

    var total = 0;
    await for (final entity in root.list(recursive: true, followLinks: false)) {
      if (entity is! File) continue;
      try {
        total += await entity.length();
      } catch (_) {}
    }
    return total;
  }

  /// Deletes every cached tile, from every city and casual browsing.
  static Future<void> clearAll() async {
    final root = await _root();
    if (await root.exists()) {
      try {
        await root.delete(recursive: true);
      } catch (_) {}
    }
  }
}
