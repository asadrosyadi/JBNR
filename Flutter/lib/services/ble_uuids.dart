import 'package:flutter_blue_plus/flutter_blue_plus.dart';

class BleUuids {
  BleUuids._();

  static final Guid vitalsService = Guid('000000ff-0000-1000-8000-00805f9b34fb');
  static final Guid vitalsCharacteristic = Guid('0000ff01-0000-1000-8000-00805f9b34fb');

  static final Guid locationService = Guid('000000ee-0000-1000-8000-00805f9b34fb');
  static final Guid locationCharacteristic = Guid('0000ee01-0000-1000-8000-00805f9b34fb');

  static final Guid temperatureService = Guid('000000dd-0000-1000-8000-00805f9b34fb');
  static final Guid temperatureCharacteristic = Guid('0000dd01-0000-1000-8000-00805f9b34fb');

  static final Guid textService = Guid('000000cc-0000-1000-8000-00805f9b34fb');
  static final Guid textCharacteristic = Guid('0000cc01-0000-1000-8000-00805f9b34fb');

  static const String deviceNameFilter = 'ESP-BLE';

  static const String defaultNodeName = 'Jaket-1';

  static const int maxChatTextLength = 128;
}
