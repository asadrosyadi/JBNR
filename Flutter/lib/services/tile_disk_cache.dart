import 'dart:io';
import 'dart:typed_data';

import 'package:flutter_map/flutter_map.dart';
import 'package:path_provider/path_provider.dart';

import 'tile_math.dart';

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
    }
  }

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

  static Future<void> clearAll() async {
    final root = await _root();
    if (await root.exists()) {
      try {
        await root.delete(recursive: true);
      } catch (_) {}
    }
  }
}
