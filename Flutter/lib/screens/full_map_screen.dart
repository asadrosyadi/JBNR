import 'dart:math' as math;

import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:geolocator/geolocator.dart';
import 'package:latlong2/latlong.dart';
import 'package:provider/provider.dart';
import 'package:url_launcher/url_launcher.dart';

import '../l10n/strings.dart';
import '../models/lora_node.dart';
import '../services/ble_service.dart';
import '../services/downloaded_areas_store.dart';
import '../services/format_utils.dart';
import '../services/geocoder.dart';
import '../services/locale_service.dart';
import '../services/map_tile_downloader.dart';
import '../services/offline_first_tile_provider.dart';
import '../theme/app_colors.dart';

class FullMapScreen extends StatefulWidget {
  const FullMapScreen({super.key});

  @override
  State<FullMapScreen> createState() => _FullMapScreenState();
}

class _FullMapScreenState extends State<FullMapScreen> {
  final _mapController = MapController();
  LoraNode? _selectedNode;
  bool _downloading = false;
  bool _cancelDownload = false;
  double _downloadProgress = 0;
  int _downloadedTiles = 0;
  int _totalTiles = 0;
  bool _didInitialFit = false;

  List<LatLng> _points(double? myLat, double? myLng, List<LoraNode> nodes) => [
    if (myLat != null && myLng != null) LatLng(myLat, myLng),
    for (final node in nodes)
      if (node.hasFix) LatLng(node.latitude!, node.longitude!),
  ];

  void _fitBounds(List<LatLng> points) {
    if (points.isEmpty) return;
    if (points.length == 1) {
      _mapController.move(points.first, 16);
      return;
    }
    _mapController.fitCamera(
      CameraFit.bounds(
        bounds: LatLngBounds.fromPoints(points),
        padding: const EdgeInsets.all(48),
      ),
    );
  }

  Future<void> _openDirections(LoraNode node) async {
    if (!node.hasFix) return;
    final uri = Uri.https('www.google.com', '/maps/dir/', {
      'api': '1',
      'destination': '${node.latitude},${node.longitude}',
    });
    final launched = await launchUrl(uri, mode: LaunchMode.externalApplication);
    if (!launched && mounted) {
      final s = Strings(context.read<LocaleService>().language);
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(s.noMapAppFound)),
      );
    }
  }

  Future<void> _downloadVisibleArea() async {
    final bounds = _mapController.camera.visibleBounds;
    final currentZoom = _mapController.camera.zoom.round();
    await _downloadArea(
      bounds: bounds,
      minZoom: (currentZoom - 2).clamp(1, 19),
      maxZoom: (currentZoom + 2).clamp(1, 19),
    );
  }

  Future<void> _searchAndDownloadPlace() async {
    final s = Strings(context.read<LocaleService>().language);
    final lang = context.read<LocaleService>().language;
    final downloadedAreas = await DownloadedAreasStore.load();
    if (!mounted) return;

    final result = await showDialog<_PlaceSearchResult>(
      context: context,
      builder: (ctx) => _PlaceSearchDialog(downloadedAreas: downloadedAreas),
    );
    if (result == null || !mounted) return;

    if (result.existing != null) {
      final area = result.existing!;
      _mapController.fitCamera(
        CameraFit.bounds(
          bounds: area.bounds,
          padding: const EdgeInsets.all(24),
        ),
      );

      if (!result.redownload) return;

      final completed = await _downloadArea(
        bounds: area.bounds,
        minZoom: area.minZoom,
        maxZoom: area.maxZoom,
        areaLabel: s.alreadyDownloadedNote(area.label, formatDate(area.downloadedAt, lang)),
      );
      if (completed) {
        await DownloadedAreasStore.upsert(
          DownloadedArea(
            label: area.label,
            bounds: area.bounds,
            minZoom: area.minZoom,
            maxZoom: area.maxZoom,
            downloadedAt: DateTime.now(),
          ),
        );
      }
      return;
    }

    final query = result.query;
    if (query == null || query.trim().isEmpty) return;

    showDialog<void>(
      context: context,
      barrierDismissible: false,
      builder: (_) => const Center(child: CircularProgressIndicator()),
    );

    GeocodeResult? geocoded;
    try {
      geocoded = await Geocoder.search(query.trim());
    } catch (_) {
      geocoded = null;
    }

    if (!mounted) return;
    Navigator.of(context, rootNavigator: true).pop();

    if (geocoded == null) {
      ScaffoldMessenger.of(context).showSnackBar(
        SnackBar(content: Text(s.placeNotFound(query))),
      );
      return;
    }

    _mapController.fitCamera(
      CameraFit.bounds(
        bounds: geocoded.bounds,
        padding: const EdgeInsets.all(24),
      ),
    );

    const minZoom = 12;
    const maxZoom = 19;
    final completed = await _downloadArea(
      bounds: geocoded.bounds,
      minZoom: minZoom,
      maxZoom: maxZoom,
      areaLabel: geocoded.displayName,
    );
    if (completed) {
      await DownloadedAreasStore.upsert(
        DownloadedArea(
          label: geocoded.displayName,
          bounds: geocoded.bounds,
          minZoom: minZoom,
          maxZoom: maxZoom,
          downloadedAt: DateTime.now(),
        ),
      );
    }
  }

  static const _largeDownloadThreshold = 5000;

  Future<bool> _downloadArea({
    required LatLngBounds bounds,
    required int minZoom,
    required int maxZoom,
    String? areaLabel,
  }) async {
    final tileCount = MapTileDownloader.countTiles(
      bounds: bounds,
      minZoom: minZoom,
      maxZoom: maxZoom,
    );
    final estimatedBytes = tileCount * 15360;
    final estimatedSeconds = tileCount * 0.2;
    final isLargeDownload = tileCount > _largeDownloadThreshold;

    if (!mounted) return false;
    final lang = context.read<LocaleService>().language;
    final s = Strings(lang);
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(s.downloadThisAreaTitle),
        content: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              if (areaLabel != null) ...[
                Text(
                  areaLabel,
                  style: const TextStyle(fontWeight: FontWeight.w600),
                ),
                const SizedBox(height: 8),
              ],
              Text(
                s.downloadAreaBody(
                  tileCount,
                  minZoom,
                  maxZoom,
                  formatBytes(estimatedBytes),
                  formatDuration(estimatedSeconds, lang),
                ),
              ),
              if (isLargeDownload) ...[
                const SizedBox(height: 12),
                Container(
                  padding: const EdgeInsets.all(10),
                  decoration: BoxDecoration(
                    color: AppColors.warning.withValues(alpha: 0.12),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: Text(
                    s.largeDownloadWarning,
                    style: const TextStyle(fontSize: 12, color: AppColors.warning),
                  ),
                ),
              ],
            ],
          ),
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(s.cancel),
          ),
          FilledButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(s.download),
          ),
        ],
      ),
    );
    if (confirmed != true || !mounted) return false;

    _cancelDownload = false;
    setState(() {
      _downloading = true;
      _downloadProgress = 0;
      _downloadedTiles = 0;
      _totalTiles = tileCount;
    });

    var blockedByServer = false;
    try {
      await MapTileDownloader.download(
        bounds: bounds,
        minZoom: minZoom,
        maxZoom: maxZoom,
        isCancelled: () => _cancelDownload,
        onProgress: (done, total) {
          if (!mounted) return;
          setState(() {
            _downloadedTiles = done;
            _totalTiles = total;
            _downloadProgress = total == 0 ? 0 : done / total * 100;
          });
        },
      );
    } on OsmTileServerBlockedException {
      blockedByServer = true;
    }

    if (!mounted) return false;
    setState(() => _downloading = false);
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(
        duration: Duration(seconds: blockedByServer ? 6 : 4),
        content: Text(
          blockedByServer
              ? s.mapServerBlocked
              : _cancelDownload
              ? s.downloadCancelled
              : s.areaSavedOffline,
        ),
      ),
    );
    return !_cancelDownload && !blockedByServer;
  }

  @override
  Widget build(BuildContext context) {
    final ble = context.watch<BleService>();
    final s = Strings(context.watch<LocaleService>().language);
    final nodes = ble.nodeList;
    final points = _points(ble.myLatitude, ble.myLongitude, nodes);

    if (!_didInitialFit && points.isNotEmpty) {
      _didInitialFit = true;
      WidgetsBinding.instance.addPostFrameCallback((_) => _fitBounds(points));
    }

    final selected = _selectedNode;
    if (selected != null && !nodes.any((n) => identical(n, selected))) {
      _selectedNode = null;
    }

    return Scaffold(
      appBar: AppBar(
        title: Text(s.liveMap),
        actions: [
          IconButton(
            onPressed: _downloading ? null : _searchAndDownloadPlace,
            tooltip: s.searchAndDownloadPlace,
            icon: const Icon(Icons.search),
          ),
          IconButton(
            onPressed: _downloading
                ? () => setState(() => _cancelDownload = true)
                : _downloadVisibleArea,
            tooltip: _downloading
                ? s.cancelDownload
                : s.downloadThisAreaOffline,
            icon: _downloading
                ? Stack(
                    alignment: Alignment.center,
                    children: [
                      SizedBox(
                        width: 22,
                        height: 22,
                        child: CircularProgressIndicator(
                          strokeWidth: 2,
                          value: _downloadProgress / 100,
                          color: Colors.white,
                        ),
                      ),
                      const Icon(Icons.close, size: 12, color: Colors.white),
                    ],
                  )
                : const Icon(Icons.download_for_offline),
          ),
        ],
      ),
      body: Stack(
        children: [
          FlutterMap(
            mapController: _mapController,
            options: MapOptions(
              initialCenter: points.isNotEmpty
                  ? points.first
                  : const LatLng(0, 0),
              initialZoom: 15,
              onTap: (_, _) => setState(() => _selectedNode = null),
            ),
            children: [
              TileLayer(
                urlTemplate: tileUrlTemplate,
                userAgentPackageName: tileUserAgentPackageName,
                tileProvider: OfflineFirstTileProvider(),
                errorTileCallback: (tile, error, stackTrace) =>
                    debugPrint('[FullMapScreen] Tile load failed: $error'),
              ),
              if (_selectedNode != null &&
                  ble.myLatitude != null &&
                  ble.myLongitude != null &&
                  _selectedNode!.hasFix)
                PolylineLayer(
                  polylines: [
                    Polyline(
                      points: [
                        LatLng(ble.myLatitude!, ble.myLongitude!),
                        LatLng(
                          _selectedNode!.latitude!,
                          _selectedNode!.longitude!,
                        ),
                      ],
                      strokeWidth: 3,
                      color: AppColors.accentBlue,
                    ),
                  ],
                ),
              MarkerLayer(
                markers: [
                  if (ble.myLatitude != null && ble.myLongitude != null)
                    Marker(
                      point: LatLng(ble.myLatitude!, ble.myLongitude!),
                      width: 40,
                      height: 40,
                      child: const Icon(
                        Icons.my_location,
                        color: AppColors.accentBlue,
                        size: 30,
                      ),
                    ),
                  for (final node in nodes)
                    if (node.hasFix)
                      Marker(
                        point: LatLng(node.latitude!, node.longitude!),
                        width: 44,
                        height: 44,
                        child: GestureDetector(
                          onTap: () => setState(() => _selectedNode = node),
                          child: Icon(
                            Icons.location_on,
                            color: identical(_selectedNode, node)
                                ? AppColors.danger
                                : AppColors.warning,
                            size: 36,
                          ),
                        ),
                      ),
                ],
              ),
              RichAttributionWidget(
                alignment: AttributionAlignment.bottomLeft,
                attributions: [
                  TextSourceAttribution('OpenStreetMap contributors'),
                ],
              ),
            ],
          ),
          if (_selectedNode != null)
            Positioned(
              left: 12,
              right: 12,
              bottom: 12,
              child: _DirectionPanel(
                node: _selectedNode!,
                myLatitude: ble.myLatitude,
                myLongitude: ble.myLongitude,
                onClose: () => setState(() => _selectedNode = null),
                onNavigate: () => _openDirections(_selectedNode!),
              ),
            ),
          if (_downloading)
            Positioned(
              top: 12,
              left: 12,
              right: 12,
              child: _DownloadProgressBar(
                progress: _downloadProgress,
                downloadedTiles: _downloadedTiles,
                totalTiles: _totalTiles,
                onCancel: () => setState(() => _cancelDownload = true),
              ),
            ),
        ],
      ),
    );
  }
}

class _DownloadProgressBar extends StatelessWidget {
  final double progress;
  final int downloadedTiles;
  final int totalTiles;
  final VoidCallback onCancel;

  const _DownloadProgressBar({
    required this.progress,
    required this.downloadedTiles,
    required this.totalTiles,
    required this.onCancel,
  });

  @override
  Widget build(BuildContext context) {
    final s = Strings(context.watch<LocaleService>().language);
    return Material(
      elevation: 6,
      borderRadius: BorderRadius.circular(12),
      color: Colors.white,
      child: Padding(
        padding: const EdgeInsets.symmetric(horizontal: 16, vertical: 12),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Row(
              children: [
                Expanded(
                  child: Text(
                    s.downloadingMapForOffline,
                    style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 13),
                  ),
                ),
                Text(
                  '${progress.toStringAsFixed(0)}%',
                  style: const TextStyle(
                    fontWeight: FontWeight.bold,
                    fontSize: 13,
                  ),
                ),
              ],
            ),
            const SizedBox(height: 8),
            ClipRRect(
              borderRadius: BorderRadius.circular(6),
              child: LinearProgressIndicator(
                value: totalTiles == 0 ? null : progress / 100,
                minHeight: 8,
                backgroundColor: AppColors.background,
                color: AppColors.accentBlue,
              ),
            ),
            const SizedBox(height: 6),
            Row(
              mainAxisAlignment: MainAxisAlignment.spaceBetween,
              children: [
                Text(
                  s.tilesProgress(downloadedTiles, totalTiles),
                  style: const TextStyle(fontSize: 11, color: AppColors.muted),
                ),
                TextButton(
                  onPressed: onCancel,
                  style: TextButton.styleFrom(padding: EdgeInsets.zero),
                  child: Text(s.cancel),
                ),
              ],
            ),
          ],
        ),
      ),
    );
  }
}

class _DirectionPanel extends StatelessWidget {
  final LoraNode node;
  final double? myLatitude;
  final double? myLongitude;
  final VoidCallback onClose;
  final VoidCallback onNavigate;

  const _DirectionPanel({
    required this.node,
    required this.myLatitude,
    required this.myLongitude,
    required this.onClose,
    required this.onNavigate,
  });

  @override
  Widget build(BuildContext context) {
    final s = Strings(context.watch<LocaleService>().language);
    final distance = node.distanceFrom(myLatitude, myLongitude);
    final direction = node.compassFrom(myLatitude, myLongitude);
    final bearing = (myLatitude != null && myLongitude != null && node.hasFix)
        ? Geolocator.bearingBetween(
            myLatitude!,
            myLongitude!,
            node.latitude!,
            node.longitude!,
          )
        : null;

    return Material(
      elevation: 6,
      borderRadius: BorderRadius.circular(16),
      color: Colors.white,
      child: Padding(
        padding: const EdgeInsets.all(16),
        child: Column(
          mainAxisSize: MainAxisSize.min,
          children: [
            Row(
              children: [
                if (bearing != null)
                  Transform.rotate(
                    angle: bearing * (math.pi / 180),
                    child: const Icon(
                      Icons.navigation,
                      color: AppColors.accentBlue,
                      size: 40,
                    ),
                  )
                else
                  const Icon(
                    Icons.location_searching,
                    color: AppColors.muted,
                    size: 40,
                  ),
                const SizedBox(width: 12),
                Expanded(
                  child: Column(
                    crossAxisAlignment: CrossAxisAlignment.start,
                    children: [
                      Text(
                        node.name,
                        style: const TextStyle(
                          fontWeight: FontWeight.bold,
                          fontSize: 16,
                        ),
                      ),
                      const SizedBox(height: 2),
                      Text(
                        distance != null
                            ? s.distanceDirection(formatDistanceMeters(distance), s.compass(direction))
                            : s.waitingGpsFix,
                        style: const TextStyle(
                          color: AppColors.muted,
                          fontSize: 13,
                        ),
                      ),
                    ],
                  ),
                ),
                IconButton(onPressed: onClose, icon: const Icon(Icons.close)),
              ],
            ),
            if (node.hasFix) ...[
              const SizedBox(height: 8),
              SizedBox(
                width: double.infinity,
                child: FilledButton.icon(
                  onPressed: onNavigate,
                  icon: const Icon(Icons.directions),
                  label: Text(s.navigate),
                  style: FilledButton.styleFrom(backgroundColor: AppColors.accentBlue),
                ),
              ),
            ],
          ],
        ),
      ),
    );
  }
}

class _PlaceSearchResult {
  final String? query;
  final DownloadedArea? existing;
  final bool redownload;

  _PlaceSearchResult.query(this.query) : existing = null, redownload = false;
  _PlaceSearchResult.existing(this.existing, {this.redownload = false})
    : query = null;
}

class _PlaceSearchDialog extends StatefulWidget {
  final List<DownloadedArea> downloadedAreas;

  const _PlaceSearchDialog({required this.downloadedAreas});

  @override
  State<_PlaceSearchDialog> createState() => _PlaceSearchDialogState();
}

class _PlaceSearchDialogState extends State<_PlaceSearchDialog> {
  final _controller = TextEditingController();

  @override
  void dispose() {
    _controller.dispose();
    super.dispose();
  }

  void _submit() {
    final text = _controller.text;
    if (text.trim().isEmpty) return;
    Navigator.pop(context, _PlaceSearchResult.query(text));
  }

  void _selectExisting(DownloadedArea area) =>
      Navigator.pop(context, _PlaceSearchResult.existing(area));

  void _redownloadExisting(DownloadedArea area) => Navigator.pop(
    context,
    _PlaceSearchResult.existing(area, redownload: true),
  );

  @override
  Widget build(BuildContext context) {
    final lang = context.watch<LocaleService>().language;
    final s = Strings(lang);
    final typed = _controller.text.trim().toLowerCase();
    final matches = typed.isEmpty
        ? widget.downloadedAreas
        : widget.downloadedAreas
              .where((a) => a.label.toLowerCase().contains(typed))
              .toList();

    return AlertDialog(
      title: Text(s.downloadByPlaceTitle),
      content: SizedBox(
        width: double.maxFinite,
        child: SingleChildScrollView(
          child: Column(
            mainAxisSize: MainAxisSize.min,
            crossAxisAlignment: CrossAxisAlignment.start,
            children: [
              TextField(
                controller: _controller,
                autofocus: true,
                decoration: InputDecoration(
                  hintText: s.locationCityHint,
                  border: const OutlineInputBorder(),
                ),
                textInputAction: TextInputAction.search,
                onSubmitted: (_) => _submit(),
                onChanged: (_) => setState(() {}),
              ),
              if (matches.isNotEmpty) ...[
                const SizedBox(height: 12),
                Text(
                  s.alreadyDownloaded,
                  style: const TextStyle(fontSize: 12, color: AppColors.muted),
                ),
                ConstrainedBox(
                  constraints: const BoxConstraints(maxHeight: 180),
                  child: ListView.builder(
                    shrinkWrap: true,
                    itemCount: matches.length,
                    itemBuilder: (context, index) {
                      final area = matches[index];
                      return ListTile(
                        dense: true,
                        contentPadding: EdgeInsets.zero,
                        leading: const Icon(
                          Icons.check_circle,
                          color: AppColors.statusActive,
                          size: 18,
                        ),
                        title: Text(
                          area.label,
                          style: const TextStyle(fontSize: 13),
                          maxLines: 1,
                          overflow: TextOverflow.ellipsis,
                        ),
                        subtitle: Text(
                          s.downloadedOn(formatDate(area.downloadedAt, lang)),
                          style: const TextStyle(fontSize: 11),
                        ),
                        trailing: IconButton(
                          onPressed: () => _redownloadExisting(area),
                          tooltip: s.redownloadTooltip,
                          icon: const Icon(Icons.refresh, size: 18),
                        ),
                        onTap: () => _selectExisting(area),
                      );
                    },
                  ),
                ),
              ],
            ],
          ),
        ),
      ),
      actions: [
        TextButton(
          onPressed: () => Navigator.pop(context),
          child: Text(s.cancel),
        ),
        FilledButton(onPressed: _submit, child: Text(s.search)),
      ],
    );
  }
}
