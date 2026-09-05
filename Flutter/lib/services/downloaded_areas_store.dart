import 'dart:convert';

import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:shared_preferences/shared_preferences.dart';

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

  static Future<void> upsert(DownloadedArea area) async {
    final areas = await load();
    areas.removeWhere((a) => a.label.toLowerCase() == area.label.toLowerCase());
    areas.insert(0, area);
    await _save(areas);
  }

  static Future<void> remove(String label) async {
    final areas = await load();
    areas.removeWhere((a) => a.label.toLowerCase() == label.toLowerCase());
    await _save(areas);
  }

  static Future<void> clear() => _save([]);

  static Future<void> _save(List<DownloadedArea> areas) async {
    final prefs = await SharedPreferences.getInstance();
    await prefs.setString(_prefsKey, jsonEncode(areas.map((a) => a.toJson()).toList()));
  }
}
