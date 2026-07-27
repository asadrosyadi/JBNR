import 'package:flutter/material.dart';

import '../models/lora_node.dart';
import '../theme/app_colors.dart';

/// Small pill dropdown to pick a LoRa node by name. Reused as the
/// trailing widget on both the "My Jacket" card (pick which node is
/// yours) and the "All LoRa Readings" card (pick which node's data to
/// browse).
class NodeDropdown extends StatelessWidget {
  final List<LoraNode> nodes;
  final String? value;
  final String hint;
  final ValueChanged<String> onChanged;

  const NodeDropdown({
    super.key,
    required this.nodes,
    required this.value,
    required this.onChanged,
    this.hint = 'Select',
  });

  @override
  Widget build(BuildContext context) {
    if (nodes.isEmpty) {
      return const Text(
        'No LoRa yet',
        style: TextStyle(fontSize: 12, color: AppColors.muted),
      );
    }

    final current = nodes.any((n) => n.name == value) ? value : null;

    return Container(
      padding: const EdgeInsets.symmetric(horizontal: 10),
      decoration: BoxDecoration(
        color: AppColors.background,
        borderRadius: BorderRadius.circular(20),
      ),
      child: DropdownButtonHideUnderline(
        child: DropdownButton<String>(
          value: current,
          hint: Text(hint, style: const TextStyle(fontSize: 12)),
          items: [
            for (final node in nodes)
              DropdownMenuItem(
                value: node.name,
                child: Text(node.name, style: const TextStyle(fontSize: 13)),
              ),
          ],
          onChanged: (v) {
            if (v != null) onChanged(v);
          },
        ),
      ),
    );
  }
}
