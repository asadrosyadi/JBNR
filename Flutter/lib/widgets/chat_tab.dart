import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../models/jacket_message.dart';
import '../screens/chat_room_screen.dart';
import '../services/ble_service.dart';
import '../services/locale_service.dart';
import '../theme/app_colors.dart';

class ChatTab extends StatelessWidget {
  const ChatTab({super.key});

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final s = Strings(context.watch<LocaleService>().language);
    final messages = ble.messages;
    final nodeNames = ble.otherNodeList.map((n) => n.name).toList();

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
            title: s.allLora,
            emptySubtitle: s.broadcastToAllNodes,
            messages: broadcastMessages,
            unreadCount: ble.unreadCountForRoom(null),
            onTap: () => Navigator.of(context).push(
              MaterialPageRoute(
                builder: (_) => ChatRoomScreen(
                  roomTarget: null,
                  roomLabel: s.allLora,
                ),
              ),
            ),
            onLongPress: broadcastMessages.isEmpty
                ? null
                : () => _confirmDeleteRoom(
                      context,
                      ble,
                      s,
                      roomTarget: null,
                      roomLabel: s.allLora,
                    ),
          ),
          const Divider(height: 1),
          for (final name in nodeNames) ...[
            _RoomTile(
              icon: Icons.person,
              iconColor: AppColors.accentBlue,
              title: name,
              emptySubtitle: s.personalChat,
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
                s,
                roomTarget: name,
                roomLabel: name,
              ),
            ),
            const Divider(height: 1),
          ],
          if (nodeNames.isEmpty)
            Padding(
              padding: const EdgeInsets.all(24),
              child: Text(
                s.noLoraConnectedForChat,
                textAlign: TextAlign.center,
                style: const TextStyle(color: AppColors.muted, fontSize: 13),
              ),
            ),
        ],
      ),
    );
  }
}

Future<void> _confirmDeleteRoom(
  BuildContext context,
  BleService ble,
  Strings s, {
  required String? roomTarget,
  required String roomLabel,
}) async {
  final confirmed = await showDialog<bool>(
    context: context,
    builder: (ctx) => AlertDialog(
      title: Text(s.deleteConversationTitle),
      content: Text(s.deleteConversationBody(roomLabel)),
      actions: [
        TextButton(onPressed: () => Navigator.pop(ctx, false), child: Text(s.cancel)),
        TextButton(
          onPressed: () => Navigator.pop(ctx, true),
          child: Text(s.delete, style: const TextStyle(color: AppColors.danger)),
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
