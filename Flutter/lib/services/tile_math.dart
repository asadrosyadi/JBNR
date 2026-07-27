import 'dart:math';

import 'package:flutter_map/flutter_map.dart';

/// Shared "slippy map" tile-index math, used by [MapTileDownloader] to
/// enumerate a region's tiles and by [TileDiskCache] to delete exactly
/// those same tiles again later, without touching anything else.
class TileMath {
  TileMath._();

  static ({int minX, int maxX, int minY, int maxY}) tileRange(
    LatLngBounds bounds,
    int z,
  ) {
    final maxIndex = (1 << z) - 1;
    return (
      minX: _lonToTileX(bounds.west, z).clamp(0, maxIndex),
      maxX: _lonToTileX(bounds.east, z).clamp(0, maxIndex),
      minY: _latToTileY(bounds.north, z).clamp(0, maxIndex),
      maxY: _latToTileY(bounds.south, z).clamp(0, maxIndex),
    );
  }

  static int _lonToTileX(double lon, int z) => ((lon + 180.0) / 360.0 * (1 << z)).floor();

  static int _latToTileY(double lat, int z) {
    final latRad = lat * pi / 180.0;
    return ((1.0 - log(tan(latRad) + 1 / cos(latRad)) / pi) / 2.0 * (1 << z)).floor();
  }
}
