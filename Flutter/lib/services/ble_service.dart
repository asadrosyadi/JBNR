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

class BleService extends ChangeNotifier {
  JacketConnectionState connectionState = JacketConnectionState.disconnected;
  String statusMessage = 'Click Scan to search for Smart Jacket devices';

  List<ScanResult> scanResults = [];
  BluetoothDevice? connectedDevice;
  int rssi = 0;

  final Map<String, LoraNode> nodes = {};

  String? selectedNodeName;

  String? myNodeName;

  final List<JacketMessage> messages = [];

  final Map<String?, DateTime> _lastReadAt = {};

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

  List<LoraNode> get nodeList =>
      nodes.values.toList()..sort((a, b) => a.name.compareTo(b.name));

  List<LoraNode> get otherNodeList =>
      nodeList.where((n) => n.name != myNodeName).toList();

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

  LoraNode? get myNode {
    final name = myNodeName;
    return name != null ? nodes[name] : null;
  }

  void setMyNode(String name) {
    myNodeName = name;
    notifyListeners();
  }

  void markRoomRead(String? roomTarget) {
    _lastReadAt[roomTarget] = DateTime.now();
    notifyListeners();
  }

  int unreadCountForRoom(String? roomTarget) {
    if (roomTarget == null) return 0;
    final since = _lastReadAt[roomTarget];
    return messages.where((m) {
      if (!m.fromJacket || m.nodeName != roomTarget) return false;
      return since == null || m.timestamp.isAfter(since);
    }).length;
  }

  int get totalUnreadCount =>
      nodes.keys.fold<int>(0, (sum, name) => sum + unreadCountForRoom(name));

  void setActiveRoom(String? roomTarget) {
    _isChatRoomOpen = true;
    _openRoomTarget = roomTarget;
  }

  void clearActiveRoom() {
    _isChatRoomOpen = false;
    _openRoomTarget = null;
  }

  void deleteMessage(JacketMessage message) {
    messages.remove(message);
    notifyListeners();
    unawaited(_persistMessages());
  }

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

  void _autoDetectMyNode(String name) {
    if (myNodeName != null) return;
    final deviceName = connectedDevice?.platformName ?? '';
    if (deviceName.isEmpty) return;
    if (deviceName.toLowerCase().contains(name.toLowerCase())) {
      myNodeName = name;
    }
  }

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
    }
    _positionSub = Geolocator.getPositionStream(
      locationSettings: const LocationSettings(distanceFilter: 5),
    ).listen((position) {
      myLatitude = position.latitude;
      myLongitude = position.longitude;
      notifyListeners();
    }, onError: (_) {
      Future.delayed(const Duration(seconds: 3), startTrackingMyLocation);
    });
  }

  Future<void> startScan() async {
    if (!bluetoothEnabled) {
      statusMessage = 'Turning on Bluetooth...';
      notifyListeners();
      try {
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

      if (Platform.isAndroid) {
        try {
          await device.requestMtu(517);
        } catch (e) {
          debugPrint('[BLE Manager] MTU request failed: $e');
        }
        try {
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


  void _onLocationData(List<int> value) {
    try {
      final (nodeName, payload) = _splitNode(utf8.decode(value).trim());
      final parts = payload.split(',');
      final lat = double.parse(parts[0].trim());
      final lon = double.parse(parts[1].trim());
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
