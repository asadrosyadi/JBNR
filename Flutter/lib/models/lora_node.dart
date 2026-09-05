import 'package:geolocator/geolocator.dart';

class LoraNode {
  final String name;
  double? latitude;
  double? longitude;
  double? temperatureCelsius;
  double? heartRateBpm;
  double? spo2Percent;
  DateTime lastUpdate;

  LoraNode({required this.name, DateTime? lastUpdate})
      : lastUpdate = lastUpdate ?? DateTime.now();

  bool get hasFix => latitude != null && longitude != null;

  double? distanceFrom(double? fromLatitude, double? fromLongitude) {
    if (!hasFix || fromLatitude == null || fromLongitude == null) return null;
    return Geolocator.distanceBetween(
        fromLatitude, fromLongitude, latitude!, longitude!);
  }

  String? compassFrom(double? fromLatitude, double? fromLongitude) {
    if (!hasFix || fromLatitude == null || fromLongitude == null) return null;
    final bearing = Geolocator.bearingBetween(
        fromLatitude, fromLongitude, latitude!, longitude!);
    const labels = ['N', 'NE', 'E', 'SE', 'S', 'SW', 'W', 'NW'];
    final normalized = (bearing + 360) % 360;
    final index = ((normalized + 22.5) / 45).floor() % 8;
    return labels[index];
  }
}
