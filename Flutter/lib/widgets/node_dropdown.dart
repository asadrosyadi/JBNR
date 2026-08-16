import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../models/lora_node.dart';
import '../services/locale_service.dart';
import '../theme/app_colors.dart';

/// Small pill dropdown to pick a LoRa node by name. Reused as the
/// trailing widget on both the "My Jacket" card (pick which node is
/// yours) and the "All LoRa Readings" card (pick which node's data to
/// browse). [hint] defaults to the localized "Select" when the caller
/// doesn't override it with something more specific.
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

    // Capped width + isExpanded + ellipsis: a node name long enough
    // (e.g. "Jaket-1Fx") could otherwise force this pill wider than the
    // card row has room for, overflowing next to the card's title -
    // this way it truncates instead.
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
