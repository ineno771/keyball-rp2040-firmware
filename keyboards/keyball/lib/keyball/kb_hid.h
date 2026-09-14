// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later
#pragma once

#include <stdint.h>
#include <stdbool.h>

// WebHID通信プロトコルのコマンドID
#define KB_HID_CMD_GET_INFO       0x01  // キーボード情報取得
#define KB_HID_CMD_GET_KEYCODE    0x02  // キーコード取得
#define KB_HID_CMD_SET_KEYCODE    0x03  // キーコード設定
#define KB_HID_CMD_GET_TRACKBALL  0x04  // トラックボール設定取得
#define KB_HID_CMD_SET_TRACKBALL  0x05  // トラックボール設定変更
#define KB_HID_CMD_SAVE           0x06  // EEPROMに保存
#define KB_HID_CMD_REBOOT         0x07  // ブートローダーへリセット
#define KB_HID_CMD_RESET_KEYMAP   0x08  // キーマップをデフォルトに戻す（EEPROM再初期化）
#define KB_HID_CMD_GET_ACCEL      0x09  // アクセラレーション設定取得
#define KB_HID_CMD_SET_ACCEL      0x0A  // アクセラレーション設定変更
#define KB_HID_CMD_GET_LED        0x0B  // LED設定取得
#define KB_HID_CMD_SET_LED        0x0C  // LED設定変更
#define KB_HID_CMD_GET_TD         0x0D  // タップダンス設定取得
#define KB_HID_CMD_SET_TD         0x0E  // タップダンス設定変更
#define KB_HID_CMD_GET_SETTINGS   0x0F  // キーボード詳細設定取得
#define KB_HID_CMD_SET_SETTINGS   0x10  // キーボード詳細設定変更
#define KB_HID_CMD_TEST_LED       0x11  // LED診断: 指定LEDのみ点灯 (0xFF=終了)
#define KB_HID_CMD_GET_MATRIX     0x12  // マトリクス状態取得: 各行のビットマスクを返す
#define KB_HID_CMD_GET_MACRO      0x13  // マクロ取得: [cmd,slot] → [cmd,slot,count,kc×6,status]
#define KB_HID_CMD_SET_MACRO      0x14  // マクロ設定: [cmd,slot,count,kc×6] → [cmd,status]
// 0x15/0x16は複数ジェスチャーモード化（2026-09-09）に伴い廃止（欠番）。
// 旧・単一ジェスチャー（gesture_tap・単一レイヤー）に依存していたため、新しい
// kb_gesture_mode_tの構造とは互換性がない。後継はGET/SET_GESTURE_MODE(0x20/0x21)
// とGET/SET_GESTURE_THRESHOLD(0x22/0x23)。AVR版（keyball-link/plus-firmware）は
// 引き続き0x15/0x16で旧仕様のまま応答するため、この欠番はRP2040版限定。
#define KB_HID_CMD_GET_VERSION    0x17  // ファームウェアのバージョン取得（major.minor.patch）
#define KB_HID_CMD_GET_PRECISION  0x18  // 超低速モードのCPI分周値取得
#define KB_HID_CMD_SET_PRECISION  0x19  // 超低速モードのCPI分周値変更
#define KB_HID_CMD_GET_LAYER_LED_ENABLE 0x1A  // レイヤー連動LED機能の有効/無効取得
#define KB_HID_CMD_SET_LAYER_LED_ENABLE 0x1B  // レイヤー連動LED機能の有効/無効変更
#define KB_HID_CMD_GET_LAYER_LED        0x1C  // 指定レイヤーのLED設定取得
#define KB_HID_CMD_SET_LAYER_LED        0x1D  // 指定レイヤーのLED設定変更
#define KB_HID_CMD_GET_SCROLL_INERTIA   0x1E  // 慣性スクロール設定取得
#define KB_HID_CMD_SET_SCROLL_INERTIA   0x1F  // 慣性スクロール設定変更
#define KB_HID_CMD_GET_GESTURE_MODE      0x20  // 指定ジェスチャーモード(0-3)の設定取得
#define KB_HID_CMD_SET_GESTURE_MODE      0x21  // 指定ジェスチャーモード(0-3)の設定変更
#define KB_HID_CMD_GET_GESTURE_THRESHOLD 0x22  // ジェスチャー発動しきい値（全モード共通）取得
#define KB_HID_CMD_SET_GESTURE_THRESHOLD 0x23  // ジェスチャー発動しきい値（全モード共通）変更
#define KB_HID_CMD_GET_GESTURE_WAVE_SPEED 0x24  // ジェスチャー連動LEDウェーブの速さ取得
#define KB_HID_CMD_SET_GESTURE_WAVE_SPEED 0x25  // ジェスチャー連動LEDウェーブの速さ変更
#define KB_HID_CMD_GET_GESTURE_WAVE_ENABLE 0x26  // ジェスチャー連動LEDウェーブの有効/無効取得
#define KB_HID_CMD_SET_GESTURE_WAVE_ENABLE 0x27  // ジェスチャー連動LEDウェーブの有効/無効変更
#define KB_HID_CMD_GET_SHAKE              0x28  // シェイク機能（発動キー・感度）取得
#define KB_HID_CMD_SET_SHAKE              0x29  // シェイク機能（発動キー・感度）変更
#define KB_HID_CMD_GET_DFLICK             0x2A  // ダブルフリック（方向別キー・時間窓）取得
#define KB_HID_CMD_SET_DFLICK             0x2B  // ダブルフリック（方向別キー・時間窓）変更
#define KB_HID_CMD_GET_COMBO              0x2C  // 指定コンボスロット(0-7)の設定取得
#define KB_HID_CMD_SET_COMBO              0x2D  // 指定コンボスロット(0-7)の設定変更
#define KB_HID_CMD_GET_OS                 0x2E  // OS自動判別: 現在検出しているOS種別を取得（0=不明,1=Linux,2=Windows,3=macOS,4=iOS）
#define KB_HID_CMD_GET_DPI_CURVE          0x2F  // DPIカーブ（有効/無効・出力5点）取得
#define KB_HID_CMD_SET_DPI_CURVE          0x30  // DPIカーブ（有効/無効・出力5点）変更

// ステータスコード
#define KB_HID_STATUS_OK    0x00
#define KB_HID_STATUS_ERROR 0x01

// プロトコルバージョン
#define KB_HID_PROTOCOL_VERSION 0x01

void kb_hid_receive(uint8_t *data, uint8_t length);

#ifdef RGBLIGHT_ENABLE
// GET/SET_LED・GET/SET_LAYER_LEDで使うeffect_id（0=オフ 1=単色 ...）を
// 実際のRGBLIGHT_MODE_*定数に変換する。範囲外のIDは0（オフ相当）扱い。
// kb_hid.cとkeymap.c(レイヤー連動LED)の両方から使う唯一の変換元。
uint8_t kb_hid_led_effect_to_mode(uint8_t effect_id);
uint8_t kb_hid_led_effect_count(void);
#endif

#ifdef RGB_MATRIX_ENABLE
// RGBLIGHT版のkb_hid_led_effect_to_mode()に相当するRGB_MATRIX版の変換元。
// 実際のRGB_MATRIX_*定数（自作エフェクト含む）に変換する。範囲外のIDは0（オフ相当）扱い。
uint8_t kb_hid_led_effect_to_rgb_matrix_mode(uint8_t effect_id);

// UG_TOG/UG_NEXT/UG_HUE+等（QK_UNDERGLOW_*、quantum/process_keycode/process_underglow.c）は
// RGB_MATRIX本体の状態を直接書き換えるが、本ファームの「通常（レイヤー0）LED設定」は
// kb_led_config（EEPROM独自管理領域）が唯一の正とされていて、起動時・レイヤー連動LED/
// ジェスチャーウェーブのオーバーライド終了時にkeyball_apply_normal_led()がRGB_MATRIX本体を
// kb_led_configの値へ強制的に書き戻す。UG_*キーでの変更はkb_led_config側に反映されない
// ため、その次の書き戻しタイミング（次回起動、あるいはオーバーライド終了時）で変更が
// 消えてしまう不具合があった（「Val+/Val-を押しても効かない」）。
// UG_*キー押下後にRGB_MATRIX本体の実際の状態を読み戻し、kb_led_configへ反映することで
// 同期を保つ。post_process_record_user（keymap.c）から呼ぶ。
void kb_led_config_sync_from_rgb_matrix(void);
#endif

#if defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)
// 季節限定エフェクト（ハロウィン・イースター）。バックエンド本体のモードとしては存在
// しない（kb_hid_led_effect_to_mode()/kb_hid_led_effect_to_rgb_matrix_mode()では
// 0＝オフ相当が返る）ため、実際の描画はkeyball_seasonal_led_task()が別途行う。
// このID自体はバックエンドに依存しない概念なので両対応で共有する。
// - クリスマス(7)は各バックエンド本体のモードのまま。
// - ニューイヤー(12)は削除済み・欠番。
// - ナイトライダー(6)・交互点灯(10)はRGBLIGHT版でスプリット両ハーフの同期方式が
//   定まらず一旦見送った欠番。RGB_MATRIX版では片側ハーフ内で完結する実装に直った
//   ため対応表(kb_hid.c)には存在するが、Web UIのLED_EFFECTSは本家RGBLIGHT版とも
//   共有しているため、そちらでは引き続き非表示にしている。
// - 14・15・16・17・18はRGB_MATRIX版限定の追加エフェクト：
//   14=リアクティブ(REACTIVE_KEYS) 15=タイピングヒートマップ(HEATMAP、自作。
//   組み込みTYPING_HEATMAPは隣接キーへの熱伝播があり不採用。明るさは設定値のまま
//   保持し、押した回数(蓄熱量)に応じて色相が寒色→暖色に変化する仕様)
//   16=トラックボールリアクティブ(TRACKBALL) 17=リップル(RIPPLE)
//   18=ジェスチャーウェーブ(GESTURE_WAVE)。GESTURE_ENABLEのあるファームでのみ意味を
//   持つが、他のエフェクトと違いWeb UIのLED_EFFECTS一覧には出さない（選択式ではない
//   ため）。ジェスチャーで実際にキーが送出された瞬間(keyball_gesture_wave_trigger)に
//   keyball_gesture_wave_task()が現在のRGB_MATRIXモードを強制的にこのIDへ上書きし、
//   発火が終わったら元の通常LED/レイヤー連動LEDの表示に自動で復帰する。こうする
//   ことで、レイヤー連動LEDが別のエフェクトを選んでいる最中でも必ずウェーブが
//   見える（2026-09-09: 選択式エフェクトのままだと競合して発動しないことがあった）。
#define KB_LED_EFFECT_HALLOWEEN      11
#define KB_LED_EFFECT_EASTER         13
#define KB_LED_EFFECT_TOTAL_COUNT    19  // 有効なeffect_idの総数（0-18。6・10・12は欠番）
bool kb_hid_led_effect_is_seasonal(uint8_t effect_id);
#endif
