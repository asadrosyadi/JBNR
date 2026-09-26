class JacketMessage {
  static const String broadcastTarget = 'ALL';

  final String text;
  final bool fromJacket;

  final String nodeName;

  /// True for a message that arrived addressed to everyone rather than to
  /// this jacket. [nodeName] is still the sender, so the broadcast room can
  /// show who wrote it, but it must not land in that sender's personal room.
  final bool viaBroadcast;
  final DateTime timestamp;

  JacketMessage({
    required this.text,
    required this.fromJacket,
    required this.nodeName,
    this.viaBroadcast = false,
    DateTime? timestamp,
  }) : timestamp = timestamp ?? DateTime.now();

  bool get isBroadcast => nodeName == broadcastTarget;

  bool get inBroadcastRoom => viaBroadcast || (!fromJacket && isBroadcast);

  Map<String, dynamic> toJson() => {
        'text': text,
        'fromJacket': fromJacket,
        'nodeName': nodeName,
        'viaBroadcast': viaBroadcast,
        'timestamp': timestamp.toIso8601String(),
      };

  factory JacketMessage.fromJson(Map<String, dynamic> json) => JacketMessage(
        text: json['text'] as String,
        fromJacket: json['fromJacket'] as bool,
        nodeName: json['nodeName'] as String,
        viaBroadcast: json['viaBroadcast'] as bool? ?? false,
        timestamp: DateTime.parse(json['timestamp'] as String),
      );
}
