import 'locale_service.dart';

const _monthsId = ['Jan', 'Feb', 'Mar', 'Apr', 'Mei', 'Jun', 'Jul', 'Agu', 'Sep', 'Okt', 'Nov', 'Des'];
const _monthsEn = ['Jan', 'Feb', 'Mar', 'Apr', 'May', 'Jun', 'Jul', 'Aug', 'Sep', 'Oct', 'Nov', 'Dec'];

String formatDate(DateTime date, AppLanguage lang) {
  final months = lang == AppLanguage.indonesian ? _monthsId : _monthsEn;
  return '${date.day} ${months[date.month - 1]} ${date.year}';
}

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

String formatBytes(int bytes) {
  const kb = 1024;
  const mb = kb * 1024;
  const gb = mb * 1024;
  if (bytes >= gb) return '${(bytes / gb).toStringAsFixed(1)} GB';
  if (bytes >= mb) return '${(bytes / mb).toStringAsFixed(0)} MB';
  return '${(bytes / kb).toStringAsFixed(0)} KB';
}

String formatDistanceMeters(double meters) {
  if (meters < 1000) return '${meters.toStringAsFixed(0)} m';
  final km = meters / 1000;
  final decimals = km < 10 ? 2 : 1;
  return '${km.toStringAsFixed(decimals)} km';
}
