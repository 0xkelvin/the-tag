import 'package:flutter_test/flutter_test.dart';
import 'package:veea_tag_app/main.dart';

void main() {
  testWidgets('App launches', (WidgetTester tester) async {
    await tester.pumpWidget(const VeeaTagApp());
    expect(find.text('Veea Tag'), findsOneWidget);
  });
}
