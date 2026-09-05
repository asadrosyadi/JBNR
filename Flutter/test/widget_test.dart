import 'package:flutter/material.dart';
import 'package:flutter_test/flutter_test.dart';

import 'package:smartjacketreceiver/main.dart';

void main() {
  testWidgets('Home screen shows title and scan button', (WidgetTester tester) async {
    await tester.pumpWidget(const SmartJacketApp());
    await tester.pump();

    expect(find.text('E-Textile Jacket'), findsWidgets);

    await tester.tap(find.text('Pengaturan'));
    await tester.pumpAndSettle();

    expect(find.widgetWithText(ElevatedButton, 'Start Scanning'), findsOneWidget);
  });
}
