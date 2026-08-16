import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

const _languagePrefsKey = 'app_language_v1';

enum AppLanguage { indonesian, english }

/// Holds the user's chosen UI language, persisted across app restarts.
/// Every screen/widget builds its text from `Strings(language)` (see
/// `l10n/strings.dart`) instead of hardcoding it, and [LanguageCard] in
/// Settings is the only place that calls [setLanguage] - so picking a
/// language there re-localizes the entire app at once via
/// [notifyListeners], not just one screen.
class LocaleService extends ChangeNotifier {
  AppLanguage language = AppLanguage.indonesian;

  LocaleService() {
    unawaited(_load());
  }

  Future<void> _load() async {
    try {
      final prefs = await SharedPreferences.getInstance();
      final saved = prefs.getString(_languagePrefsKey);
      final loaded = saved == 'en' ? AppLanguage.english : (saved == 'id' ? AppLanguage.indonesian : null);
      if (loaded != null && loaded != language) {
        language = loaded;
        notifyListeners();
      }
    } catch (e) {
      // No shared_preferences plugin under `flutter test`/unsupported
      // desktop targets - keep the default language rather than crash.
      debugPrint('[LocaleService] Failed to load saved language: $e');
    }
  }

  Future<void> setLanguage(AppLanguage value) async {
    if (language == value) return;
    language = value;
    notifyListeners();
    try {
      final prefs = await SharedPreferences.getInstance();
      await prefs.setString(_languagePrefsKey, value == AppLanguage.english ? 'en' : 'id');
    } catch (e) {
      debugPrint('[LocaleService] Failed to save language: $e');
    }
  }
}
