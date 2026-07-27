import 'dart:async';
import 'dart:convert';
import 'dart:io' show Platform;

import 'package:flutter/foundation.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:geolocator/geolocator.dart';
import 'package:shared_preferences/shared_preferences.dart';

import '../models/jacket_message.dart';
import '../models/lora_node.dart';
import 'ble_uuids.dart';
import 'notification_service.dart';

const String _messagesPrefsKey = 'chat_messages_v1';

enum JacketConnectionState { disconnected, scanning, connecting, connected }

/// Owns the BLE lifecycle for the Smart Jacket receiver: scanning,
/// connecting, subscribing to its data channels (location, temperature,
/// vitals, text) and writing outgoing messages. A single receiver
/// relays telemetry for every LoRa node it hears from, so this service
/// fans that stream out into a [nodes] map keyed by node name. Also
/// tracks the phone's own GPS position so the UI can show
/// distance/direction to each node.
/// Exposed as a [ChangeNotifier] so screens can just
/// `context.watch<BleService>()`.
class BleService extends ChangeNotifier {
  JacketConnectionState connectionState = JacketConnectionState.disconnected;
  String statusMessage = 'Click Scan to search for Smart Jacket devices';

  List<ScanResult> scanResults = [];
  BluetoothDevice? connectedDevice;
  int rssi = 0;

  /// All LoRa nodes heard from since connecting, keyed by node name.
  final Map<String, LoraNode> nodes = {};

  /// Which node's telemetry is shown in the "All LoRa Readings" detail
  /// cards (health metrics, device info, messaging default). Location
  /// tracking always shows every node regardless of this. Falls back
  /// to the first known node when unset or when the selected node has
  /// dropped out.
  String? selectedNodeName;

  /// Which node is *your own* jacket, pinned independently of
  /// [selectedNodeName] so "My Jacket" keeps showing your vitals even
  /// while you're browsing someone else's data. Auto-detected in
  /// [_nodeFor] the moment a node name is heard that matches the
  /// currently-connected BLE device's own advertised name (see
  /// [_autoDetectMyNode]) - that's always the wearer's own jacket,
  /// since it's the one the phone is physically linked to. Manual
  /// picking via [setMyNode] remains available as a fallback for when
  /// detection can't find a match.
  String? myNodeName;

  final List<JacketMessage> messages = [];

  /// Last time each chat room was viewed, keyed by room target (null =
  /// the broadcast room). Drives unread badges and whether a system
  /// notification should pop for a new incoming message.
  final Map<String?, DateTime> _lastReadAt = {};

  /// Whether a [ChatRoomScreen] is currently on screen, and if so,
  /// which room - so an incoming message for that exact room doesn't
  /// also pop a redundant system notification.
  bool _isChatRoomOpen = false;
  String? _openRoomTarget;

  double? myLatitude;
  double? myLongitude;
  bool gpsEnabled = false;
  bool bluetoothEnabled = false;

  BluetoothCharacteristic? _textChar;

  StreamSubscription<List<ScanResult>>? _scanSub;
  StreamSubscription<BluetoothConnectionState>? _connectionSub;
  final List<StreamSubscription<List<int>>> _notifySubs = [];
  Timer? _rssiTimer;
  StreamSubscription<Position>? _positionSub;
  StreamSubscription<ServiceStatus>? _gpsStatusSub;
  StreamSubscription<BluetoothAdapterState>? _adapterStateSub;

  BleService() {
    // Platform channels for these plugins aren't available under
    // `flutter test` or on unsupported desktop targets - guard against
    // that so the app (and widget tests) can still run without BLE/GPS.
    try {
      _adapterStateSub = FlutterBluePlus.adapterState.listen((state) {
        bluetoothEnabled = state == BluetoothAdapterState.on;
        notifyListeners();
      }, onError: (_) {});
    } catch (_) {}
    try {
      _gpsStatusSub = Geolocator.getServiceStatusStream().listen((status) {
        gpsEnabled = status == ServiceStatus.enabled;
        notifyListeners();
      }, onError: (_) {});
    } catch (_) {}
    Geolocator.isLocationServiceEnabled().then((enabled) {
      gpsEnabled = enabled;
      notifyListeners();
    }).catchError((_) {});
    unawaited(_loadMessages());
  }

  /// Loads persisted chat history so it survives app restarts - chat
  /// should only ever go away if the wearer explicitly deletes it.
  Future<void> _loadMessages() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final raw = prefs.getString(_messagesPrefsKey);
      if (raw == null) return;
      final decoded = jsonDecode(raw) as List;
      messages.addAll(
        decoded.map((e) => JacketMessage.fromJson(e as Map<String, dynamic>)),
      );
      notifyListeners();
    } catch (e) {
      debugPrint('[BLE Manager] Failed to load saved chat history: $e');
    }
  }

  Future<void> _persistMessages() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString(
        _messagesPrefsKey,
        jsonEncode(messages.map((m) => m.toJson()).toList()),
      );
    } catch (e) {
      debugPrint('[BLE Manager] Failed to save chat history: $e');
    }
  }

  int get deviceCount => scanResults.length;

  bool get hasMyFix => myLatitude != null && myLongitude != null;

  /// Nodes sorted by name, for stable dropdown/list ordering.
  List<LoraNode> get nodeList =>
      nodes.values.toList()..sort((a, b) => a.name.compareTo(b.name));

  /// Nodes other than [myNodeName], for the "All LoRa Readings" card -
  /// the wearer's own jacket already has its own "My Jacket" card, so
  /// browsing here should only ever offer everyone else's.
  List<LoraNode> get otherNodeList =>
      nodeList.where((n) => n.name != myNodeName).toList();

  /// The node whose telemetry the "All LoRa Readings" detail card
  /// should show - always excludes [myNodeName].
  LoraNode? get selectedNode {
    final selected = selectedNodeName;
    if (selected != null && selected != myNodeName && nodes.containsKey(selected)) {
      return nodes[selected];
    }
    final others = otherNodeList;
    return others.isEmpty ? null : others.first;
  }

  void selectNode(String name) {
    selectedNodeName = name;
    notifyListeners();
  }

  /// The node pinned as "My Jacket", or null until the wearer picks
  /// one (or it drops out of [nodes]).
  LoraNode? get myNode {
    final name = myNodeName;
    return name != null ? nodes[name] : null;
  }

  void setMyNode(String name) {
    myNodeName = name;
    notifyListeners();
  }

  /// Marks [roomTarget] (null = broadcast room) as read as of now,
  /// clearing its unread badge.
  void markRoomRead(String? roomTarget) {
    _lastReadAt[roomTarget] = DateTime.now();
    notifyListeners();
  }

  /// Number of unread incoming messages in [roomTarget]. Only incoming
  /// messages count - the broadcast room never has any (every incoming
  /// message names a specific sending node), so it's always 0.
  int unreadCountForRoom(String? roomTarget) {
    if (roomTarget == null) return 0;
    final since = _lastReadAt[roomTarget];
    return messages.where((m) {
      if (!m.fromJacket || m.nodeName != roomTarget) return false;
      return since == null || m.timestamp.isAfter(since);
    }).length;
  }

  /// Total unread messages across every node's room, for the Chat tab
  /// badge.
  int get totalUnreadCount =>
      nodes.keys.fold<int>(0, (sum, name) => sum + unreadCountForRoom(name));

  /// Called by [ChatRoomScreen] while it's the active screen, so
  /// incoming messages for that room don't also pop a system
  /// notification on top of what's already visible on screen.
  void setActiveRoom(String? roomTarget) {
    _isChatRoomOpen = true;
    _openRoomTarget = roomTarget;
  }

  void clearActiveRoom() {
    _isChatRoomOpen = false;
    _openRoomTarget = null;
  }

  /// Deletes a single message (long-press in a chat room), like WA's
  /// "Delete message".
  void deleteMessage(JacketMessage message) {
    messages.remove(message);
    notifyListeners();
    unawaited(_persistMessages());
  }

  /// Deletes every message belonging to [roomTarget] (null = the
  /// broadcast room) at once, like WA's "Delete chat"/"Clear chat".
  void deleteRoom(String? roomTarget) {
    messages.removeWhere((m) {
      if (roomTarget == null) return !m.fromJacket && m.isBroadcast;
      return m.nodeName == roomTarget;
    });
    _lastReadAt.remove(roomTarget);
    notifyListeners();
    unawaited(_persistMessages());
  }

  LoraNode _nodeFor(String name) {
    final node = nodes.putIfAbsent(name, () => LoraNode(name: name));
    _autoDetectMyNode(name);
    return node;
  }

  /// Pins [name] as "My Jacket" ([myNodeName]) the first time a node
  /// name is heard that matches the currently-connected BLE device's
  /// own advertised name (e.g. device "JKT-BLE-ESP32-DEV" reporting
  /// telemetry as node "ESP32-DEV") - that node is always the wearer's
  /// own jacket, since it's the one the phone is physically linked to
  /// over Bluetooth. No-ops once [myNodeName] is set, whether by this
  /// or by [setMyNode].
  void _autoDetectMyNode(String name) {
    if (myNodeName != null) return;
    final deviceName = connectedDevice?.platformName ?? '';
    if (deviceName.isEmpty) return;
    if (deviceName.toLowerCase().contains(name.toLowerCase())) {
      myNodeName = name;
    }
  }

  /// Splits a `"<node>:<payload>"` wire message into its node name and
  /// remaining payload, falling back to [BleUuids.defaultNodeName] when
  /// no node prefix is present (older single-jacket firmware).
  (String, String) _splitNode(String raw) {
    final idx = raw.indexOf(':');
    if (idx <= 0) return (BleUuids.defaultNodeName, raw);
    return (raw.substring(0, idx).trim(), raw.substring(idx + 1));
  }

  Future<void> startTrackingMyLocation() async {
    await _positionSub?.cancel();
    try {
      final position = await Geolocator.getCurrentPosition();
      myLatitude = position.latitude;
      myLongitude = position.longitude;
      notifyListeners();
    } catch (_) {
      // no fix yet, live stream below will fill it in
    }
    _positionSub = Geolocator.getPositionStream(
      locationSettings: const LocationSettings(distanceFilter: 5),
    ).listen((position) {
      myLatitude = position.latitude;
      myLongitude = position.longitude;
      notifyListeners();
    }, onError: (_) {
      // The position stream can error out transiently (e.g. while the
      // OS "location accuracy" dialog is up); back off briefly and
      // restart rather than leaving the phone's own fix stuck forever.
      Future.delayed(const Duration(seconds: 3), startTrackingMyLocation);
    });
  }

  Future<void> startScan() async {
    if (!bluetoothEnabled) {
      statusMessage = 'Turning on Bluetooth...';
      notifyListeners();
      try {
        // No-ops immediately if Bluetooth is already on; otherwise
        // prompts the user to enable it (Android only - iOS doesn't
        // allow apps to toggle Bluetooth programmatically) and waits
        // until the adapter actually comes on.
        await FlutterBluePlus.turnOn();
      } catch (e) {
        connectionState = JacketConnectionState.disconnected;
        statusMessage = 'Please turn on Bluetooth to scan';
        notifyListeners();
        return;
      }
    }

    scanResults = [];
    connectionState = JacketConnectionState.scanning;
    statusMessage = 'Scanning...';
    notifyListeners();

    await _scanSub?.cancel();
    _scanSub = FlutterBluePlus.scanResults.listen((results) {
      // TEMPORARY: showing every named BLE device (not filtered to
      // BleUuids.deviceNameFilter) because real jacket firmware names
      // seen in the field (e.g. "JKT-BLE-ESP32-DEV") don't all match
      // that substring. Once the actual naming convention across all
      // nodes is confirmed, restore the name filter below.
      scanResults = results
          .where((r) =>
              r.device.platformName.isNotEmpty ||
              r.advertisementData.advName.isNotEmpty)
          .toList()
        ..sort((a, b) => b.rssi.compareTo(a.rssi));
      notifyListeners();
    });

    await FlutterBluePlus.startScan(timeout: const Duration(seconds: 12));
    await FlutterBluePlus.isScanning.where((s) => s == false).first;

    if (connectionState == JacketConnectionState.scanning) {
      connectionState = JacketConnectionState.disconnected;
      statusMessage = scanResults.isEmpty
          ? 'No Smart Jacket devices found nearby'
          : 'Click Scan to search for Smart Jacket devices';
      notifyListeners();
    }
  }

  Future<void> stopScan() async {
    await FlutterBluePlus.stopScan();
    await _scanSub?.cancel();
  }

  Future<bool> connect(BluetoothDevice device) async {
    await stopScan();
    connectionState = JacketConnectionState.connecting;
    statusMessage = 'Connecting...';
    notifyListeners();

    try {
      await device.connect(timeout: const Duration(seconds: 15));
      connectedDevice = device;

      await _connectionSub?.cancel();
      _connectionSub = device.connectionState.listen((state) {
        if (state == BluetoothConnectionState.disconnected) {
          _handleDisconnect();
        }
      });

      // Default BLE ATT MTU only fits ~20 bytes per write. Chat text
      // (up to BleUuids.maxChatTextLength plus the node-name prefix)
      // regularly exceeds that, and several ESP32 BLE stacks reset the
      // GATT connection outright when they receive a write that
      // doesn't fit the negotiated MTU - which shows up as Bluetooth
      // suddenly dropping mid-chat. iOS negotiates its own (larger)
      // MTU automatically and doesn't allow requesting one explicitly.
      if (Platform.isAndroid) {
        try {
          await device.requestMtu(517);
        } catch (e) {
          debugPrint('[BLE Manager] MTU request failed: $e');
        }
        try {
          // The jacket's firmware briefly stalls the BLE stack while it
          // relays a chat message out over LoRa; a shorter connection
          // interval gives Android more chances to hear from it within
          // that stall so it doesn't trip the link's supervision
          // timeout and drop the connection a few seconds after a send.
          await device.requestConnectionPriority(
            connectionPriorityRequest: ConnectionPriority.high,
          );
        } catch (e) {
          debugPrint('[BLE Manager] Connection priority request failed: $e');
        }
      }

      final services = await device.discoverServices();
      _bindCharacteristics(services);
      _startRssiPolling(device);
      unawaited(startTrackingMyLocation());

      connectionState = JacketConnectionState.connected;
      statusMessage = 'Successfully connected to Smart Jacket!';
      notifyListeners();
      return true;
    } catch (e) {
      connectionState = JacketConnectionState.disconnected;
      statusMessage = 'Failed to connect: $e';
      notifyListeners();
      return false;
    }
  }

  void _bindCharacteristics(List<BluetoothService> services) {
    for (final service in services) {
      for (final c in service.characteristics) {
        if (c.uuid == BleUuids.locationCharacteristic) {
          _subscribe(c, _onLocationData);
        } else if (c.uuid == BleUuids.temperatureCharacteristic) {
          _subscribe(c, _onTemperatureData);
        } else if (c.uuid == BleUuids.textCharacteristic) {
          _textChar = c;
          _subscribe(c, _onTextData);
        } else if (c.uuid == BleUuids.vitalsCharacteristic) {
          _subscribe(c, _onVitalsData);
        }
      }
    }
  }

  void _subscribe(
    BluetoothCharacteristic c,
    void Function(List<int> value) onData,
  ) {
    if (!c.properties.notify && !c.properties.indicate) return;
    c.setNotifyValue(true);
    final sub = c.onValueReceived.listen(onData, onError: (_) {});
    _notifySubs.add(sub);
  }

  // The firmware (Jacket_reciver/receiver_arduino/ble_server.cpp) relays
  // every LoRa node onto the same four characteristics, prefixed with
  // the sending/target node's name (see BleUuids for the full format):
  //   location:    "<node>:<lat>,<lon>"
  //   temperature: "<node>:Temp:<celsius>"
  //   vitals:      "<node>:HR:<bpm>,SpO2:<pct>"
  //   text:        "<node>:Text:<message>"

  void _onLocationData(List<int> value) {
    try {
      final (nodeName, payload) = _splitNode(utf8.decode(value).trim());
      final parts = payload.split(',');
      final lat = double.parse(parts[0].trim());
      final lon = double.parse(parts[1].trim());
      // The jacket's GPS module can emit a stale/partial fix (e.g. while
      // still acquiring satellites) that parses fine but is out of
      // range - passing that straight to flutter_map crashes the
      // Location Tracking card, so drop it and keep the last good fix.
      if (lat < -90 || lat > 90 || lon < -180 || lon > 180) {
        debugPrint('[BLE Manager] Ignoring out-of-range location from $nodeName: $lat, $lon');
        return;
      }
      final node = _nodeFor(nodeName);
      node.latitude = lat;
      node.longitude = lon;
      node.lastUpdate = DateTime.now();
      debugPrint('[BLE Manager] Location update - $nodeName: ${node.latitude}, ${node.longitude}');
      notifyListeners();
    } catch (e) {
      debugPrint('[BLE Manager] Failed to parse location: $e');
    }
  }

  void _onTemperatureData(List<int> value) {
    try {
      final (nodeName, payload) = _splitNode(utf8.decode(value).trim());
      final numeric = payload.contains(':') ? payload.split(':').last : payload;
      final node = _nodeFor(nodeName);
      node.temperatureCelsius = double.parse(numeric.trim());
      node.lastUpdate = DateTime.now();
      debugPrint('[BLE Manager] Temperature update - $nodeName: ${node.temperatureCelsius}');
      notifyListeners();
    } catch (e) {
      debugPrint('[BLE Manager] Failed to parse temperature: $e');
    }
  }

  void _onVitalsData(List<int> value) {
    try {
      final (nodeName, payload) = _splitNode(utf8.decode(value).trim());
      final node = _nodeFor(nodeName);
      for (final field in payload.split(',')) {
        final kv = field.split(':');
        if (kv.length != 2) continue;
        final value = double.tryParse(kv[1].trim());
        if (value == null) continue;
        switch (kv[0].trim().toLowerCase()) {
          case 'hr':
            node.heartRateBpm = value;
          case 'spo2':
            node.spo2Percent = value;
        }
      }
      node.lastUpdate = DateTime.now();
      debugPrint('[BLE Manager] Vitals update - $nodeName: HR ${node.heartRateBpm}, SpO2 ${node.spo2Percent}');
      notifyListeners();
    } catch (e) {
      debugPrint('[BLE Manager] Failed to parse vitals: $e');
    }
  }

  void _onTextData(List<int> value) {
    try {
      final (nodeName, payload) = _splitNode(utf8.decode(value));
      final text = payload.startsWith('Text:') ? payload.substring(5) : payload;
      messages.add(JacketMessage(text: text, fromJacket: true, nodeName: nodeName));
      _nodeFor(nodeName).lastUpdate = DateTime.now();
      debugPrint('[BLE Manager] Text update - $nodeName: $text');
      if (!(_isChatRoomOpen && _openRoomTarget == nodeName)) {
        NotificationService.showMessage(sender: nodeName, text: text);
      }
      notifyListeners();
      unawaited(_persistMessages());
    } catch (e) {
      debugPrint('[BLE Manager] Failed to parse text: $e');
    }
  }

  /// Sends [text] to [targetNode], or broadcasts it to every LoRa node
  /// when [targetNode] is null.
  Future<void> sendMessage(String text, {String? targetNode}) async {
    if (_textChar == null || text.trim().isEmpty) return;
    final target = targetNode ?? JacketMessage.broadcastTarget;
    final truncated = text.length > BleUuids.maxChatTextLength
        ? text.substring(0, BleUuids.maxChatTextLength)
        : text;
    try {
      await _textChar!.write(utf8.encode('$target:$truncated'), withoutResponse: false);
      messages.add(JacketMessage(text: truncated, fromJacket: false, nodeName: target));
      debugPrint('[BLE Manager] Text sent to $target: "$truncated"');
      notifyListeners();
      unawaited(_persistMessages());
    } catch (e) {
      debugPrint('[BLE Manager] Failed to send text: $e');
    }
  }

  void _startRssiPolling(BluetoothDevice device) {
    _rssiTimer?.cancel();
    _rssiTimer = Timer.periodic(const Duration(seconds: 3), (_) async {
      try {
        rssi = await device.readRssi();
        notifyListeners();
      } catch (_) {
        // RSSI read failed for device - ignore, will retry next tick
      }
    });
  }

  void _handleDisconnect() {
    _rssiTimer?.cancel();
    _positionSub?.cancel();
    for (final s in _notifySubs) {
      s.cancel();
    }
    _notifySubs.clear();
    _textChar = null;
    connectedDevice = null;
    nodes.clear();
    selectedNodeName = null;
    myNodeName = null;
    _lastReadAt.clear();
    connectionState = JacketConnectionState.disconnected;
    statusMessage = 'Click Scan to search for Smart Jacket devices';
    notifyListeners();
  }

  Future<void> disconnect() async {
    final device = connectedDevice;
    if (device != null) {
      await device.disconnect();
    }
    _handleDisconnect();
  }

  @override
  void dispose() {
    _scanSub?.cancel();
    _connectionSub?.cancel();
    _rssiTimer?.cancel();
    _positionSub?.cancel();
    _gpsStatusSub?.cancel();
    _adapterStateSub?.cancel();
    for (final s in _notifySubs) {
      s.cancel();
    }
    super.dispose();
  }
}
