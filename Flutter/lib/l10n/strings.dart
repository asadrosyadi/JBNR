import '../services/locale_service.dart';

class Strings {
  final AppLanguage lang;

  const Strings(this.lang);

  bool get _id => lang == AppLanguage.indonesian;

  String _t(String id, String en) => _id ? id : en;

  String get appSubtitle => _t('Sistem Pemantauan', 'Monitoring System');

  String get connected => _t('Terhubung', 'Connected');
  String get connectingEllipsis => _t('Menghubungkan...', 'Connecting...');
  String get disconnected => _t('Terputus', 'Disconnected');
  String get gpsActive => _t('GPS Aktif', 'GPS Active');
  String get gpsOff => _t('GPS Mati', 'GPS Off');
  String devicesCount(int n) => _t('$n Perangkat', '$n Devices');

  String get tabSettings => _t('Pengaturan', 'Settings');
  String get tabHome => _t('Beranda', 'Home');
  String get tabChat => 'Chat';
  String get connectedTitle => _t('Terhubung', 'Connected');
  String get connectedBody => _t('Berhasil terhubung ke Smart Jacket!', 'Successfully connected to Smart Jacket!');
  String get disconnectedTitle => _t('Terputus', 'Disconnected');
  String get disconnectedBody => _t('Berhasil terputus dari perangkat.', 'Successfully disconnected from device.');
  String get ok => 'OK';
  String get connectedBanner => _t('Terhubung ke Smart Jacket', 'Connected to Smart Jacket');
  String get myJacket => _t('Jaket Saya', 'My Jacket');
  String get pickYours => _t('Pilih milikmu', 'Pick yours');
  String get allLoraReadings => _t('Semua Pembacaan LoRa', 'All LoRa Readings');
  String get pickNode => _t('Pilih node', 'Pick node');

  String get availableDevices => _t('Perangkat Tersedia', 'Available Devices');
  String get scanningEllipsis => _t('Memindai...', 'Scanning...');
  String get scanAgain => _t('Pindai Lagi', 'Scan Again');
  String get noDevicesFound => _t('Tidak Ada Perangkat Ditemukan', 'No Devices Found');
  String get clickScanToSearch =>
      _t('Klik Pindai untuk mencari perangkat Smart Jacket', 'Click Scan to search for Smart Jacket devices');
  String get startScanning => _t('Mulai Memindai', 'Start Scanning');

  String get deviceInformation => _t('Informasi Perangkat', 'Device Information');
  String get deviceNameLabel => _t('Nama Perangkat:', 'Device Name:');
  String get deviceIdLabel => _t('ID Perangkat:', 'Device ID:');
  String get connectionLabel => _t('Koneksi:', 'Connection:');
  String get secure => _t('Aman', 'Secure');
  String get disconnectDevice => _t('Putuskan Perangkat', 'Disconnect Device');

  String get systemStatus => _t('Status Sistem', 'System Status');
  String get enabled => _t('Aktif', 'Enabled');
  String get disabled => _t('Nonaktif', 'Disabled');
  String get active => _t('Aktif', 'Active');
  String get off => _t('Mati', 'Off');
  String get connectionLabelShort => _t('Koneksi', 'Connection');
  String get inactive => _t('Nonaktif', 'Inactive');
  String get security => _t('Keamanan', 'Security');

  String get healthMetrics => _t('Metrik Kesehatan', 'Health Metrics');
  String get heartRateBpm => _t('Detak Jantung\nBPM', 'Heart Rate\nBPM');
  String get spo2Percent => 'SpO2\n%';
  String get temperatureC => _t('Suhu\n°C', 'Temperature\n°C');

  String get noLoraYet => _t('Belum ada LoRa', 'No LoRa yet');
  String get select => _t('Pilih', 'Select');

  String get locationCoordinates => _t('Koordinat Lokasi', 'Location Coordinates');
  String get yourLocation => _t('Lokasi Anda', 'Your Location');
  String get live => _t('Langsung', 'Live');
  String get noFix => _t('Belum ada fix', 'No fix');
  String get noLoraReportingYet => _t('Belum ada node LoRa yang melapor', 'No LoRa nodes reporting yet');

  String get locationTracking => _t('Pelacakan Lokasi', 'Location Tracking');
  String jacketsFixOf(int fixed, int total) => _t('$fixed/$total jaket fix', '$fixed/$total jackets fixed');
  String get idle => _t('Nonaktif', 'Idle');
  String get waitingEllipsis => _t('Menunggu...', 'Waiting...');
  String get tracking => _t('Melacak', 'Tracking');
  String get jacketsTracked => _t('Jaket Dilacak', 'Jackets Tracked');
  String get status => _t('Status', 'Status');
  String get phoneGpsYou => _t('GPS HP (Anda)', 'Phone GPS (You)');
  String get waitingGpsFix => _t('Menunggu fix GPS...', 'Waiting for GPS fix...');

  String compass(String? code) {
    if (code == null) return '--';
    if (_id) {
      const map = {
        'N': 'Utara', 'NE': 'Timur Laut', 'E': 'Timur', 'SE': 'Tenggara',
        'S': 'Selatan', 'SW': 'Barat Daya', 'W': 'Barat', 'NW': 'Barat Laut',
      };
      return map[code] ?? code;
    }
    const map = {
      'N': 'North', 'NE': 'Northeast', 'E': 'East', 'SE': 'Southeast',
      'S': 'South', 'SW': 'Southwest', 'W': 'West', 'NW': 'Northwest',
    };
    return map[code] ?? code;
  }

  String get allLora => _t('Semua LoRa', 'All LoRa');
  String get broadcastToAllNodes => _t('Broadcast ke semua node', 'Broadcast to all nodes');
  String get personalChat => _t('Chat personal', 'Personal chat');
  String get noLoraConnectedForChat =>
      _t('Belum ada LoRa yang terhubung untuk diajak chat personal', 'No LoRa connected yet for personal chat');
  String get deleteConversationTitle => _t('Hapus Percakapan', 'Delete Conversation');
  String deleteConversationBody(String label) => _t(
        'Semua pesan di percakapan "$label" akan dihapus. Lanjutkan?',
        'All messages in "$label" will be deleted. Continue?',
      );
  String get cancel => _t('Batal', 'Cancel');
  String get delete => _t('Hapus', 'Delete');
  String get deleteMessageTitle => _t('Hapus Pesan', 'Delete Message');
  String get deleteMessageBody => _t('Hapus pesan ini?', 'Delete this message?');
  String get deleteAllMessagesTitle => _t('Hapus Semua Pesan', 'Delete All Messages');
  String get noMessagesYet => _t('Belum ada pesan', 'No messages yet');
  String get messageToAllLora => _t('Pesan ke semua LoRa...', 'Message to all LoRa...');
  String messageTo(String label) => _t('Pesan ke $label...', 'Message to $label...');
  String get deleteAllMessagesTooltip => _t('Hapus semua pesan', 'Delete all messages');

  String get offlineMap => _t('Peta Offline', 'Offline Map');
  String get calculatingEllipsis => _t('Menghitung...', 'Calculating...');
  String storedSize(String size) => _t('$size tersimpan', '$size stored');
  String get deleteAll => _t('Hapus Semua', 'Delete All');
  String get noPlacesDownloadedYet => _t('Belum ada tempat yang diunduh', 'No places downloaded yet');
  String downloadedOn(String date) => _t('Diunduh $date', 'Downloaded $date');
  String get deleteThisPlaceTooltip => _t('Hapus data tempat ini', "Delete this place's data");
  String get deleteThisPlaceTitle => _t('Hapus Data Tempat Ini', "Delete This Place's Data");
  String deleteThisPlaceBody(String label) => _t(
        'Data offline "$label" akan dihapus. Tempat lain yang sudah diunduh tidak akan terpengaruh.',
        'Offline data for "$label" will be deleted. Other downloaded places won\'t be affected.',
      );
  String get deleteAllOfflineMapTitle => _t('Hapus Semua Data Peta Offline', 'Delete All Offline Map Data');
  String get deleteAllOfflineMapBody => _t(
        'Semua tile peta dari semua tempat, termasuk yang tersimpan dari penjelajahan biasa, akan dihapus. '
            'Anda perlu koneksi internet lagi untuk memuat ulang area manapun.',
        'Every map tile from every place, including tiles cached from casual browsing, will be deleted. '
            'You\'ll need an internet connection again to reload any area.',
      );
  String placeDataDeleted(String label) => _t('Data "$label" dihapus', '"$label" data deleted');
  String get allOfflineMapDataDeleted => _t('Semua data peta offline dihapus', 'All offline map data deleted');

  String get liveMap => _t('Peta Langsung', 'Live Map');
  String get searchAndDownloadPlace => _t('Cari & unduh tempat', 'Search & download place');
  String get cancelDownload => _t('Batalkan unduhan', 'Cancel download');
  String get downloadThisAreaOffline => _t('Unduh area ini untuk offline', 'Download this area for offline');
  String get noMapAppFound => _t('Tidak ada aplikasi peta untuk membuka navigasi', 'No map app found to open directions');
  String get downloadThisAreaTitle => _t('Unduh Area Ini', 'Download This Area');
  String downloadAreaBody(int tileCount, int minZoom, int maxZoom, String bytes, String duration) => _t(
        'Sekitar $tileCount tile peta (zoom $minZoom-$maxZoom)\n≈ $bytes • perkiraan waktu $duration',
        'About $tileCount map tiles (zoom $minZoom-$maxZoom)\n≈ $bytes • estimated time $duration',
      );
  String get largeDownloadWarning => _t(
        'Unduhan sebesar ini makan waktu lama, dan menghabiskan banyak penyimpanan',
        'A download this size takes a long time and uses a lot of storage',
      );
  String get download => _t('Unduh', 'Download');
  String get mapServerBlocked => _t(
        'Server peta OpenStreetMap membatasi akses sementara. Coba lagi nanti dengan area yang lebih kecil.',
        'The OpenStreetMap tile server is temporarily rate-limiting access. Try again later with a smaller area.',
      );
  String get downloadCancelled => _t('Unduhan dibatalkan', 'Download cancelled');
  String get areaSavedOffline => _t('Area peta tersimpan untuk offline', 'Map area saved for offline use');
  String get downloadingMapForOffline => _t('Mengunduh peta untuk offline...', 'Downloading map for offline...');
  String tilesProgress(int done, int total) => '$done/$total tile';
  String get navigate => _t('Navigasi', 'Navigate');
  String distanceDirection(String distance, String direction) =>
      _t('$distance • arah $direction', '$distance • $direction direction');
  String placeNotFound(String query) => _t('Tempat "$query" tidak ditemukan', 'Place "$query" not found');
  String get downloadByPlaceTitle => _t('Unduh Berdasarkan Tempat', 'Download By Place');
  String get locationCityHint => _t('lokasi/kota', 'location/city');
  String get alreadyDownloaded => _t('Sudah pernah diunduh', 'Already downloaded');
  String get redownloadTooltip => _t('Unduh ulang', 'Re-download');
  String get search => _t('Cari', 'Search');
  String alreadyDownloadedNote(String label, String date) =>
      _t('$label\n(sudah pernah diunduh $date)', '$label\n(already downloaded $date)');

  String get locationPermissionRequiredTitle => _t('Izin Lokasi Diperlukan', 'Location Permission Required');
  String get locationPermissionDeniedBody => _t(
        'Izin lokasi ditolak. Mohon berikan izin agar posisi GPS Anda dapat ditampilkan.',
        'Location permission was denied. Please grant it so your GPS position can be shown.',
      );
  String get locationServicesDisabledTitle => _t('Layanan Lokasi Nonaktif', 'Location Services Disabled');
  String get locationServicesDisabledBody => _t(
        'Informasi lokasi tidak tersedia. Mohon periksa GPS dan koneksi jaringan Anda.',
        'Location information is unavailable. Please check your GPS and network connection.',
      );
  String get bluetoothLocationPermissionDeniedBody => _t(
        'Izin Bluetooth dan Lokasi ditolak. Mohon berikan izin untuk memindai Smart Jacket Anda.',
        'Bluetooth and Location permission was denied. Please grant them to scan for your Smart Jacket.',
      );
  String get askMeLater => _t('Nanti Saja', 'Ask Me Later');
  String get openSettings => _t('Buka Pengaturan', 'Open Settings');

  String get language => _t('Bahasa', 'Language');
  String get languageIndonesian => 'Bahasa Indonesia';
  String get languageEnglish => 'English';
}
