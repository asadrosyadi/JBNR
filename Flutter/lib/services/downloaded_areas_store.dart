import 'dart:convert';

import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:shared_preferences/shared_preferences.dart';

/// A named area (a city searched via [Geocoder]) that's already been
/// downloaded for offline use.
class DownloadedArea {
  final String label;
  final LatLngBounds bounds;
  final int minZoom;
  final int maxZoom;
  final DateTime downloadedAt;

  DownloadedArea({
    required this.label,
    required this.bounds,
    required this.minZoom,
    required this.maxZoom,
    required this.downloadedAt,
  });

  Map<String, dynamic> toJson() => {
        'label': label,
        'south': bounds.south,
        'west': bounds.west,
        'north': bounds.north,
        'east': bounds.east,
        'minZoom': minZoom,
        'maxZoom': maxZoom,
        'downloadedAt': downloadedAt.toIso8601String(),
      };

  factory DownloadedArea.fromJson(Map<String, dynamic> json) => DownloadedArea(
        label: json['label'] as String,
        bounds: LatLngBounds(
          LatLng((json['south'] as num).toDouble(), (json['west'] as num).toDouble()),
          LatLng((json['north'] as num).toDouble(), (json['east'] as num).toDouble()),
        ),
        minZoom: json['minZoom'] as int,
        maxZoom: json['maxZoom'] as int,
        downloadedAt: DateTime.parse(json['downloadedAt'] as String),
      );
}

/// Persists which named areas have already been downloaded for
/// offline use, so the city-search dialog can surface them instead of
/// making the wearer re-search from scratch every time - re-selecting
/// one re-downloads straight from its saved bounds, without needing to
/// hit Nominatim (or even be online, if nothing's actually missing
/// from the tile cache).
class DownloadedAreasStore {
  DownloadedAreasStore._();

  static const _prefsKey = 'downloaded_map_areas_v1';

  static Future<List<DownloadedArea>> load() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final raw = prefs.getString(_prefsKey);
      if (raw == null) return [];
      final decoded = jsonDecode(raw) as List;
      final areas = decoded
          .map((e) => DownloadedArea.fromJson(e as Map<String, dynamic>))
          .toList();
      areas.sort((a, b) => b.downloadedAt.compareTo(a.downloadedAt));
      return areas;
    } catch (_) {
      return [];
    }
  }

  /// Records [area] as downloaded, replacing any existing entry with
  /// the same (case-insensitive) label.
  static Future<void> upsert(DownloadedArea area) async {
    final areas = await load();
    areas.removeWhere((a) => a.label.toLowerCase() == area.label.toLowerCase());
    areas.insert(0, area);
    await _save(areas);
  }

  /// Forgets [label] - used after its tiles are deleted so it stops
  /// showing up as "already downloaded".
  static Future<void> remove(String label) async {
    final areas = await load();
    areas.removeWhere((a) => a.label.toLowerCase() == label.toLowerCase());
    await _save(areas);
  }

  /// Forgets every downloaded area - used after a full cache wipe.
  static Future<void> clear() => _save([]);

  static Future<void> _save(List<DownloadedArea> areas) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_prefsKey, jsonEncode(areas.map((a) => a.toJson()).toList()));
  }
}
