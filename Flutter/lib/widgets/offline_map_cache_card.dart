import 'package:flutter/material.dart';

import '../services/downloaded_areas_store.dart';
import '../services/tile_disk_cache.dart';
import '../theme/app_colors.dart';
import 'section_card.dart';

/// Lists every place downloaded for offline use - a city, a mountain,
/// a campus, anything searched and downloaded from the live map - each
/// deletable on its own. Deleting one only removes that place's tiles,
/// leaving other downloaded places (and anything cached from casual
/// browsing) untouched. A "Hapus Semua" action is still available for
/// a full wipe.
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

  String _formatSize(int bytes) {
    const kb = 1024;
    const mb = kb * 1024;
    if (bytes >= mb) return '${(bytes / mb).toStringAsFixed(1)} MB';
    if (bytes >= kb) return '${(bytes / kb).toStringAsFixed(0)} KB';
    return '$bytes B';
  }

  String _formatDate(DateTime date) {
    const months = [
      'Jan', 'Feb', 'Mar', 'Apr', 'Mei', 'Jun',
      'Jul', 'Agu', 'Sep', 'Okt', 'Nov', 'Des',
    ];
    return '${date.day} ${months[date.month - 1]} ${date.year}';
  }

  Future<void> _deleteArea(DownloadedArea area) async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Hapus Data Tempat Ini'),
        content: Text(
          'Data offline "${area.label}" akan dihapus. Tempat lain yang sudah '
          'diunduh tidak akan terpengaruh.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Batal'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Hapus', style: TextStyle(color: AppColors.danger)),
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
      SnackBar(content: Text('Data "${area.label}" dihapus')),
    );
  }

  Future<void> _confirmClearAll() async {
    final confirmed = await showDialog<bool>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: const Text('Hapus Semua Data Peta Offline'),
        content: const Text(
          'Semua tile peta dari semua tempat, termasuk yang tersimpan dari '
          'penjelajahan biasa, akan dihapus. Anda perlu koneksi internet '
          'lagi untuk memuat ulang area manapun.',
        ),
        actions: [
          TextButton(
            onPressed: () => Navigator.pop(ctx, false),
            child: const Text('Batal'),
          ),
          TextButton(
            onPressed: () => Navigator.pop(ctx, true),
            child: const Text('Hapus Semua', style: TextStyle(color: AppColors.danger)),
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
      const SnackBar(content: Text('Semua data peta offline dihapus')),
    );
  }

  @override
  Widget build(BuildContext context) {
    return SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Row(
            children: [
              Icon(Icons.map_outlined, color: AppColors.primary, size: 20),
              SizedBox(width: 8),
              Text(
                'Peta Offline',
                style: TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
            ],
          ),
          const SizedBox(height: 12),
          FutureBuilder<(int, List<DownloadedArea>)>(
            future: _future,
            builder: (context, snapshot) {
              if (!snapshot.hasData) {
                return const Text(
                  'Menghitung...',
                  style: TextStyle(color: AppColors.muted, fontSize: 13),
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
                        '${_formatSize(totalBytes)} tersimpan',
                        style: const TextStyle(color: AppColors.muted, fontSize: 13),
                      ),
                      TextButton(
                        onPressed: totalBytes == 0 ? null : _confirmClearAll,
                        style: TextButton.styleFrom(foregroundColor: AppColors.danger),
                        child: const Text('Hapus Semua'),
                      ),
                    ],
                  ),
                  if (areas.isEmpty)
                    const Padding(
                      padding: EdgeInsets.only(top: 4),
                      child: Text(
                        'Belum ada tempat yang diunduh',
                        style: TextStyle(color: AppColors.muted, fontSize: 12),
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
                                    'Diunduh ${_formatDate(area.downloadedAt)}',
                                    style: const TextStyle(fontSize: 11, color: AppColors.muted),
                                  ),
                                ],
                              ),
                            ),
                            IconButton(
                              onPressed: () => _deleteArea(area),
                              tooltip: 'Hapus data tempat ini',
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
