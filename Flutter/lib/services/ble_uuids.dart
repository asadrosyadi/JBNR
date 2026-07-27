import 'package:flutter_blue_plus/flutter_blue_plus.dart';

/// GATT service/characteristic UUIDs used by the Smart Jacket firmware
/// (ESP32 + Arduino BLE), taken directly from the firmware source
/// (`Arduino/Arduino.ino`, `ble_server_init()`/`BleUuids`-equivalent
/// `GATTS_*` defines). Every jacket - transmitter or receiver role, it's
/// the same firmware - runs this same BLE server, so the phone can
/// connect to whichever one is in range.
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

  /// Substring used to filter scan results down to jacket devices.
  /// Each jacket advertises `ESP-BLE-<NODE_NAME>` (e.g. `ESP-BLE-Jaket-1`)
  /// so this still matches while letting the scan list show which
  /// specific node each entry is.
  static const String deviceNameFilter = 'ESP-BLE';

  /// Wire format (jacket -> phone and phone -> jacket), one BLE link
  /// fanning out to/in from the connected jacket's own telemetry plus
  /// whatever it relays in from peer jackets over LoRa. Every payload is
  /// prefixed with the sending/target node's `NODE_NAME` (see the
  /// firmware's `NODE_NAME` define - one per physical jacket) and a
  /// colon, so one BLE link can carry many LoRa nodes on the same four
  /// characteristics:
  ///   location:    `<node>:<lat>,<lon>`          e.g. `Jaket-A:-6.200000,106.816600`
  ///   temperature: `<node>:Temp:<celsius>`       e.g. `Jaket-A:Temp:25.30`
  ///   vitals:      `<node>:HR:<bpm>,SpO2:<pct>`  e.g. `Jaket-A:HR:72.50,SpO2:98.00`
  ///   text (in):   `<node>:Text:<message>`       e.g. `Jaket-A:Text:hello`
  ///   text (out):  `<node>:<message>`            sent over LoRa to one node
  ///                `ALL:<message>`               broadcast to every node
  /// A payload with no recognised `<node>:` prefix is attributed to a
  /// single implicit node so older single-jacket firmware keeps working.
  static const String defaultNodeName = 'Jaket-1';

  /// Matches the firmware's `LORA_CHAT_MAX_LEN` (`Arduino/Arduino.ino`) -
  /// chat text beyond this is silently truncated on the wire, so the UI
  /// caps input at the same length rather than surprising the sender.
  static const int maxChatTextLength = 128;
}
