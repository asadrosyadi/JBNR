import 'locale_service.dart';

const _monthsId = ['Jan', 'Feb', 'Mar', 'Apr', 'Mei', 'Jun', 'Jul', 'Agu', 'Sep', 'Okt', 'Nov', 'Des'];
const _monthsEn = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

/// `"3 Jan 2026"` / `"3 Jan 2026"` - shared by [OfflineMapCacheCard] and
/// [FullMapScreen] so both spell downloaded-area dates the same way.
String formatDate(DateTime date, AppLanguage lang) {
  final months = lang == AppLanguage.indonesian ? _monthsId : _monthsEn;
  return '${date.day} ${months[date.month - 1]} ${date.year}';
}

/// Estimated-download-time readout, e.g. `"2 menit"` / `"2 min"`.
String formatDuration(double seconds, AppLanguage lang) {
  final isId = lang == AppLanguage.indonesian;
  if (seconds >= 3600) {
    final v = (seconds / 3600).toStringAsFixed(1);
    return isId ? '$v jam' : '$v hr';
  }
  if (seconds >= 60) {
    final v = (seconds / 60).toStringAsFixed(0);
    return isId ? '$v menit' : '$v min';
  }
  final v = seconds.toStringAsFixed(0);
  return isId ? '$v detik' : '$v sec';
}

/// Byte-size readout, e.g. `"12 MB"`. Units (GB/MB/KB) are the same
/// abbreviation in both languages, so this doesn't need [AppLanguage].
String formatBytes(int bytes) {
  const kb = 1024;
  const mb = kb * 1024;
  const gb = mb * 1024;
  if (bytes >= gb) return '${(bytes / gb).toStringAsFixed(1)} GB';
  if (bytes >= mb) return '${(bytes / mb).toStringAsFixed(0)} MB';
  return '${(bytes / kb).toStringAsFixed(0)} KB';
}

/// Distance readout: meters below 1000, kilometers above (2 decimals
/// under 10 km, 1 decimal beyond - keeps it short without losing
/// precision right at the switchover). `m`/`km` are the same
/// abbreviation in both languages.
String formatDistanceMeters(double meters) {
  if (meters < 1000) return '${meters.toStringAsFixed(0)} m';
  final km = meters / 1000;
  final decimals = km < 10 ? 2 : 1;
  return '${km.toStringAsFixed(decimals)} km';
}
