import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../services/locale_service.dart';
import '../theme/app_colors.dart';
import 'section_card.dart';

/// App-wide language picker: Bahasa Indonesia / English. Changing it
/// updates [LocaleService.language], which every screen/widget reads
/// via `Strings(...)` - so this is the one place that flips the whole
/// app's text at once, not just this tab.
class LanguageCard extends StatelessWidget {
  const LanguageCard({super.key});

  @override
  Widget build(BuildContext context) {
    final locale = context.watch<LocaleService>();
    final s = Strings(locale.language);

    return SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.language, color: AppColors.primary, size: 20),
              const SizedBox(width: 8),
              Text(
                s.language,
                style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
            ],
          ),
          const SizedBox(height: 8),
          _LanguageOption(
            label: s.languageIndonesian,
            selected: locale.language == AppLanguage.indonesian,
            onTap: () => locale.setLanguage(AppLanguage.indonesian),
          ),
          _LanguageOption(
            label: s.languageEnglish,
            selected: locale.language == AppLanguage.english,
            onTap: () => locale.setLanguage(AppLanguage.english),
          ),
        ],
      ),
    );
  }
}

class _LanguageOption extends StatelessWidget {
  final String label;
  final bool selected;
  final VoidCallback onTap;

  const _LanguageOption({required this.label, required this.selected, required this.onTap});

  @override
  Widget build(BuildContext context) {
    return InkWell(
      onTap: onTap,
      borderRadius: BorderRadius.circular(12),
      child: Padding(
        padding: const EdgeInsets.symmetric(vertical: 8),
        child: Row(
          children: [
            Icon(
              selected ? Icons.radio_button_checked : Icons.radio_button_unchecked,
              color: selected ? AppColors.accentBlue : AppColors.mutedLight,
              size: 20,
            ),
            const SizedBox(width: 10),
            Text(
              label,
              style: TextStyle(
                fontSize: 14,
                fontWeight: selected ? FontWeight.w600 : FontWeight.normal,
                color: selected ? AppColors.primaryDark : AppColors.muted,
              ),
            ),
          ],
        ),
      ),
    );
  }
}
