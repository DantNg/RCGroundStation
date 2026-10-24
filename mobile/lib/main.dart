// Trạm Điều Khiển Mặt Đất — bản Flutter (Android), port từ bản Qt/QML.
//
// Giao diện "GroundController Handheld": bản đồ vệ tinh + PFD + cần lái ảo, khoá
// ngang. Backend MAVLink/UDP trong controller.dart; UI dưới lib/ui/.
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import 'controller.dart';
import 'joystick_controller.dart';
import 'theme.dart';
import 'ui/handheld_screen.dart';

Future<void> main() async {
  WidgetsFlutterBinding.ensureInitialized();
  await SystemChrome.setPreferredOrientations(
      [DeviceOrientation.landscapeLeft, DeviceOrientation.landscapeRight]);
  SystemChrome.setEnabledSystemUIMode(SystemUiMode.immersiveSticky);
  runApp(const GcsApp());
}

class GcsApp extends StatefulWidget {
  const GcsApp({super.key});
  @override
  State<GcsApp> createState() => _GcsAppState();
}

class _GcsAppState extends State<GcsApp> {
  late final GcsController controller = GcsController();
  late final JoystickController joystick = JoystickController(controller.link);

  @override
  void dispose() {
    joystick.dispose();
    controller.dispose();
    super.dispose();
  }

  @override
  Widget build(BuildContext context) {
    return MaterialApp(
      title: 'Trạm Điều Khiển Mặt Đất',
      debugShowCheckedModeBanner: false,
      theme: ThemeData(
        brightness: Brightness.dark,
        scaffoldBackgroundColor: AppTheme.deepBg,
        fontFamily: AppTheme.ui,
      ),
      home: HandheldScreen(controller: controller, joystick: joystick),
    );
  }
}
