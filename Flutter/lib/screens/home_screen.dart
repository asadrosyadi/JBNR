import 'dart:async';

import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../services/ble_service.dart';
import '../services/locale_service.dart';
import '../services/permission_service.dart';
import '../theme/app_colors.dart';
import '../widgets/app_header.dart';
import '../widgets/chat_tab.dart';
import '../widgets/health_metrics_card.dart';
import '../widgets/location_coordinates_card.dart';
import '../widgets/location_tracking_card.dart';
import '../widgets/node_dropdown.dart';
import '../widgets/settings_tab.dart';
import '../widgets/status_bar.dart';

/// Single continuously-updating home screen: "Beranda" (scan/connect,
/// telemetry) and "Chat" as swipeable top tabs, the same way WhatsApp
/// switches between Chats/Status/Calls.
class HomeScreen extends StatefulWidget {
  const HomeScreen({super.key});

  @override
  State<HomeScreen> createState() => _HomeScreenState();
}

class _HomeScreenState extends State<HomeScreen> {
  @override
  void initState() {
    super.initState();
    WidgetsBinding.instance.addPostFrameCallback((_) async {
      // The phone's own GPS position on Location Tracking shouldn't
      // have to wait for a Bluetooth scan/connect - ask for Location
      // permission right away so it can start populating immediately.
      if (!mounted) return;
      final ready = await PermissionService.ensureLocationReady(context);
      if (!ready || !mounted) return;
      context.read<BleService>().startTrackingMyLocation();
    });
  }

  Future<void> _handleScan(BuildContext context) async {
    final ble = context.read<BleService>();
    final ready = await PermissionService.ensureReady(context);
    if (!ready) return;
    unawaited(ble.startTrackingMyLocation());
    await ble.startScan();
  }

  Future<void> _handleConnect(
    BuildContext context,
    BluetoothDevice device,
  ) async {
    final ble = context.read<BleService>();
    final s = Strings(context.read<LocaleService>().language);
    final connected = await ble.connect(device);
    if (!context.mounted) return;
    if (connected) {
      await showDialog<void>(
        context: context,
        builder: (ctx) => AlertDialog(
          title: Text(s.connectedTitle),
          content: Text(s.connectedBody),
          actions: [
            TextButton(
              onPressed: () => Navigator.of(ctx).pop(),
              child: Text(s.ok),
            ),
          ],
        ),
      );
    }
  }

  Future<void> _handleDisconnect(BuildContext context) async {
    final ble = context.read<BleService>();
    final s = Strings(context.read<LocaleService>().language);
    await ble.disconnect();
    if (!context.mounted) return;
    await showDialog<void>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(s.disconnectedTitle),
        content: Text(s.disconnectedBody),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: Text(s.ok),
          ),
        ],
      ),
    );
  }

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final s = Strings(context.watch<LocaleService>().language);
    final isConnected = ble.connectionState == JacketConnectionState.connected;
    final isConnecting =
        ble.connectionState == JacketConnectionState.connecting;
    final isScanning = ble.connectionState == JacketConnectionState.scanning;
    final unreadCount = ble.totalUnreadCount;

    return DefaultTabController(
      length: 3,
      initialIndex: 1,
      child: Scaffold(
        backgroundColor: AppColors.background,
        body: SafeArea(
          child: Column(
            children: [
              Container(
                decoration: const BoxDecoration(
                  gradient: LinearGradient(
                    colors: [AppColors.accentBlue, AppColors.primaryDark],
                    begin: Alignment.topCenter,
                    end: Alignment.bottomCenter,
                  ),
                ),
                child: Column(
                  children: [
                    AppHeader(onBluetoothTap: () => _handleScan(context)),
                    StatusBar(
                      connected: isConnected,
                      connecting: isConnecting,
                      gpsActive: ble.gpsEnabled,
                      deviceCount: ble.deviceCount,
                    ),
                    TabBar(
                      labelColor: Colors.white,
                      unselectedLabelColor: Colors.white70,
                      indicatorColor: Colors.white,
                      tabs: [
                        Tab(text: s.tabSettings),
                        Tab(text: s.tabHome),
                        Tab(
                          child: Row(
                            mainAxisSize: MainAxisSize.min,
                            children: [
                              Text(s.tabChat),
                              if (unreadCount > 0) ...[
                                const SizedBox(width: 6),
                                CircleAvatar(
                                  radius: 9,
                                  backgroundColor: Colors.white,
                                  child: Text(
                                    '$unreadCount',
                                    style: const TextStyle(
                                      fontSize: 10,
                                      color: AppColors.primaryDark,
                                    ),
                                  ),
                                ),
                              ],
                            ],
                          ),
                        ),
                      ],
                    ),
                  ],
                ),
              ),
              Expanded(
                child: TabBarView(
                  children: [
                    SettingsTab(
                      isConnected: isConnected,
                      isScanning: isScanning,
                      isConnecting: isConnecting,
                      scanResults: ble.scanResults,
                      onScan: () => _handleScan(context),
                      onConnect: (device) => _handleConnect(context, device),
                      deviceName: ble.connectedDevice?.platformName.isNotEmpty == true
                          ? ble.connectedDevice!.platformName
                          : 'ESP-BLE-RECIVER',
                      deviceId: ble.connectedDevice?.remoteId.str ?? '--',
                      onDisconnect: () => _handleDisconnect(context),
                      bluetoothEnabled: ble.bluetoothEnabled,
                      gpsActive: ble.gpsEnabled,
                    ),
                    ListView(
                      padding: const EdgeInsets.all(16),
                      children: [
                        if (isConnected) ...[
                          Container(
                            width: double.infinity,
                            padding: const EdgeInsets.symmetric(vertical: 10),
                            decoration: BoxDecoration(
                              color: AppColors.statusActive,
                              borderRadius: BorderRadius.circular(10),
                            ),
                            child: Row(
                              mainAxisAlignment: MainAxisAlignment.center,
                              children: [
                                const Icon(
                                  Icons.check_circle,
                                  color: Colors.white,
                                  size: 18,
                                ),
                                const SizedBox(width: 8),
                                Text(
                                  s.connectedBanner,
                                  style: const TextStyle(
                                    color: Colors.white,
                                    fontWeight: FontWeight.w600,
                                  ),
                                ),
                              ],
                            ),
                          ),
                          const SizedBox(height: 16),
                        ],
                        LocationTrackingCard(
                          hasMyFix: ble.hasMyFix,
                          myLatitude: ble.myLatitude,
                          myLongitude: ble.myLongitude,
                          nodes: ble.nodeList,
                          isConnected: isConnected,
                          isLoading: isScanning || isConnecting,
                          onRefresh: () => ble.startTrackingMyLocation(),
                        ),
                        const SizedBox(height: 16),
                        if (isConnected) ...[
                          HealthMetricsCard(
                            title: s.myJacket,
                            trailing: NodeDropdown(
                              nodes: ble.nodeList,
                              value: ble.myNodeName,
                              hint: s.pickYours,
                              onChanged: ble.setMyNode,
                            ),
                            heartRate: ble.myNode?.heartRateBpm,
                            spo2: ble.myNode?.spo2Percent,
                            temperature: ble.myNode?.temperatureCelsius,
                          ),
                          const SizedBox(height: 16),
                          HealthMetricsCard(
                            title: s.allLoraReadings,
                            trailing: NodeDropdown(
                              nodes: ble.otherNodeList,
                              value: ble.selectedNode?.name,
                              hint: s.pickNode,
                              onChanged: ble.selectNode,
                            ),
                            heartRate: ble.selectedNode?.heartRateBpm,
                            spo2: ble.selectedNode?.spo2Percent,
                            temperature: ble.selectedNode?.temperatureCelsius,
                          ),
                          const SizedBox(height: 16),
                        ],
                        LocationCoordinatesCard(
                          myLatitude: ble.myLatitude,
                          myLongitude: ble.myLongitude,
                          gpsActive: ble.gpsEnabled,
                          nodes: ble.nodeList,
                        ),
                      ],
                    ),
                    const ChatTab(),
                  ],
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
