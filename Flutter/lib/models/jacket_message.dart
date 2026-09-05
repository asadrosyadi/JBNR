class JacketMessage {
  static const String broadcastTarget = 'ALL';

  final String text;
  final bool fromJacket;

  final String nodeName;
  final DateTime timestamp;

  JacketMessage({
    required this.text,
    required this.fromJacket,
    required this.nodeName,
    DateTime? timestamp,
  }) : timestamp = timestamp ?? DateTime.now();

  bool get isBroadcast => nodeName == broadcastTarget;

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
