import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/jacket_message.dart';
import '../screens/chat_room_screen.dart';
import '../services/ble_service.dart';
import '../theme/app_colors.dart';

/// Chat tab: a room list, like WhatsApp's chat list - one room for
/// broadcasting to every LoRa node, and a separate personal room per
/// node, so the two never mix in the same timeline. Tapping a room
/// opens [ChatRoomScreen] for that conversation.
class ChatTab extends StatelessWidget {
  const ChatTab({super.key});

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final messages = ble.messages;
    final nodeNames = ble.nodeList.map((n) => n.name).toList();

    final broadcastMessages = messages
        .where((m) => !m.fromJacket && m.isBroadcast)
        .toList();

    return Container(
      color: AppColors.background,
      child: ListView(
        children: [
          _RoomTile(
            icon: Icons.campaign,
            iconColor: AppColors.warning,
            title: 'Semua LoRa',
            emptySubtitle: 'Broadcast ke semua node',
            messages: broadcastMessages,
            unreadCount: ble.unreadCountForRoom(null),
            onTap: () => Navigator.of(context).push(
              MaterialPageRoute(
                builder: (_) => const ChatRoomScreen(
                  roomTarget: null,
                  roomLabel: 'Semua LoRa',
                ),
              ),
            ),
            onLongPress: broadcastMessages.isEmpty
                ? null
                : () => _confirmDeleteRoom(
                      context,
                      ble,
                      roomTarget: null,
                      roomLabel: 'Semua LoRa',
                    ),
          ),
          const Divider(height: 1),
          for (final name in nodeNames) ...[
            _RoomTile(
              icon: Icons.person,
              iconColor: AppColors.accentBlue,
              title: name,
              emptySubtitle: 'Chat personal',
              messages: messages.where((m) => m.nodeName == name).toList(),
              unreadCount: ble.unreadCountForRoom(name),
              onTap: () => Navigator.of(context).push(
                MaterialPageRoute(
                  builder: (_) => ChatRoomScreen(roomTarget: name, roomLabel: name),
                ),
              ),
              onLongPress: () => _confirmDeleteRoom(
                context,
                ble,
                roomTarget: name,
                roomLabel: name,
              ),
            ),
            const Divider(height: 1),
          ],
          if (nodeNames.isEmpty)
            const Padding(
              padding: EdgeInsets.all(24),
              child: Text(
                'Belum ada LoRa yang terhubung untuk diajak chat personal',
                textAlign: TextAlign.center,
                style: TextStyle(color: AppColors.muted, fontSize: 13),
              ),
            ),
        ],
      ),
    );
  }
}

/// Confirms, then deletes every message in a room at once - like WA's
/// "Delete chat".
Future<void> _confirmDeleteRoom(
  BuildContext context,
  BleService ble, {
  required String? roomTarget,
  required String roomLabel,
}) async {
  final confirmed = await showDialog<bool>(
    context: context,
    builder: (ctx) => AlertDialog(
      title: const Text('Hapus Percakapan'),
      content: Text('Semua pesan di percakapan "$roomLabel" akan dihapus. Lanjutkan?'),
      actions: [
        TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Batal')),
        TextButton(
          onPressed: () => Navigator.pop(ctx, true),
          child: const Text('Hapus', style: TextStyle(color: AppColors.danger)),
        ),
      ],
    ),
  );
  if (confirmed == true) {
    ble.deleteRoom(roomTarget);
  }
}

class _RoomTile extends StatelessWidget {
  final IconData icon;
  final Color iconColor;
  final String title;
  final String emptySubtitle;
  final List<JacketMessage> messages;
  final int unreadCount;
  final VoidCallback onTap;
  final VoidCallback? onLongPress;

  const _RoomTile({
    required this.icon,
    required this.iconColor,
    required this.title,
    required this.emptySubtitle,
    required this.messages,
    required this.unreadCount,
    required this.onTap,
    required this.onLongPress,
  });

  @override
  Widget build(BuildContext context) {
    final last = messages.isNotEmpty ? messages.last : null;
    final hasUnread = unreadCount > 0;

    return ListTile(
      onTap: onTap,
      onLongPress: onLongPress,
      leading: CircleAvatar(
        backgroundColor: iconColor.withValues(alpha: 0.15),
        child: Icon(icon, color: iconColor),
      ),
      title: Text(
        title,
        style: TextStyle(fontWeight: hasUnread ? FontWeight.bold : FontWeight.w600),
      ),
      subtitle: Text(
        last?.text ?? emptySubtitle,
        maxLines: 1,
        overflow: TextOverflow.ellipsis,
        style: TextStyle(
          color: hasUnread ? AppColors.primaryDark : AppColors.muted,
          fontWeight: hasUnread ? FontWeight.w600 : FontWeight.normal,
          fontSize: 13,
        ),
      ),
      trailing: last == null
          ? null
          : Column(
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.end,
              children: [
                Text(
                  '${last.timestamp.hour.toString().padLeft(2, '0')}:'
                  '${last.timestamp.minute.toString().padLeft(2, '0')}',
                  style: TextStyle(
                    fontSize: 11,
                    color: hasUnread ? AppColors.statusActive : AppColors.muted,
                    fontWeight: hasUnread ? FontWeight.bold : FontWeight.normal,
                  ),
                ),
                if (hasUnread) ...[
                  const SizedBox(height: 4),
                  CircleAvatar(
                    radius: 9,
                    backgroundColor: AppColors.statusActive,
                    child: Text(
                      '$unreadCount',
                      style: const TextStyle(fontSize: 10, color: Colors.white),
                    ),
                  ),
                ],
              ],
            ),
    );
  }
}
