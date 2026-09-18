// Copyright 2024 keyball-custom contributors
// SPDX-License-Identifier: GPL-2.0-or-later

#include QMK_KEYBOARD_H
#include "quantum.h"
#include "lib/keyball/kb_hid.h"
#include "lib/keyball/td_config.h"
#include "lib/keyball/kb_settings.h"
#ifdef COMBO_ENABLE
#include "lib/keyball/kb_combo.h"
#endif
#ifdef OS_DETECTION_ENABLE
#include "os_detection.h"
#endif
#ifdef CONSOLE_ENABLE
#include "eeprom.h"
#endif
#ifndef LED_VERSION_BUILD
#include "lib/keyball/kb_macro.h"
#endif

#ifdef KEYBALL_AML_THRESHOLD_RUNTIME
extern uint8_t kb_aml_threshold;
#endif

// ジェスチャー方向・シェイク・ダブルフリックなど、Web UIで選んだキーコードを、
// 実際にそのキーを押した場合と全く同じ経路で1回分（押す→離す）発動させる共通ヘルパー。
//
// 2026-09-10発覚: tap_code16()/register_code16()はTG/TO/DF/OSLのような
// 「量子(quantum)キーコード」（0x5000番台以降、レイヤー切替やワンショット等）を
// 正しく処理できない。実装（quantum.c）を見ると、モディファイア処理をした後、
// 最終的にregister_code((uint8_t)code)を呼んでいるため、16bit値の下位1バイトだけが
// 生のHIDキーコードとして送られてしまう。例えばTG(1)=0x5261の下位バイトは
// 0x61=KC_KP_9で、実際に「TG1」のつもりで設定したのに数字の「9」が入力される
// という実害のあるバグとして発見された（本人のシェイク機能テストで発覚）。
// 最初はTG/TO/DF/OSLだけを個別にQMKのレイヤーAPIへ振り分けて対処したが、その後
// OSM(ワンショット修飾)でも同じ問題が再発した。量子キーコードの種類ごとに
// この場しのぎの分岐を増やし続けるのは限界があるため、QMK標準のCombo機能
// （process_combo.cのrelease_combo）と全く同じ方法に作り直した：実在しない
// キー位置(row=0,col=0)・COMBO_EVENT種別の合成キーイベントを組み立てて
// action_tapping_process()に渡す。これは実際にそのキーを押下/離した場合と
// 完全に同じ処理経路（process_action→action_for_keycode）を通るため、
// TG/TO/DF/OSL/OSMはもちろん、Mod+キーやワンショットレイヤーの「次の1キーだけ」
// という本来の自動解除動作まで含めて正しく動作する。
// keyrecord_t.keycode フィールドはQMK側でCOMBO_ENABLE(または REPEAT_KEY_ENABLE)が
// 定義されているときしか実体を持たない（action.h参照）。このファイルはCOMBO_ENABLEに
// 依存しているので、rules.mkから外すとここがコンパイルエラーになる（黙って
// 壊れるより先に気づけるように意図的にエラーにしている）。
#if !defined(COMBO_ENABLE) && !defined(REPEAT_KEY_ENABLE)
#    error "kb_synth_keyevent needs keyrecord_t.keycode: keep COMBO_ENABLE=yes in rules.mk"
#endif

static void kb_synth_keyevent(uint16_t kc, bool pressed) {
    if (!kc) return;
    keyrecord_t record = {
        .event   = MAKE_COMBOEVENT(pressed),
        .keycode = kc,
    };
#ifndef NO_ACTION_TAPPING
    action_tapping_process(record);
#else
    process_record(&record);
#endif
}

static void kb_fire_keycode(uint16_t kc) {
    kb_synth_keyevent(kc, true);
    kb_synth_keyevent(kc, false);
}


#ifdef GESTURE_ENABLE
// 複数ジェスチャーモード: トラックボールの移動を累積し方向で判定して送出する。
// モードは4つ(0-3)あり、GST_HOLD〜4キーを押している間はそのモードを一時的に優先、
// 離すと現在のレイヤーに連動するモード（なければ無効）に戻る。
static int8_t   g_gst_manual_mode = -1;  // GST_HOLD〜4を押している間の一時モード。-1=なし
static int8_t   g_gst_layer_mode  = -1;  // 現レイヤーに連動するモード。-1=連動なし
static int16_t  g_gesture_acc_x   = 0;
static int16_t  g_gesture_acc_y   = 0;
static bool     g_gesture_cooldown = false;  // 単発方向発火後、次の発火まで待つ（1スイング1回）
static uint16_t g_gesture_cd_timer = 0;
#define GST_COOLDOWN_MS 350  // クールダウン時間(ms)

// 手動優先、なければレイヤー連動。どちらもなければ-1(ジェスチャー無効)
static inline int8_t gst_active_mode(void) {
    return g_gst_manual_mode >= 0 ? g_gst_manual_mode : g_gst_layer_mode;
}
#endif

// シェイク機能（2026-09-10〜。トラックボールを振ると設定したキーを発動）。
// 判定方法: 移動方向（主に動いている軸の符号）が反転した回数を数え、一定回数
// 反転したら「振っている」と判定する。動きが一定時間途切れたらリセットする。
// 以前は固定時間窓内の「合計移動量 vs 正味の変位」を比較する方式だったが、窓の
// 切れ目とシェイクの往復周期がズレると往復の片道だけを見てしまい正味の変位が
// 小さくならず、実機で全く発動しない不具合があったため、反転回数を直接数える
// 方式に変更した。感度しきい値(kb_shake_threshold)は反転が始まってからの移動量
// 合計と比較し、「小さな反転を素早く繰り返しただけ」の誤発動を防ぐ。
// ジェスチャーモードの有無に関わらず常時判定する（物理的な「振る」動作は方向
// ジェスチャーの動きとは性質が異なり混同しにくいため）。
//
// 2026-09-10、本人希望により発火条件を厳しくした。反転回数の既定値を4→6
// （3往復）に増やしたことに加え、「一連の反転すべてが短時間以内に完了する
// こと」を新たに要求するようにした。これが無いと、普通に作業しながら
// トラックボールを操作しているだけでも、時間をかければ偶然何度も方向反転が
// 積み重なって誤発動しうる（個々の反転間の間隔=SHAKE_GAP_MSだけ見ていても、
// 間隔が毎回ギリギリ収まれば全体としては長時間だらだら動かしただけでも条件を
// 満たしてしまうため）。反転の合計所要時間まで短く制限することで、本当に
// 素早く振った時にしか発動しないようにしている。反転回数・所要時間の上限は
// どちらもkb_shake_reversals/kb_shake_run_max_msとしてWeb UIから調整できる
// （現状の既定値=反転6回・700msを中心に、緩める・厳しくする両方向へ余白を
// 持たせてある）。
static int8_t   g_shake_last_sign = 0;  // 直近の主動方向の符号（-1/0/1）
static uint8_t  g_shake_reversals = 0;  // 現在の連続反転回数
static uint16_t g_shake_abs       = 0;  // 反転が始まってからの移動量合計
static uint16_t g_shake_run_start;      // 最初の反転が起きた時刻（合計所要時間の起点）
static uint16_t g_shake_last_move_time;
static bool     g_shake_cooldown = false;
static uint16_t g_shake_cd_timer;

static void shake_task(const report_mouse_t *mouse_report) {
    if (!kb_shake_enable_get()) return;

    const uint16_t SHAKE_GAP_MS      = 200;  // これより動きが途切れたらリセット
    const uint16_t SHAKE_COOLDOWN_MS = 500;  // 発動後、連続発動しないための猶予
    uint8_t        shake_reversals_needed = kb_shake_reversals_get();  // 発動に必要な反転回数
    uint16_t       shake_run_max_ms       = kb_shake_run_max_ms_get(); // 反転が全て収まるべき時間の上限

    int8_t x = mouse_report->x;
    int8_t y = mouse_report->y;

    if (x == 0 && y == 0) {
        if (timer_elapsed(g_shake_last_move_time) > SHAKE_GAP_MS) {
            g_shake_reversals = 0;
            g_shake_abs       = 0;
            g_shake_last_sign = 0;
        }
    } else {
        g_shake_last_move_time = timer_read();
        // 横振り・縦振りどちらにも対応するため、より大きく動いた軸を見る
        int8_t ax       = x < 0 ? -x : x;
        int8_t ay       = y < 0 ? -y : y;
        int8_t dominant = (ax >= ay) ? x : y;
        int8_t sign     = dominant > 0 ? 1 : (dominant < 0 ? -1 : 0);

        if (sign != 0) {
            if (g_shake_last_sign != 0 && sign != g_shake_last_sign) {
                if (g_shake_reversals == 0) {
                    g_shake_run_start = timer_read();
                } else if (timer_elapsed(g_shake_run_start) > shake_run_max_ms) {
                    // 時間がかかりすぎ＝ゆっくりした通常操作とみなし最初からやり直す
                    g_shake_reversals = 0;
                    g_shake_abs       = 0;
                    g_shake_run_start = timer_read();
                }
                g_shake_reversals++;
#ifdef CONSOLE_ENABLE
                dprintf("kb_debug: shake reversal=%u abs=%u key=%u threshold=%u elapsed=%u\n", g_shake_reversals, g_shake_abs, kb_shake_key_get(), kb_shake_threshold_get(), timer_elapsed(g_shake_run_start));
#endif
            }
            g_shake_last_sign = sign;
        }
        g_shake_abs += (uint16_t)(ax + ay);

        uint16_t key = kb_shake_key_get();
        if (key && !g_shake_cooldown && g_shake_reversals >= shake_reversals_needed && g_shake_abs >= kb_shake_threshold_get() && timer_elapsed(g_shake_run_start) <= shake_run_max_ms) {
#ifdef CONSOLE_ENABLE
            dprintf("kb_debug: shake FIRE key=%u\n", key);
#endif
            kb_fire_keycode(key);
            g_shake_cooldown  = true;
            g_shake_cd_timer  = timer_read();
            g_shake_reversals = 0;
            g_shake_abs       = 0;
            g_shake_last_sign = 0;
        }
    }

    if (g_shake_cooldown && timer_elapsed(g_shake_cd_timer) >= SHAKE_COOLDOWN_MS) {
        g_shake_cooldown = false;
    }
}

// ダブルフリック（2026-09-10〜。同じ方向へ短時間で2回フリックすると発火）。
// 本人希望によりジェスチャーモード（GST_HOLD〜4キーやジェスチャーレイヤー）とは
// 完全に独立させ、通常のトラックボール操作（カーソル移動）中に動作するように
// している。移動が連続している間(streak)を1つの塊として捉え、streakが止まった
// 瞬間に判定する（keyball.cの慣性スクロールのフリック検出と同じ考え方）。
// カーソルの動き自体は一切変更しない（観測するだけ）ので、通常のマウス操作を
// 妨げない。
//
// 2026-09-10、実機で「登録していないキーが発動する」不具合が発覚。原因は、
// 普通にカーソルを操作しているだけでも「動いて→一瞬止まる」を繰り返しており、
// ピーク速度さえ超えていればそれを「フリック」と誤判定していたため。実際の
// フリック（指で弾く動作）はごく短時間(streak)で終わるのに対し、通常の
// カーソル操作はもっと長く動き続けることが多い。この違いを使い、streakの
// 継続時間が一定以下(FLICK_MAX_DURATION_MS)の場合だけ「フリック」とみなす
// ようにして誤発動を防いだ。
//
// 2026-09-10、さらに「同じ方向に2回振ったつもりが1回で発動してしまう」不具合が
// 発覚。原因は、センサーのノイズで移動量がほんの一瞬(1レポート分)だけ0になる
// ことがあり、それだけで「streakが止まった」と確定判定していたため、実際には
// 指1回分の連続した動きが2つのstreakに分断され、「同じ方向に2回フリックした」
// と誤認識していたこと。これを防ぐため、動きが止まったように見えても
// STOP_DEBOUNCE_MSの間は「まだ止まっていないかもしれない」候補のまま保留し、
// その間に動きが再開したら同じstreakの続きとして扱う（停止の確定を遅らせる）
// ようにした。
//
// 2026-09-10、方向検知の精度についての指摘を受け、streak中で最も速かった
// 1レポート分(ピーク値)だけで方向を決める方式から、ジェスチャー機能と同じ
// 「streak全体の移動量を合算してから方向判定する」方式に変更した。ピーク値
// 方式は、たまたまピークになった1レポートにノイズや斜め方向の動きが混ざると
// 方向を誤りやすい弱点があったが、合算方式はstreak全体を均せるためノイズに
// 強い。感度しきい値(kb_dflick_flick_threshold)も「ピーク速度」から「streak
// 全体の移動量合計」に意味が変わったため、範囲・既定値をジェスチャーの発動
// しきい値(KB_GESTURE_TH_*)と同じ基準に合わせ直した。
static bool     g_dflick_streak_active   = false;
static bool     g_dflick_stop_pending    = false;  // 停止候補（確定待ち）中かどうか
static uint16_t g_dflick_stop_pending_at = 0;
static uint16_t g_dflick_streak_start    = 0;
static int16_t  g_dflick_sum_x           = 0;  // streak開始からの移動量合計
static int16_t  g_dflick_sum_y           = 0;
static int8_t   g_dflick_last_dir        = -1;
static uint16_t g_dflick_last_time       = 0;

static void dflick_task(const report_mouse_t *mouse_report) {
    if (!kb_dflick_enable_get()) return;

    const uint16_t STOP_DEBOUNCE_MS = 30;  // これより短い停止はセンサーノイズとみなし無視する

    if (mouse_report->x != 0 || mouse_report->y != 0) {
        if (!g_dflick_streak_active) {
            g_dflick_sum_x        = 0;
            g_dflick_sum_y        = 0;
            g_dflick_streak_start = timer_read();
        }
        g_dflick_streak_active = true;
        g_dflick_stop_pending  = false;  // 動きが再開したので停止候補は取り消し、同じstreakの続きとする
        g_dflick_sum_x += mouse_report->x;
        g_dflick_sum_y += mouse_report->y;
        return;
    }

    if (!g_dflick_streak_active) return;

    if (!g_dflick_stop_pending) {
        // 止まったように見えた最初の瞬間。まだ確定させず、少し待って様子を見る。
        g_dflick_stop_pending    = true;
        g_dflick_stop_pending_at = timer_read();
        return;
    }
    if (timer_elapsed(g_dflick_stop_pending_at) < STOP_DEBOUNCE_MS) return;  // まだ確定待ち

    // ここまで来て初めて「本当に止まった」と確定する
    g_dflick_streak_active  = false;
    g_dflick_stop_pending   = false;
    uint16_t streak_elapsed = timer_elapsed(g_dflick_streak_start);
    if (streak_elapsed > kb_dflick_max_duration_ms_get()) {
#ifdef CONSOLE_ENABLE
        dprintf("kb_debug: dflick REJECT (too long) duration=%u max=%u\n", streak_elapsed, kb_dflick_max_duration_ms_get());
#endif
        return;  // 動き続けた時間が長すぎる＝フリックではない
    }

    int16_t sx  = g_dflick_sum_x;
    int16_t sy  = g_dflick_sum_y;
    int16_t asx = sx < 0 ? -sx : sx;
    int16_t asy = sy < 0 ? -sy : sy;
    if (asx + asy < kb_dflick_flick_threshold_get()) {
#ifdef CONSOLE_ENABLE
        dprintf("kb_debug: dflick REJECT (too weak) sum=%d threshold=%u\n", asx + asy, kb_dflick_flick_threshold_get());
#endif
        return;  // 動きが小さすぎる＝フリックではない
    }

    bool    horizontal = asx > asy;
    uint8_t dir        = horizontal
        ? (sx > 0 ? 3 : 2)   // 右:3 左:2
        : (sy > 0 ? 1 : 0);  // 下:1 上:0

#ifdef CONSOLE_ENABLE
    dprintf("kb_debug: dflick flick detected dir=%u sx=%d sy=%d last_dir=%d elapsed=%u window=%u\n", dir, (int)sx, (int)sy, g_dflick_last_dir, timer_elapsed(g_dflick_last_time), kb_dflick_window_ms_get());
#endif

    if (g_dflick_last_dir == dir && timer_elapsed(g_dflick_last_time) <= kb_dflick_window_ms_get()) {
        uint16_t dkc = kb_dflick_key_get(dir);
#ifdef CONSOLE_ENABLE
        dprintf("kb_debug: dflick FIRE dir=%u keycode=0x%04X\n", dir, dkc);
#endif
        if (dkc) kb_fire_keycode(dkc);
        g_dflick_last_dir = -1;  // 消費（3連続フリックで多重発火しないように）
    } else {
        g_dflick_last_dir  = dir;
        g_dflick_last_time = timer_read();
    }
}

// clang-format off
const uint16_t PROGMEM keymaps[][MATRIX_ROWS][MATRIX_COLS] = {
  [0] = {
    {     19,     18,     12,     24,     28,      0 },
    {  17197,     15,     14,  16653,     11,      0 },
    {  16952,     55,     54,     16,     17,      0 },
    {    647,      0,      0,  16938,  16938,  16680 },
    {     20,     26,      8,     21,     23,      0 },
    {      4,     22,      7,      9,     10,      0 },
    {  16669,     27,      6,     25,      5,      0 },
    {    225,    227,    226,  17297,     44,  17040 },
  },
  [1] = {
    {     39,     38,     37,     36,     35,      0 },
    {     67,     66,     65,     64,     63,      0 },
    {    558,     47,     49,     48,     51,      0 },
    {    137,      0,      0,    209,      1,      1 },
    {     30,     31,     32,     33,     34,      0 },
    {     58,     59,     60,     61,     62,      0 },
    {     46,    549,     48,     49,     52,      0 },
    {      1,      1,      1,     41,     43,     76 },
  },
  [2] = {
    {      0,      0,    211,      0,      0,      0 },
    {  21027,    210,     82,    209,      0,      0 },
    {      0,     79,     81,     80,      0,      0 },
    {      0,      0,      0,     42,      1,      1 },
    {     84,     95,     96,     97,     86,      0 },
    {     85,     92,     93,     94,     87,      0 },
    {     98,     89,     90,     91,     99,      0 },
    {      1,      1,      1,      1,      1,      1 },
  },
  [3] = {
    {     86,     97,     96,     95,     84,      0 },
    {     87,     94,     93,     92,     85,      0 },
    {     99,     91,     90,     89,     98,      0 },
    {      0,      0,      0,    210,      1,      1 },
    {      0,      0,      0,      0,      0,      0 },
    {    260,    278,    263,    265,    266,      0 },
    {    285,    283,    262,    281,    261,      0 },
    {      1,      1,      1,      1,      1,      1 },
  },
};
// clang-format on

// マクロキー再生（QK_MACRO_0〜QK_MACRO_15 = 0x7700〜0x770F）
bool process_record_user(uint16_t keycode, keyrecord_t *record) {
#ifdef GESTURE_ENABLE
    if (keycode == GST_HOLD || keycode == GST_HOLD2 || keycode == GST_HOLD3 || keycode == GST_HOLD4) {
        int8_t idx = (keycode == GST_HOLD) ? 0 : (keycode == GST_HOLD2) ? 1 : (keycode == GST_HOLD3) ? 2 : 3;
        if (record->event.pressed) {
            g_gst_manual_mode = idx;
        } else if (g_gst_manual_mode == idx) {
            g_gst_manual_mode = -1;
        }
        g_gesture_acc_x    = 0;
        g_gesture_acc_y    = 0;
        g_gesture_cooldown = false;
        return false;
    }
#endif
#ifndef LED_VERSION_BUILD
    if (keycode >= QK_MACRO && keycode <= QK_MACRO_MAX) {
        if (record->event.pressed) {
            kb_macro_play(keycode - QK_MACRO);   // タップ実行＋ホールド開始
        } else {
            kb_macro_release();                  // ホールド中のキーを解放
        }
        return false;
    }
#endif
    return true;
}

#ifdef RGB_MATRIX_ENABLE
// UG_TOG/UG_NEXT/UG_HUE+/UG_VAL+等（process_record_userより後、quantum本体の
// process_underglow内でRGB_MATRIXの状態を直接書き換えた後）に呼ばれる。押した直後の
// RGB_MATRIX本体の実際の状態をkb_led_config（Web UI側の「通常」LED設定の正）へ
// 読み戻して同期する（詳細はkb_hid.hのkb_led_config_sync_from_rgb_matrix参照。
// 同期しないとVal+/Val-等での変更が次回起動やレイヤー連動LEDのオーバーライド終了時に
// 消えてしまう）。
void post_process_record_user(uint16_t keycode, keyrecord_t *record) {
    if (!record->event.pressed) return;
    switch (keycode) {
        case QK_UNDERGLOW_TOGGLE:
        case QK_UNDERGLOW_MODE_NEXT:
        case QK_UNDERGLOW_MODE_PREVIOUS:
        case QK_UNDERGLOW_HUE_UP:
        case QK_UNDERGLOW_HUE_DOWN:
        case QK_UNDERGLOW_SATURATION_UP:
        case QK_UNDERGLOW_SATURATION_DOWN:
        case QK_UNDERGLOW_VALUE_UP:
        case QK_UNDERGLOW_VALUE_DOWN:
        case QK_UNDERGLOW_SPEED_UP:
        case QK_UNDERGLOW_SPEED_DOWN:
            kb_led_config_sync_from_rgb_matrix();
            break;
    }
}
#endif

// DPIカーブ: speed(0-127、動きの速さ)を、5点を滑らかな曲線で結んだルックアップ
// テーブル(kb_dpi_curve_lut_get()、設定変更時にだけ計算済み)から引いて、
// 実際に送る速さ(0-255)に変換する。Photoshopのトーンカーブと同じ考え方
// （X軸=入力の速さは固定、Y軸=出力の速さだけをユーザーが調整する）。
static uint8_t keyball_dpi_curve_eval(uint8_t speed) {
    if (speed >= KB_DPI_CURVE_LUT_SIZE) speed = KB_DPI_CURVE_LUT_SIZE - 1;  // 念のための範囲外ガード
    return kb_dpi_curve_lut_get()[speed];
}

// ポインターアクセラレーション（DPIカーブが有効なときはそちらを優先する）
void keyball_on_apply_motion_to_mouse_move(keyball_motion_t *m, report_mouse_t *r, bool is_left) {
    static int32_t acc[2][2];
    static int16_t ema[2][2];

    int16_t dx = m->y;
    int16_t dy = m->x;
    if (!is_left) { dx = -dx; dy = -dy; }

    uint8_t side = is_left ? 0 : 1;
    ema[side][0] += dx - ema[side][0] / 4;
    ema[side][1] += dy - ema[side][1] / 4;
    dx = ema[side][0] / 4;
    dy = ema[side][1] / 4;

    int16_t abs_dx = dx < 0 ? -dx : dx;
    int16_t abs_dy = dy < 0 ? -dy : dy;
    int16_t speed  = abs_dx > abs_dy ? abs_dx : abs_dy;

    if (kb_dpi_curve_enable_get()) {
        acc[side][0] = 0; acc[side][1] = 0;  // 旧アクセル方式の蓄積は使わないのでクリアしておく
        if (speed == 0) {
            dx = 0; dy = 0;
        } else {
            uint8_t  out_speed = keyball_dpi_curve_eval((uint8_t)speed);
            int32_t out_x = ((int32_t)dx * out_speed) / speed;
            int32_t out_y = ((int32_t)dy * out_speed) / speed;
            dx = out_x < -127 ? -127 : out_x > 127 ? 127 : (int16_t)out_x;
            dy = out_y < -127 ? -127 : out_y > 127 ? 127 : (int16_t)out_y;
        }
    } else {
        uint8_t accel = keyball_get_accel();
        if (accel > 0) {
            if (speed == 0) {
                acc[side][0] = 0; acc[side][1] = 0;
            } else {
                uint8_t scale = 64 / accel;
                acc[side][0] += (int32_t)dx * speed;
                acc[side][1] += (int32_t)dy * speed;
                int32_t out_x = acc[side][0] / scale;
                int32_t out_y = acc[side][1] / scale;
                acc[side][0] -= out_x * (int32_t)scale;
                acc[side][1] -= out_y * (int32_t)scale;
                dx = out_x < -127 ? -127 : out_x > 127 ? 127 : (int16_t)out_x;
                dy = out_y < -127 ? -127 : out_y > 127 ? 127 : (int16_t)out_y;
            }
        } else {
            acc[side][0] = 0; acc[side][1] = 0;
        }
    }
    r->x = (int8_t)dx;
    r->y = (int8_t)dy;
    m->x = 0; m->y = 0;
}

#ifdef OS_DETECTION_ENABLE
// OS自動判別: KB_FLAG_OS_AUTO_SWAP が有効なときだけ、Mac/iOS接続時に
// Cmd(GUI)とCtrlを入れ替える。RAM上の keymap_config を書き換えるだけで
// EEPROMには触らない（接続のたびに再判定されるため保存不要。Magic機能の
// EEPROM保存値と混線させないためにも直接書き込みはしない）。
void kb_apply_os_swap(os_variant_t os) {
    bool on                       = (kb_settings_get().flags & KB_FLAG_OS_AUTO_SWAP) != 0;
    bool mac                      = on && (os == OS_MACOS || os == OS_IOS);
    keymap_config.swap_lctl_lgui  = mac;
    keymap_config.swap_rctl_rgui  = mac;
}

// OS判別が確定するたびに呼ばれる（接続直後・リセット後など）。
bool process_detected_host_os_kb(os_variant_t os) {
    if (!process_detected_host_os_user(os)) return false;
    kb_apply_os_swap(os);
    return true;
}
#endif

// layer_state_set_user()より後方で定義しているため前方宣言する
// （keyboard_post_init_user()から起動直後の初期反映のために呼ぶ）。
static void kb_apply_layer_features(uint8_t hl);

// 2026-09-18
// Keyball+はトラックボールを左右どちらの基板にも実装できるリバーシブル設計
// （本家keyball-plus-firmwareのkeyball_on_adjust_layout()参照。あちらはRGBLIGHTの
// クリッピング範囲をkeyball.this_have_ball/that_have_ball（実際にPMW3360センサーを
// 検出した結果）で都度計算しており、is_keyboard_left()（SPLIT_HAND_MATRIX_GRIDに
// よる基板固有の配線特性で決まる値。トラックボールの有無とは無関係）に依存していない）。
//
// ところがこのキーマップには、is_keyboard_left()依存の前提が2箇所ある：
//
// (1) RGB_MATRIX: コア側のrgb_matrix_get_limits()/rgb_matrix_led_index()
//     （quantum/rgb_matrix/rgb_matrix.c、weak関数ではないため上書き不可）が
//     is_keyboard_left()とRGB_MATRIX_SPLITだけでハーフごとの描画範囲を固定的に
//     決める仕組みのため、g_led_config（keyballplus.c。ボール搭載側=idx0-25と
//     いう前提で作成済み）と整合させるには「ボール搭載側 = is_keyboard_left()が
//     真」という前提が必要。→ is_keyboard_left()自体を下記のように上書きして解決。
//
// (2) g_led_config.matrix_co（quantum/matrix.cのthisHand = isLeftHand ?
//     0 : MATRIX_ROWS_PER_HAND;によって、is_keyboard_left()=true側の物理スイッチが
//     常にマトリクス行0-3、false側が行4-7に来る）は、「行0-3 = ボール搭載側の
//     スイッチ」という前提で作られた固定テーブル（keyballplus.c）。この前提は
//     ボールが本来の意味でのis_keyboard_left()=true側の基板にある場合しか
//     成立しない。ボールがfalse側の基板にある場合、行0-3には実際には非搭載側の
//     スイッチが来るため、g_led_config.matrix_coの行0-3/4-7を入れ替えないと
//     キー反応系エフェクト（リップル等）の対応がズレる
//     （2026-09-18、Pキーを押すとidx=8＝ボール側の行0列0のLEDが反応する不具合として
//     発覚。matrix_coは`const`ではない通常のRAM上の構造体なので実行時に書き換え可能）。
//
// is_keyboard_left()はQMKコア(split_common)がweak関数として提供しており上書き
// 可能。これを利用し、実際にボールを検出したこちら側を「左」として扱うよう
// 差し替える。ただし起動直後・トラックボールセンサーの検出（keyboard_post_init_kb
// のpmw3360_init()）が終わる前は、分割キーボード間の通信ネゴシエーションや
// マトリクス結合（上記(2)のthisHand計算。起動時に一度だけis_keyboard_left()を
// 読んでキャッシュし、以後は再取得しないためキー入力には影響しない）に基板本来の
// 配線特性由来の値が必要なため、g_ball_probe_readyが立つ（＝post_initが完了し
// this_have_ballが確定した）までは元の値をそのまま返す。
static bool    g_ball_probe_ready = false;
static int8_t  g_hw_is_left       = -1;  // 基板本来の配線特性由来の値（(2)のキャッシュと共有）
extern bool is_keyboard_left_impl(void);

bool is_keyboard_left(void) {
    if (g_hw_is_left < 0) {
        g_hw_is_left = is_keyboard_left_impl();
    }
    if (!g_ball_probe_ready) {
        return (bool)g_hw_is_left;
    }
    return keyball.this_have_ball;
}

#ifdef RGB_MATRIX_ENABLE
// 上記(2)の対処。「ボール搭載側の基板が、本来の意味でのis_keyboard_left()=false側
// だったか」を判定し、そうであればg_led_config.matrix_coの行0-3⇔4-7を入れ替える。
// this_have_ball/g_hw_is_leftはどちらもこのハーフだけで完結する値のため、
// 両ハーフが独立に同じ結論に達する（相方との通信は不要）。
static void kb_fixup_led_matrix_rows_if_needed(void) {
    bool ball_is_on_hw_left_side = keyball.this_have_ball ? (bool)g_hw_is_left : !(bool)g_hw_is_left;
    if (ball_is_on_hw_left_side) return;  // g_led_configの前提通りなので何もしない

    for (uint8_t r = 0; r < MATRIX_ROWS / 2; r++) {
        for (uint8_t c = 0; c < MATRIX_COLS; c++) {
            uint8_t tmp                                  = g_led_config.matrix_co[r][c];
            g_led_config.matrix_co[r][c]                 = g_led_config.matrix_co[r + MATRIX_ROWS / 2][c];
            g_led_config.matrix_co[r + MATRIX_ROWS / 2][c] = tmp;
        }
    }
}
#endif

void keyboard_post_init_user(void) {
#ifdef CONSOLE_ENABLE
    // 2026-09-09、原因調査用の一時的なデバッグ出力（qmk consoleで確認）。
    debug_enable = true;
    uint8_t raw_scroll_layer = eeprom_read_byte((const uint8_t *)(uintptr_t)KB_SCROLL_LAYER_EEPROM);
    dprintf("kb_debug: boot raw_scroll_layer_byte=%u kb_scroll_layer_get=%u\n", raw_scroll_layer, kb_scroll_layer_get());
#endif
    kb_settings_t s = kb_settings_get();
#ifdef AUTO_SHIFT_ENABLE
    if (s.flags & KB_FLAG_AUTO_SHIFT) autoshift_enable();
    else autoshift_disable();
#endif
#ifdef COMBO_ENABLE
    kb_combo_init();  // EEPROMに保存された組み合わせをkey_combos[]へ反映する
    // SET_SETTINGS時と同じ反映処理。起動直後にも保存済みの設定を適用しないと、
    // 再起動のたびにQMK既定(有効)へ戻ってしまい、OFFに保存した意味が無くなるため。
    if (s.flags & KB_FLAG_COMBO) combo_enable();
    else combo_disable();
#endif
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
    set_auto_mouse_layer(s.aml_layer);
    set_auto_mouse_timeout(s.aml_timeout);
    set_auto_mouse_enable(!(s.flags & KB_FLAG_AML_DISABLE));
#endif
#ifdef KEYBALL_AML_THRESHOLD_RUNTIME
    kb_aml_threshold = s.aml_threshold;
#endif
#ifdef OS_DETECTION_ENABLE
    // 起動時点で既にOS判別が済んでいることがあるため、ここでも一度反映する
    // （判別前なら OS_UNSURE = 非Mac扱いで入れ替えなし）。
    kb_apply_os_swap(detected_host_os());
#endif
    // 起動直後、まだ一度もレイヤーを切り替えていない状態でも、スクロール/精密
    // モード/ジェスチャーの連動レイヤーがレイヤー0（ベースレイヤー）に設定されて
    // いれば正しく反映されるようにする（kb_apply_layer_features側のコメント参照）。
    kb_apply_layer_features(get_highest_layer(layer_state));
    (void)s;

    // this_have_ballはここまでの処理（keyboard_post_init_kbでのpmw3360_init()）で
    // 確定済みなので、g_led_config.matrix_coの行入れ替えが必要か判定・実行してから、
    // 以降is_keyboard_left()をthis_have_ball基準に切り替える。
#ifdef RGB_MATRIX_ENABLE
    kb_fixup_led_matrix_rows_if_needed();
#endif
    g_ball_probe_ready = true;
}

uint16_t get_tapping_term(uint16_t keycode, keyrecord_t *record) {
    uint16_t tt = kb_settings_get().tapping_term;
    return (tt >= 50 && tt <= 1000) ? tt : TAPPING_TERM;
}

bool get_permissive_hold(uint16_t keycode, keyrecord_t *record) {
    return (kb_settings_get().flags & KB_FLAG_PERMISSIVE_HOLD) != 0;
}

// レイヤー番号に応じてスクロール/精密モード/ジェスチャーの連動状態を反映する。
// layer_state_set_user()（レイヤーが実際に切り替わった時）だけでなく、
// keyboard_post_init_user()（起動直後、まだ一度もレイヤーを切り替えていない時）
// からも呼ぶための共通処理。
//
// 2026-09-11発覚: これらをlayer_state_set_user()の中でしか計算していなかったため、
// 連動レイヤーを「レイヤー0（ベースレイヤー）」に設定した場合だけ効かない不具合が
// あった。layer_state_set_user()はQMKがレイヤーの切り替えを検知した時にしか
// 呼ばれないコールバックで、起動してから一度もレイヤーを切り替えていなければ
// （＝レイヤー0にいるままなら）一度も実行されない。他のレイヤー（1以上）の場合は
// 「まだ計算していない＝連動なし」という初期値がそのレイヤーにいない状態と
// 偶然一致するため問題が表面化しなかったが、レイヤー0は起動直後から実際に
// 「そのレイヤーにいる」状態なので、初期値の「連動なし」のままズレてしまっていた。
static void kb_apply_layer_features(uint8_t hl) {
#if defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)
    // レイヤー連動LED: 有効化しているレイヤーに専用の光り方を設定していると、そのレイヤーに
    // いる間ずっとその光り方になる（抜けると通常のLED設定に戻る）。無効時・専用設定なし
    // レイヤーでは何もしない（通常のLED設定のまま）。実処理はkeyball.c側に集約している
    // （SET_LEDもここを経由するようにして、通常設定とレイヤー設定が混線しないようにするため）。
    keyball_apply_layer_led(hl);
#endif

    keyball_set_scroll_mode(kb_scroll_layer_get() == hl);  // 設定レイヤーでスクロール（なし=0xFEは一致しない）
    keyball_set_precision_layer(kb_precision_layer_get() == hl);  // 設定レイヤーで超低速モード
#ifdef GESTURE_ENABLE
    g_gst_layer_mode = -1;
    for (uint8_t i = 0; i < KB_GESTURE_MODE_COUNT; i++) {
        if (kb_gesture_mode_get(i).layer == hl) {
            g_gst_layer_mode = i;
            break;
        }
    }
    if (gst_active_mode() < 0) {
        g_gesture_acc_x = 0;
        g_gesture_acc_y = 0;
        g_gesture_cooldown = false;
    }
#ifdef CONSOLE_ENABLE
    dprintf("kb_debug: apply_layer_features hl=%u scroll_layer=%u scroll_mode=%u gst_layer_mode=%d gst_manual_mode=%d active_mode=%d\n",
            hl, kb_scroll_layer_get(), keyball_get_scroll_mode(), g_gst_layer_mode, g_gst_manual_mode, gst_active_mode());
#endif
#endif
}

layer_state_t layer_state_set_user(layer_state_t state) {
    kb_apply_layer_features(get_highest_layer(state));
    return state;
}

void matrix_scan_user(void) {
#ifdef RGBLIGHT_ENABLE
    // RGB_MATRIX版はハロウィン・イースターも通常の自作エフェクトとして実装しており
    // 外部から毎フレーム駆動する必要が無いため、この呼び出しはRGBLIGHT版のみでよい。
    keyball_seasonal_led_task();
#endif
}

// スクロール方向の反転（EEPROM設定に応じて符号を反転）
report_mouse_t pointing_device_task_user(report_mouse_t mouse_report) {
    // シェイク・ダブルフリックはジェスチャーモードの状態に関わらず常に生の移動量を見る
    // （ジェスチャー側で後段がmouse_report.x/yを0にする場合があるため、必ず先に呼ぶ）。
    shake_task(&mouse_report);
    dflick_task(&mouse_report);

#ifdef GESTURE_ENABLE
    int8_t gst_mode = gst_active_mode();
#ifdef CONSOLE_ENABLE
    if (mouse_report.x || mouse_report.y || mouse_report.h || mouse_report.v) {
        static uint16_t dbg_timer = 0;
        if (timer_elapsed(dbg_timer) > 300) {
            dbg_timer = timer_read();
            dprintf("kb_debug: motion x=%d y=%d h=%d v=%d gst_mode=%d scroll_mode=%u\n",
                    mouse_report.x, mouse_report.y, mouse_report.h, mouse_report.v, gst_mode, keyball_get_scroll_mode());
        }
    }
#endif
    if (gst_mode >= 0) {
        if (g_gesture_cooldown) {
            // 単発方向発火直後のクールダウン中は溜め込まない（1スイングで連続発火しない）
            if (timer_elapsed(g_gesture_cd_timer) > GST_COOLDOWN_MS) {
                g_gesture_cooldown = false;
            } else {
                g_gesture_acc_x = 0;
                g_gesture_acc_y = 0;
                mouse_report.x = 0;
                mouse_report.y = 0;
                return mouse_report;
            }
        }
        g_gesture_acc_x += mouse_report.x;
        g_gesture_acc_y += mouse_report.y;
        uint8_t th_h = kb_gesture_th_h_get();  // 横方向（左右）のしきい値
        uint8_t th_v = kb_gesture_th_v_get();  // 縦方向（上下）のしきい値
        if (g_gesture_acc_x > th_h || g_gesture_acc_x < -th_h ||
            g_gesture_acc_y > th_v || g_gesture_acc_y < -th_v) {
            int16_t ax = g_gesture_acc_x < 0 ? -g_gesture_acc_x : g_gesture_acc_x;
            int16_t ay = g_gesture_acc_y < 0 ? -g_gesture_acc_y : g_gesture_acc_y;
            bool    horizontal = ax > ay;
            uint8_t dir = horizontal
                ? (g_gesture_acc_x > 0 ? 3 : 2)   // 右:3 左:2
                : (g_gesture_acc_y > 0 ? 1 : 0);  // 下:1 上:0

            kb_gesture_mode_t m    = kb_gesture_mode_get(gst_mode);
            uint16_t          kc   = m.key[dir];
            // SCRL_TO/SCRL_MOはスクロールモードのON/OFFを反転させるトグル系キーで、
            // 押すたびに状態が入れ替わる（=何度も撃つと結果が予測できない）。
            // 「連続入力」はクールダウン無しで動いている間ずっと撃ち続けるため、
            // これらに設定するとスクロールモードが偶奇不定で固定化してしまい、
            // 他のレイヤーに移ってもレイヤー連動の値へ戻らなくなる不具合があった
            // （本人からの報告: レイヤー2にスクロールを設定していないのにスクロール
            // してしまう。別レイヤーのジェスチャーの連続入力をOFFにしたら直った）。
            // そのためこの2キーだけは「連続入力」指定を無視し、常に単発扱いにする。
            bool              cont = ((m.continuous >> dir) & 1) && kc != SCRL_TO && kc != SCRL_MO;
            if (kc) {
                kb_fire_keycode(kc);
                keyball_gesture_wave_trigger(dir);  // 未割当方向(kc==0)では発動させない
            }

            if (cont) {
                // 連続入力: しきい値分だけ引いて余りを持ち越す（クールダウンなし）。
                // 速く回すほど短い間隔で再発火するので、回転速度に連動した連続入力になる。
                if (horizontal) g_gesture_acc_x -= (g_gesture_acc_x > 0 ? th_h : -th_h);
                else            g_gesture_acc_y -= (g_gesture_acc_y > 0 ? th_v : -th_v);
            } else {
                g_gesture_acc_x    = 0;
                g_gesture_acc_y    = 0;
                g_gesture_cooldown = true;
                g_gesture_cd_timer = timer_read();
            }
        }
        mouse_report.x = 0;  // ジェスチャー中はカーソルを動かさない
        mouse_report.y = 0;
        return mouse_report;
    }
#endif
    if (mouse_report.h != 0 || mouse_report.v != 0) {
        uint8_t flags = kb_settings_get().flags;
        if (flags & KB_FLAG_SCROLL_INV_V) mouse_report.v = -mouse_report.v;
        if (flags & KB_FLAG_SCROLL_INV_H) mouse_report.h = -mouse_report.h;
    }
    return mouse_report;
}

void raw_hid_receive(uint8_t *data, uint8_t length) {
    kb_hid_receive(data, length);
}

#ifdef TAP_DANCE_ENABLE

static uint8_t td_outcome[TD_SLOT_COUNT];
#define TD_OUT_TAP  1
#define TD_OUT_HOLD 2
#define TD_OUT_DTAP 3

// register_code16()/unregister_code16()はTG/MO/OSMのような量子キーコードを正しく
// 扱えず下位1バイトだけを生キーコードとして送ってしまう問題があった
// （kb_synth_keyeventのコメント参照）。以前はMO/TGだけを個別にレイヤーAPIへ
// 振り分けて対処していたが、kb_fire_keycodeと同じくQMK標準のCombo機能に倣い、
// 合成キーイベントをaction_tapping_process()に渡す方式に統一した。押す/離すを
// 別タイミングで呼べるので、hold中はレイヤーやMod-Tapが「押されっぱなし」の
// 状態を正しく維持できる（MO/TG/OSM/OSLはもちろん、通常のキー・Mod+キー・
// レイヤータップなど何を割り当てても実際のキー入力と同じに動く）。
static void td_handle_finished(tap_dance_state_t *state, uint8_t idx) {
    td_slot_t s = td_config_get(idx);
    if (state->count == 1 && !state->pressed) {
        td_outcome[idx] = TD_OUT_TAP;
        kb_synth_keyevent(s.tap, true);
    } else if (state->count == 1 && state->pressed) {
        td_outcome[idx] = TD_OUT_HOLD;
        kb_synth_keyevent(s.hold, true);
    } else {
        td_outcome[idx] = TD_OUT_DTAP;
        kb_synth_keyevent(s.dtap ? s.dtap : s.tap, true);
    }
}

static void td_handle_reset(tap_dance_state_t *state, uint8_t idx) {
    td_slot_t s = td_config_get(idx);
    switch (td_outcome[idx]) {
        case TD_OUT_TAP:  kb_synth_keyevent(s.tap, false);  break;
        case TD_OUT_HOLD: kb_synth_keyevent(s.hold, false); break;
        case TD_OUT_DTAP: kb_synth_keyevent(s.dtap ? s.dtap : s.tap, false); break;
    }
    td_outcome[idx] = 0;
}

#define DEFINE_TD_FN(n) \
    static void td_fin_##n(tap_dance_state_t *s, void *u) { td_handle_finished(s, n); } \
    static void td_rst_##n(tap_dance_state_t *s, void *u) { td_handle_reset(s, n); }

DEFINE_TD_FN(0) DEFINE_TD_FN(1) DEFINE_TD_FN(2) DEFINE_TD_FN(3)
DEFINE_TD_FN(4) DEFINE_TD_FN(5) DEFINE_TD_FN(6) DEFINE_TD_FN(7)

tap_dance_action_t tap_dance_actions[] = {
    [0] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_fin_0, td_rst_0),
    [1] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_fin_1, td_rst_1),
    [2] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_fin_2, td_rst_2),
    [3] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_fin_3, td_rst_3),
    [4] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_fin_4, td_rst_4),
    [5] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_fin_5, td_rst_5),
    [6] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_fin_6, td_rst_6),
    [7] = ACTION_TAP_DANCE_FN_ADVANCED(NULL, td_fin_7, td_rst_7),
};

#endif // TAP_DANCE_ENABLE

#ifdef COMBO_ENABLE
// コンボ（2026-09-10、本人希望で再有効化。2026-09-10、さらにWeb UIから組み合わせを
// 編集できるようにした）。key_combos[]の実体はkb_combo.c側が管理し、EEPROMの内容を
// keyboard_post_init_user()（本ファイル内）でkb_combo_init()を呼んで反映する。
// ここでは各スロットの入れ物（keys配列の先頭ポインタとkeycode）だけを確保する。
combo_t key_combos[KB_COMBO_SLOT_COUNT];
#endif // COMBO_ENABLE

#ifdef OLED_ENABLE
#    include "lib/oledkit/oledkit.h"
#    include "lib/oledkit/anim_frames.h"
void oledkit_render_info_user(void) {
    keyball_oled_render_keyinfo();
    keyball_oled_render_ballinfo();
    keyball_oled_render_layerinfo();
}

#if defined(RGB_MATRIX_ENABLE) && defined(RGB_MATRIX_CUSTOM_USER)
// LEDエフェクトが季節イベント（クリスマス/ハロウィン/イースター/トゥインクル）の間だけ、
// ロゴの代わりに対応する全画面アニメーションを表示する（2026-09-17、本人リクエスト）。
//
// 以前はクリスマスのみ oled_write_pixel() で手描きしたツリーを表示していたが、
// 本人が用意した実写ベースのコマ送り画像（90フレーム/15fps/6秒ループ、
// 128x32の全画面を1枚のバイト列にしたもの）に差し替えた。frames[idx]を
// そのままフレームバッファへ転送するだけなので、手描きより滑らかで、
// 4エフェクト分を同じ処理で共通化できる。
static void render_seasonal_anim_oled(const uint8_t frames[][KB_ANIM_FRAME_BYTES]) {
    uint8_t idx = (uint8_t)((timer_read() / KB_ANIM_FRAME_MS) % KB_ANIM_FRAME_COUNT);
    oled_set_cursor(0, 0);
    oled_write_raw_P((const char *)frames[idx], KB_ANIM_FRAME_BYTES);
}
#endif

// スレーブ側（今まで静止ロゴだった側）のロゴを常時アニメーションにする
// （2026-09-10、OLEDリッチ化: まずはシンプルな常時アニメから）。
// oledkit.cの既定実装(静止表示)を上書きする。
//
// このOLEDは1ビット(白/消灯のみ)でグレースケールが無いため、本物の輝度フェード
// はできない（ディザで代替を試したが、本人が「フェードはできないんですね」と
// 了承の上で左右に揺らす動きへ方向転換を指示）。そこでロゴ全体の左余白を
// 三角波で往復させ、ロゴが左右にゆらゆら揺れる動きにしている。振れ幅は
// 元の静止表示の余白(BASE_MARGIN=2文字)を中心に±AMPLITUDE文字とし、
// 画面右端をはみ出さない範囲に収めている。
void oledkit_render_logo_user(void) {
    static bool was_handled = false;
    bool        handled     = false;
#if defined(RGB_MATRIX_ENABLE) && defined(RGB_MATRIX_CUSTOM_USER)
    switch (rgb_matrix_get_mode()) {
        case RGB_MATRIX_CUSTOM_CHRISTMAS: render_seasonal_anim_oled(anim_xmas); handled = true; break;
        case RGB_MATRIX_CUSTOM_HALLOWEEN: render_seasonal_anim_oled(anim_hallow); handled = true; break;
        case RGB_MATRIX_CUSTOM_EASTER: render_seasonal_anim_oled(anim_easter); handled = true; break;
        case RGB_MATRIX_CUSTOM_TWINKLE: render_seasonal_anim_oled(anim_twinkle); handled = true; break;
        default: break;
    }
#endif

    // 季節アニメーション→ロゴに戻った直後だけ、画面全体を一旦クリアする（本人指摘・
    // 2026-09-18）。下のロゴ描画はoled_write_charで3行分(24px)しか触れないため、
    // アニメーションが使っていた4段目(下8px、128x32のうち一度も上書きされない領域)に
    // 前のフレームの残像が残ったままになっていた。切り替わった瞬間の1回だけで済むよう
    // 毎フレームではなくwas_handledとの比較で遷移エッジのみ検出している。
    if (!handled && was_handled) {
        oled_clear();
    }
    was_handled = handled;

    if (!handled) {
        const uint16_t PERIOD_MS   = 3200;  // 左右1往復にかかる時間
        const int8_t   AMPLITUDE   = 2;     // 左右に振れる最大文字数
        const uint8_t  BASE_MARGIN = 2;     // 元の静止表示と同じ基準余白
        uint16_t       t           = timer_read() % PERIOD_MS;

        // -AMPLITUDE 〜 +AMPLITUDE を往復する三角波
        int16_t swing;
        if (t < PERIOD_MS / 2) {
            swing = (int16_t)(-AMPLITUDE + (int32_t)t * (2 * AMPLITUDE) / (PERIOD_MS / 2));
        } else {
            swing = (int16_t)(AMPLITUDE - (int32_t)(t - PERIOD_MS / 2) * (2 * AMPLITUDE) / (PERIOD_MS / 2));
        }
        uint8_t margin = (uint8_t)(BASE_MARGIN + swing);

        char ch = 0x80;
        for (int y = 0; y < 3; y++) {
            for (uint8_t i = 0; i < margin; i++) {
                oled_write_char(' ', false);
            }
            for (int x = 0; x < 16; x++) {
                oled_write_char(ch, false);
                ch++;
            }
            // 余白の変化で前フレームの残像が残らないよう、行の残りを消す
            oled_advance_page(true);
        }
    }
}
#endif
