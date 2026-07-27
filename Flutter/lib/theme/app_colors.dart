import 'package:flutter/material.dart';

/// Flat-UI palette recovered from the original SmartJacketReceiver app.
class AppColors {
  AppColors._();

  static const Color primary = Color(0xFF2980B9); // belize hole
  static const Color primaryDark = Color(0xFF2C3E50); // midnight blue
  static const Color accentBlue = Color(0xFF3498DB); // peter river
  static const Color background = Color(0xFFECF0F1); // clouds
  static const Color warning = Color(0xFFF39C12); // orange
  static const Color danger = Color(0xFFE74C3C); // alizarin
  static const Color dangerDark = Color(0xFFC0392B); // pomegranate
  static const Color muted = Color(0xFF95A5A6); // concrete
  static const Color mutedLight = Color(0xFFB0BEC5);
  static const Color gold = Color(0xFFF7B731);
  static const Color inactive = Color(0xFF999999);

  static const Color statusActive = Color(0xFF27AE60); // nephritis green
  static const Color statusInactive = muted;

  /// Inactive status dot color when placed on the dark end of the
  /// header gradient (sampled from the original app).
  static const Color statusInactiveOnDark = Color(0xFF96A7B4);
}
