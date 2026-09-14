# Web Configurator keymap for Keyball39
# WebHID通信を有効化
RAW_ENABLE = yes

# Viaのraw_hid_receiveが独自プロトコルと競合するため無効化
VIA_ENABLE = no

# EEPROMへのキーマップ保存を有効化
DYNAMIC_KEYMAP_ENABLE = yes

# マウスボタン（MS_BTN1-5はPointing Device経由で動作するため不要）
MOUSEKEY_ENABLE = no

OLED_ENABLE = yes

# RGB_MATRIX移行作業用（rgb-matrix-migrationブランチ）。
# 波紋演出（SOLID_RIPPLE）のためRGBLIGHTから移行中。同じWS2812チェーンを
# 両方式で同時に駆動することはできないため、移行完了までRGBLIGHTは無効化する。
RGB_MATRIX_ENABLE = yes
# RP2040の大容量フラッシュにより、LED版/通常版の分岐は廃止。
# メディアキー・マクロ・ジェスチャーを全て同時有効化する。
RGBLIGHT_ENABLE = no
EXTRAKEY_ENABLE = yes
OPT_DEFS += -DGESTURE_ENABLE

# 2026-09-09、複数ジェスチャーモード化後に発生した不具合（スクロールレイヤーの
# 誤検知等）の原因調査のため一時的に有効化。原因特定後は必ずno（board側の既定値）
# に戻すこと。
CONSOLE_ENABLE = yes

# タップダンス（2026-09-11、本人希望で再有効化。「ダブルタップ機能」として利用。
# 以前AVR版で容量節約のためOFFにしていたが、RP2040は2MBフラッシュに対し実測
# 60KB台しか使っておらず制約は無い。コンボ再有効化と同じ理由）
TAP_DANCE_ENABLE = yes

# Auto Shift（2026-09-11、本人希望で有効化。RP2040はフラッシュに余裕があるため制約なし。
# タップダンス・コンボ再有効化と同じ理由）
AUTO_SHIFT_ENABLE = yes

# コンボ（2026-09-10、本人希望で再有効化。以前AVR版で容量不足によりOFFにしていたが、
# RP2040は2MBフラッシュに対し実測60KB程度しか使っておらず制約は無い）
COMBO_ENABLE = yes

# OS自動判別（2026-09-11、本人希望で追加。接続先PCのOS種別をUSB記述子から判定し、
# Keyball LinkでのOS表示と「Mac接続時にCmd/Ctrl自動入れ替え」機能に使う）
OS_DETECTION_ENABLE = yes

# HIDハンドラ・詳細設定・マクロをビルドに含める
SRC += lib/keyball/kb_hid.c
SRC += lib/keyball/kb_settings.c
SRC += lib/keyball/kb_macro.c
SRC += lib/keyball/kb_combo.c
SRC += lib/keyball/td_config.c
