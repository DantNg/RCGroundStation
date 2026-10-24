// Bảng màu + kiểu chữ, port từ Theme.qml của bản Qt ("GroundController Handheld").
import 'package:flutter/material.dart';

class AppTheme {
  // nền / khung
  static const deepBg = Color(0xFF040504);
  static const bg = Color(0xFF0A0D0B);
  static const view = Color(0xFF060A09);
  static const panel = Color(0xFF0E1210);
  static const panelSolid = Color(0xFF0B0E0C);

  // viền
  static const stroke = Color(0xFF232A25);
  static const strokeSoft = Color(0xFF1A211D);
  static const strokeBtn = Color(0xFF20281F);

  // nhấn
  static const accent = Color(0xFF37E0A0); // lục
  static const danger = Color(0xFFFF3B3B); // đỏ
  static const dangerSoft = Color(0xFFFF6B6B);
  static const warn = Color(0xFFFFB020); // hổ phách

  // chữ
  static const text = Color(0xFFE9E7EE);
  static const dim = Color(0xFF6F6D7A);
  static const sub = Color(0xFF8A8894);

  // HUD (chân trời)
  static const sky = Color(0xFF1D5478);
  static const ground = Color(0xFF6E4A1F);
  static const groundTop = Color(0xFFC99A55);

  // họ chữ
  static const ui = 'ChakraPetch';
  static const mono = 'JetBrainsMono';
}
