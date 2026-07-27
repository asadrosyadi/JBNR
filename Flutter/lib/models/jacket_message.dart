/// A single text message exchanged over the LoRa mesh, relayed through
/// the BLE receiver.
class JacketMessage {
  /// Sent to/from every node.
  static const String broadcastTarget = 'ALL';

  final String text;
  final bool fromJacket;

  /// For an incoming message, the LoRa node that sent it. For an
  /// outgoing message, the node it was addressed to, or
  /// [broadcastTarget] if it was sent to every node.
  final String nodeName;
  final DateTime timestamp;

  JacketMessage({
    required this.text,
    required this.fromJacket,
    required this.nodeName,
    DateTime? timestamp,
  }) : timestamp = timestamp ?? DateTime.now();

  bool get isBroadcast => nodeName == broadcastTarget;

  /// For persisting chat history to disk between app launches.
  Map<String, dynamic> toJson() => {
        'text': text,
        'fromJacket': fromJacket,
        'nodeName': nodeName,
        'timestamp': timestamp.toIso8601String(),
      };

  factory JacketMessage.fromJson(Map<String, dynamic> json) => JacketMessage(
        text: json['text'] as String,
        fromJacket: json['fromJacket'] as bool,
        nodeName: json['nodeName'] as String,
        timestamp: DateTime.parse(json['timestamp'] as String),
      );
}
