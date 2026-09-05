import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../services/downloaded_areas_store.dart';
import '../services/format_utils.dart';
import '../services/locale_service.dart';
import '../services/tile_disk_cache.dart';
import '../theme/app_colors.dart';
import 'section_card.dart';

class OfflineMapCacheCard extends StatefulWidget {
  const OfflineMapCacheCard({super.key});

  @override
  State<OfflineMapCacheCard> createState() => _OfflineMapCacheCardState();
}

class _OfflineMapCacheCardState extends State<OfflineMapCacheCard> {
  late Future<(int, List<DownloadedArea>)> _future;

  @override
  void initState() {
    super.initState();
    _refresh();
  }

  void _refresh() {
    setState(() {
      _future = _load();
    });
  }

  Future<(int, List<DownloadedArea>)> _load() async {
    final size = await TileDiskCache.sizeInBytes();
    final areas = await DownloadedAreasStore.load();
    return (size, areas);
  }

  Future<void> _deleteArea(DownloadedArea area, Strings s) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(s.deleteThisPlaceTitle),
        content: Text(s.deleteThisPlaceBody(area.label)),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(s.cancel),
          ),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(s.delete, style: const TextStyle(color: AppColors.danger)),
          ),
        ],
      ),
    );
    if (confirmed != true) return;

    await TileDiskCache.deleteRegion(
      bounds: area.bounds,
      minZoom: area.minZoom,
      maxZoom: area.maxZoom,
    );
    await DownloadedAreasStore.remove(area.label);
    if (!mounted) return;
    _refresh();
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(s.placeDataDeleted(area.label))),
    );
  }

  Future<void> _confirmClearAll(Strings s) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(s.deleteAllOfflineMapTitle),
        content: Text(s.deleteAllOfflineMapBody),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: Text(s.cancel),
          ),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: Text(s.deleteAll, style: const TextStyle(color: AppColors.danger)),
          ),
        ],
      ),
    );
    if (confirmed != true) return;

    await TileDiskCache.clearAll();
    await DownloadedAreasStore.clear();
    if (!mounted) return;
    _refresh();
    ScaffoldMessenger.of(context).showSnackBar(
      SnackBar(content: Text(s.allOfflineMapDataDeleted)),
    );
  }

  @override
  Widget build(BuildContext context) {
    final lang = context.watch<LocaleService>().language;
    final s = Strings(lang);
    return SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.map_outlined, color: AppColors.primary, size: 20),
              const SizedBox(width: 8),
              Text(
                s.offlineMap,
                style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
            ],
          ),
          const SizedBox(height: 12),
          FutureBuilder<(int, List<DownloadedArea>)>(
            future: _future,
            builder: (context, snapshot) {
              if (!snapshot.hasData) {
                return Text(
                  s.calculatingEllipsis,
                  style: const TextStyle(color: AppColors.muted, fontSize: 13),
                );
              }
              final (totalBytes, areas) = snapshot.data!;

              return Column(
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Row(
                    mainAxisAlignment: MainAxisAlignment.spaceBetween,
                    children: [
                      Text(
                        s.storedSize(formatBytes(totalBytes)),
                        style: const TextStyle(color: AppColors.muted, fontSize: 13),
                      ),
                      TextButton(
                        onPressed: totalBytes == 0 ? null : () => _confirmClearAll(s),
                        style: TextButton.styleFrom(foregroundColor: AppColors.danger),
                        child: Text(s.deleteAll),
                      ),
                    ],
                  ),
                  if (areas.isEmpty)
                    Padding(
                      padding: const EdgeInsets.only(top: 4),
                      child: Text(
                        s.noPlacesDownloadedYet,
                        style: const TextStyle(color: AppColors.muted, fontSize: 12),
                      ),
                    )
                  else ...[
                    const Divider(height: 20),
                    for (final area in areas)
                      Padding(
                        padding: const EdgeInsets.symmetric(vertical: 4),
                        child: Row(
                          children: [
                            Expanded(
                              child: Column(
                                crossAxisAlignment: CrossAxisAlignment.start,
                                children: [
                                  Text(
                                    area.label,
                                    style: const TextStyle(
                                      fontWeight: FontWeight.w600,
                                      fontSize: 13,
                                    ),
                                    maxLines: 1,
                                    overflow: TextOverflow.ellipsis,
                                  ),
                                  Text(
                                    s.downloadedOn(formatDate(area.downloadedAt, lang)),
                                    style: const TextStyle(fontSize: 11, color: AppColors.muted),
                                  ),
                                ],
                              ),
                            ),
                            IconButton(
                              onPressed: () => _deleteArea(area, s),
                              tooltip: s.deleteThisPlaceTooltip,
                              icon: const Icon(
                                Icons.delete_outline,
                                color: AppColors.danger,
                                size: 20,
                              ),
                            ),
                          ],
                        ),
                      ),
                  ],
                ],
              );
            },
          ),
        ],
      ),
    );
  }
}
