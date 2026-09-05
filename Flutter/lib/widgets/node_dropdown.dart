import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../models/lora_node.dart';
import '../services/locale_service.dart';
import '../theme/app_colors.dart';

class NodeDropdown extends StatelessWidget {
  final List<LoraNode> nodes;
  final String? value;
  final String? hint;
  final ValueChanged<String> onChanged;

  const NodeDropdown({
    super.key,
    required this.nodes,
    required this.value,
    required this.onChanged,
    this.hint,
  });

  @override
  Widget build(BuildContext context) {
    final s = Strings(context.watch<LocaleService>().language);
    if (nodes.isEmpty) {
      return Text(
        s.noLoraYet,
        style: const TextStyle(fontSize: 12, color: AppColors.muted),
      );
    }

    final current = nodes.any((n) => n.name == value) ? value : null;

    return ConstrainedBox(
      constraints: const BoxConstraints(maxWidth: 130),
      child: Container(
        padding: const EdgeInsets.symmetric(horizontal: 10),
        decoration: BoxDecoration(
          color: AppColors.background,
          borderRadius: BorderRadius.circular(20),
        ),
        child: DropdownButtonHideUnderline(
          child: DropdownButton<String>(
            value: current,
            isExpanded: true,
            hint: Text(
              hint ?? s.select,
              overflow: TextOverflow.ellipsis,
              style: const TextStyle(fontSize: 12),
            ),
            items: [
              for (final node in nodes)
                DropdownMenuItem(
                  value: node.name,
                  child: Text(
                    node.name,
                    overflow: TextOverflow.ellipsis,
                    style: const TextStyle(fontSize: 13),
                  ),
                ),
            ],
            onChanged: (v) {
              if (v != null) onChanged(v);
            },
          ),
        ),
      ),
    );
  }
}
