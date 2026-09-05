import 'dart:async';

import 'package:flutter/foundation.dart';
import 'package:shared_preferences/shared_preferences.dart';

const _languagePrefsKey = 'app_language_v1';

enum AppLanguage { indonesian, english }

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
