import 'dart:convert';
import 'dart:math';

import 'package:flutter_map/flutter_map.dart';
import 'package:http/http.dart' as http;
import 'package:latlong2/latlong.dart';

const _minRadiusKm = 3.0;

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

  static String _shortenDisplayName(String fullName) {
    final parts = fullName.split(',').map((p) => p.trim()).where((p) => p.isNotEmpty).toList();
    if (parts.isEmpty) return fullName;

    final short = parts.take(2).join(', ');
    return short.length > 60 ? '${short.substring(0, 57)}...' : short;
  }
}
