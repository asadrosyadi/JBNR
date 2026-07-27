import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:smartjacketreceiver/main.dart';

void main() {
  testWidgets('Home screen shows title and scan button', (WidgetTester tester) async {
    await tester.pumpWidget(const SmartJacketApp());
    await tester.pump();

    expect(find.text('E-Textile Jacket'), findsWidgets);

    // The scan/connect button lives on the "Pengaturan" tab, not the
    // default "Beranda" tab.
    await tester.tap(find.text('Pengaturan'));
    await tester.pumpAndSettle();

    expect(find.widgetWithText(ElevatedButton, 'Start Scanning'), findsOneWidget);
  });
}
