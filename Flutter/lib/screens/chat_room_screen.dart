import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../models/jacket_message.dart';
import '../services/ble_service.dart';
import '../services/ble_uuids.dart';
import '../theme/app_colors.dart';

/// A single chat room's conversation - either the broadcast room (sent
/// to every LoRa node at once) or a personal room with one specific
/// node. Kept as its own screen (pushed from the room list in
/// [ChatTab]) so broadcast and personal messages never mix in the same
/// timeline, and so sending never requires re-picking a target - the
/// room you're in already determines it.
class ChatRoomScreen extends StatefulWidget {
  /// null = broadcast room (every LoRa node); otherwise the specific
  /// node this room is a personal chat with.
  final String? roomTarget;
  final String roomLabel;

  const ChatRoomScreen({
    super.key,
    required this.roomTarget,
    required this.roomLabel,
  });

  @override
  State<ChatRoomScreen> createState() => _ChatRoomScreenState();
}

class _ChatRoomScreenState extends State<ChatRoomScreen> {
  final _controller = TextEditingController();
  final _scrollController = ScrollController();

  @override
  void initState() {
    super.initState();
    final ble = context.read<BleService>();
    ble.setActiveRoom(widget.roomTarget);
    ble.markRoomRead(widget.roomTarget);
  }

  @override
  void dispose() {
    context.read<BleService>().clearActiveRoom();
    _controller.dispose();
    _scrollController.dispose();
    super.dispose();
  }

  bool _belongsToRoom(JacketMessage message) {
    if (widget.roomTarget == null) {
      // Broadcast room: only outgoing broadcasts belong here. Every
      // incoming message names a specific sending node, so it always
      // belongs to that node's personal room instead.
      return !message.fromJacket && message.isBroadcast;
    }
    return message.nodeName == widget.roomTarget;
  }

  void _send(BleService ble) {
    final text = _controller.text;
    if (text.trim().isEmpty) return;
    ble.sendMessage(text, targetNode: widget.roomTarget);
    _controller.clear();
    WidgetsBinding.instance.addPostFrameCallback((_) {
      if (!_scrollController.hasClients) return;
      _scrollController.animateTo(
        _scrollController.position.maxScrollExtent,
        duration: const Duration(milliseconds: 200),
        curve: Curves.easeOut,
      );
    });
  }

  Future<void> _confirmDeleteMessage(JacketMessage message) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Hapus Pesan'),
        content: const Text('Hapus pesan ini?'),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Batal')),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Hapus', style: TextStyle(color: AppColors.danger)),
          ),
        ],
      ),
    );
    if (confirmed == true && mounted) {
      context.read<BleService>().deleteMessage(message);
    }
  }

  Future<void> _confirmDeleteRoom() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Hapus Semua Pesan'),
        content: Text(
          'Semua pesan di percakapan "${widget.roomLabel}" akan dihapus. Lanjutkan?',
        ),
        actions: [
          TextButton(onPressed: () => Navigator.pop(ctx, false), child: const Text('Batal')),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Hapus', style: TextStyle(color: AppColors.danger)),
          ),
        ],
      ),
    );
    if (confirmed == true && mounted) {
      context.read<BleService>().deleteRoom(widget.roomTarget);
    }
  }

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final messages = ble.messages.where(_belongsToRoom).toList();

    // Keep marking read while this room stays the active screen, so a
    // message that arrives while the user is actively looking at it
    // never lingers as "unread" once they leave.
    if (ble.unreadCountForRoom(widget.roomTarget) > 0) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        if (mounted) ble.markRoomRead(widget.roomTarget);
      });
    }

    return Scaffold(
      backgroundColor: AppColors.background,
      appBar: AppBar(
        title: Text(widget.roomLabel),
        backgroundColor: AppColors.primaryDark,
        foregroundColor: Colors.white,
        actions: [
          IconButton(
            onPressed: messages.isEmpty ? null : _confirmDeleteRoom,
            tooltip: 'Hapus semua pesan',
            icon: const Icon(Icons.delete_outline),
          ),
        ],
      ),
      body: Column(
        children: [
          Expanded(
            child: messages.isEmpty
                ? const Center(
                    child: Text(
                      'Belum ada pesan',
                      style: TextStyle(color: AppColors.muted),
                    ),
                  )
                : ListView.builder(
                    controller: _scrollController,
                    padding: const EdgeInsets.all(12),
                    itemCount: messages.length,
                    itemBuilder: (context, index) => _MessageBubble(
                      message: messages[index],
                      onLongPress: () => _confirmDeleteMessage(messages[index]),
                    ),
                  ),
          ),
          SafeArea(
            top: false,
            child: Padding(
              padding: const EdgeInsets.all(12),
              child: Container(
                decoration: BoxDecoration(
                  color: Colors.white,
                  borderRadius: BorderRadius.circular(24),
                  border: Border.all(color: AppColors.mutedLight),
                ),
                padding: const EdgeInsets.only(left: 16, right: 4),
                child: Row(
                  children: [
                    Expanded(
                      child: TextField(
                        controller: _controller,
                        maxLength: BleUuids.maxChatTextLength,
                        decoration: InputDecoration(
                          hintText: widget.roomTarget == null
                              ? 'Pesan ke semua LoRa...'
                              : 'Pesan ke ${widget.roomLabel}...',
                          border: InputBorder.none,
                          isDense: true,
                          counterText: '',
                        ),
                        onSubmitted: (_) => _send(ble),
                      ),
                    ),
                    IconButton(
                      onPressed: () => _send(ble),
                      icon: const Icon(Icons.send, color: AppColors.accentBlue),
                    ),
                  ],
                ),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _MessageBubble extends StatelessWidget {
  final JacketMessage message;
  final VoidCallback onLongPress;

  const _MessageBubble({required this.message, required this.onLongPress});

  String get _time {
    final t = message.timestamp;
    return '${t.hour.toString().padLeft(2, '0')}:${t.minute.toString().padLeft(2, '0')}';
  }

  @override
  Widget build(BuildContext context) {
    final isMine = !message.fromJacket;

    return Align(
      alignment: isMine ? Alignment.centerRight : Alignment.centerLeft,
      child: GestureDetector(
        onLongPress: onLongPress,
        child: Container(
          margin: const EdgeInsets.symmetric(vertical: 4),
          padding: const EdgeInsets.symmetric(horizontal: 14, vertical: 10),
          constraints: BoxConstraints(maxWidth: MediaQuery.of(context).size.width * 0.75),
          decoration: BoxDecoration(
            color: isMine ? AppColors.accentBlue : Colors.white,
            borderRadius: BorderRadius.only(
              topLeft: const Radius.circular(14),
              topRight: const Radius.circular(14),
              bottomLeft: Radius.circular(isMine ? 14 : 2),
              bottomRight: Radius.circular(isMine ? 2 : 14),
            ),
          ),
          child: Column(
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              Text(
                message.text,
                style: TextStyle(color: isMine ? Colors.white : AppColors.primaryDark),
              ),
              const SizedBox(height: 2),
              Text(
                _time,
                style: TextStyle(
                  fontSize: 10,
                  color: isMine ? Colors.white70 : AppColors.muted,
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
