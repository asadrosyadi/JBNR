import 'package:flutter_local_notifications/flutter_local_notifications.dart';

/// Pops a system notification (heads-up banner, like WhatsApp/SMS) for
/// incoming LoRa chat messages so they're noticed even when the app
/// isn't in the foreground or the Chat tab isn't open.
class NotificationService {
  NotificationService._();

  static final _plugin = FlutterLocalNotificationsPlugin();
  static bool _initialized = false;

  static const _androidChannel = AndroidNotificationDetails(
    'lora_chat_messages',
    'Pesan LoRa',
    channelDescription: 'Notifikasi pesan masuk dari LoRa',
    importance: Importance.high,
    priority: Priority.high,
  );

  static Future<void> init() async {
    if (_initialized) return;
    _initialized = true;

    try {
      const androidSettings = AndroidInitializationSettings('@mipmap/ic_launcher');
      const iosSettings = DarwinInitializationSettings();
      await _plugin.initialize(
        const InitializationSettings(android: androidSettings, iOS: iosSettings),
      );

      await _plugin
          .resolvePlatformSpecificImplementation<AndroidFlutterLocalNotificationsPlugin>()
          ?.requestNotificationsPermission();
      await _plugin
          .resolvePlatformSpecificImplementation<IOSFlutterLocalNotificationsPlugin>()
          ?.requestPermissions(alert: true, badge: true, sound: true);
    } catch (_) {
      // Notification setup can fail on unsupported/desktop test targets -
      // the app should keep working without popups in that case.
    }
  }

  static Future<void> showMessage({required String sender, required String text}) async {
    try {
      await _plugin.show(
        DateTime.now().millisecondsSinceEpoch.remainder(1 << 31),
        sender,
        text,
        const NotificationDetails(android: _androidChannel, iOS: DarwinNotificationDetails()),
      );
    } catch (_) {
      // Ignore - a missed popup shouldn't crash message handling.
    }
  }
}
