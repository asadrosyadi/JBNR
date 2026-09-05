import 'dart:async';
import 'dart:ui';

import 'package:flutter/foundation.dart';
import 'package:flutter/painting.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:http/http.dart' as http;

import 'map_tile_downloader.dart' show isOsmBlockedResponse;
import 'tile_disk_cache.dart';

class OfflineFirstTileProvider extends TileProvider {
  OfflineFirstTileProvider._();

  static OfflineFirstTileProvider? _instance;

  factory OfflineFirstTileProvider() =>
      _instance ??= OfflineFirstTileProvider._();

  final _client = http.Client();

  @override
  ImageProvider getImage(TileCoordinates coordinates, TileLayer options) {
    return _OfflineFirstImageProvider(
      z: coordinates.z,
      x: coordinates.x,
      y: coordinates.y,
      url: getTileUrl(coordinates, options),
      headers: headers,
      client: _client,
    );
  }
}

@immutable
class _OfflineFirstImageProvider
    extends ImageProvider<_OfflineFirstImageProvider> {
  final int z;
  final int x;
  final int y;
  final String url;
  final Map<String, String> headers;
  final http.Client client;

  const _OfflineFirstImageProvider({
    required this.z,
    required this.x,
    required this.y,
    required this.url,
    required this.headers,
    required this.client,
  });

  @override
  ImageStreamCompleter loadImage(
    _OfflineFirstImageProvider key,
    ImageDecoderCallback decode,
  ) {
    return MultiFrameImageStreamCompleter(
      codec: _load(decode),
      scale: 1,
      debugLabel: url,
    );
  }

  Future<Codec> _load(ImageDecoderCallback decode) async {
    Future<Codec> decodeBytes(Uint8List bytes) =>
        ImmutableBuffer.fromUint8List(bytes).then(decode);

    final cached = await TileDiskCache.getTile(z, x, y);
    if (cached != null) {
      try {
        return await decodeBytes(cached);
      } catch (_) {
      }
    }

    final response = await client.get(Uri.parse(url), headers: headers);
    if (isOsmBlockedResponse(response)) {
      throw Exception('Tile request blocked by server');
    }
    if (response.statusCode != 200) {
      throw Exception('Tile request failed: HTTP ${response.statusCode}');
    }

    unawaited(TileDiskCache.putTile(z, x, y, response.bodyBytes));
    return decodeBytes(response.bodyBytes);
  }

  @override
  SynchronousFuture<_OfflineFirstImageProvider> obtainKey(
    ImageConfiguration configuration,
  ) => SynchronousFuture(this);

  @override
  bool operator ==(Object other) =>
      identical(this, other) ||
      (other is _OfflineFirstImageProvider &&
          other.z == z &&
          other.x == x &&
          other.y == y);

  @override
  int get hashCode => Object.hash(z, x, y);
}
