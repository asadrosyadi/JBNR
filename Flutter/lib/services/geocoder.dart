import 'dart:convert';
import 'dart:math';

import 'package:flutter_map/flutter_map.dart';
import 'package:http/http.dart' as http;
import 'package:latlong2/latlong.dart';

/// Below this, a place's bounding box is padded out to a circle of this
/// radius around its center point. Needed because most named places
/// that *aren't* administrative areas (a mountain peak, a campus, a
/// landmark, ...) are only mapped in OpenStreetMap as a single point,
/// not a bounded polygon - so Nominatim returns a near-zero-size
/// bounding box for them. Without padding, "downloading" such a place
/// would only fetch a single pinpoint tile instead of a useful area
/// around it.
const _minRadiusKm = 3.0;

/// A place found by [Geocoder.search].
class GeocodeResult {
  final String displayName;
  final LatLngBounds bounds;
  final LatLng center;

  GeocodeResult({
    required this.displayName,
    required this.bounds,
    required this.center,
  });
}

/// Looks up any named place - a city, a mountain, a campus, a
/// landmark, an address, ... - using OpenStreetMap's free Nominatim
/// geocoding service. No API key or billing needed.
///
/// Only used for one-off user-triggered searches (typing a place to
/// download), so usage stays well within Nominatim's fair-use policy
/// (max ~1 request/second, valid identifying User-Agent required).
class Geocoder {
  Geocoder._();

  static Future<GeocodeResult?> search(String query) async {
    final uri = Uri.https('nominatim.openstreetmap.org', '/search', {
      'q': query,
      'format': 'jsonv2',
      'limit': '1',
    });

    final response = await http.get(
      uri,
      headers: {
        'User-Agent': 'com.example.smartjacketreceiver (E-Textile Jacket offline map search)',
      },
    );
    if (response.statusCode != 200) return null;

    final results = jsonDecode(response.body) as List;
    if (results.isEmpty) return null;
    final result = results.first as Map<String, dynamic>;

    final box = (result['boundingbox'] as List).cast<String>();
    var south = double.parse(box[0]);
    var north = double.parse(box[1]);
    var west = double.parse(box[2]);
    var east = double.parse(box[3]);

    final centerLat = double.parse(result['lat'] as String);
    final centerLon = double.parse(result['lon'] as String);

    // 1 degree of latitude is ~111km everywhere; 1 degree of longitude
    // shrinks towards the poles by a factor of cos(latitude).
    final cosLat = cos(centerLat * pi / 180).abs().clamp(0.1, 1.0);

    if ((north - south) * 111.0 < _minRadiusKm * 2) {
      final latDelta = _minRadiusKm / 111.0;
      south = centerLat - latDelta;
      north = centerLat + latDelta;
    }
    if ((east - west) * 111.0 * cosLat < _minRadiusKm * 2) {
      final lonDelta = _minRadiusKm / (111.0 * cosLat);
      west = centerLon - lonDelta;
      east = centerLon + lonDelta;
    }

    return GeocodeResult(
      displayName: _shortenDisplayName(result['display_name'] as String),
      bounds: LatLngBounds(LatLng(south, west), LatLng(north, east)),
      center: LatLng(centerLat, centerLon),
    );
  }

  /// Nominatim's `display_name` is a full address hierarchy (street,
  /// suburb, city, province, postcode, country, ...) - readable as a
  /// single line, but far too long to show as a place's title without
  /// overflowing dialogs/list rows. Keep just the first couple of
  /// segments (the specific place plus its immediate locality), which
  /// is normally enough to identify it.
  static String _shortenDisplayName(String fullName) {
    final parts = fullName.split(',').map((p) => p.trim()).where((p) => p.isNotEmpty).toList();
    if (parts.isEmpty) return fullName;

    final short = parts.take(2).join(', ');
    return short.length > 60 ? '${short.substring(0, 57)}...' : short;
  }
}
