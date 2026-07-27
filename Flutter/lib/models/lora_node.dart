import 'package:geolocator/geolocator.dart';

/// Live telemetry for a single LoRa jacket node, relayed to the phone
/// over the one BLE link to the receiver. Multiple nodes can be active
/// at once; each is keyed by the name it advertises in its message
/// prefix (see `BleUuids` for the wire format), e.g. "Jaket-A".
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

  /// 8-point compass direction (N/NE/E/SE/S/SW/W/NW) from the given
  /// point to this node.
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
