/*
Copyright 2022 MURAOKA Taro (aka KoRoN, @kaoriya)

This program is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 2 of the License, or
(at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program.  If not, see <http://www.gnu.org/licenses/>.
*/

#pragma once

//////////////////////////////////////////////////////////////////////////////
// Configurations

#ifndef KEYBALL_CPI_DEFAULT
#    define KEYBALL_CPI_DEFAULT 500
#endif

#ifndef KEYBALL_SCROLL_DIV_DEFAULT
#    define KEYBALL_SCROLL_DIV_DEFAULT 4 // 4: 1/8 (1/2^(n-1))
#endif

// スクロール量の全体基準（大きいほど全段階が遅くなる）。1=従来通り。
#ifndef KEYBALL_SCROLL_DIV_BASE
#    define KEYBALL_SCROLL_DIV_BASE 1
#endif

#ifndef KEYBALL_REPORTMOUSE_INTERVAL
#    define KEYBALL_REPORTMOUSE_INTERVAL 8 // mouse report rate: 125Hz
#endif

#ifndef KEYBALL_SCROLLBALL_INHIVITOR
#    define KEYBALL_SCROLLBALL_INHIVITOR 50
#endif

/// To disable scroll snap feature, define 0 in your config.h
#ifndef KEYBALL_SCROLLSNAP_ENABLE
#    define KEYBALL_SCROLLSNAP_ENABLE 2
#endif

#ifndef KEYBALL_SCROLLSNAP_RESET_TIMER
#    define KEYBALL_SCROLLSNAP_RESET_TIMER 100
#endif

#ifndef KEYBALL_SCROLLSNAP_TENSION_THRESHOLD
#    define KEYBALL_SCROLLSNAP_TENSION_THRESHOLD 12
#endif

/// Specify SROM ID to be uploaded PMW3360DW (optical sensor).  It will be
/// enabled high CPI setting or so.  Valid valus are 0x04 or 0x81.  Define this
/// in your config.h to be enable.  Please note that using this option will
/// increase the firmware size by more than 4KB.
//#define KEYBALL_PMW3360_UPLOAD_SROM_ID 0x04
//#define KEYBALL_PMW3360_UPLOAD_SROM_ID 0x81

/// Defining this macro keeps two functions intact: keycode_config() and
/// mod_config() in keycode_config.c.
///
/// These functions customize the magic key code and are useless if the magic
/// key code is disabled.  Therefore, Keyball automatically disables it.
/// However, there may be cases where you still need these functions even after
/// disabling the magic key code. In that case, define this macro.
//#define KEYBALL_KEEP_MAGIC_FUNCTIONS

//////////////////////////////////////////////////////////////////////////////
// Constants

#define KEYBALL_TX_GETINFO_INTERVAL 500
#define KEYBALL_TX_GETMOTION_INTERVAL 4

#if (PRODUCT_ID & 0xff00) == 0x0000
#    define KEYBALL_MODEL 46
#elif (PRODUCT_ID & 0xff00) == 0x0100
#    define KEYBALL_MODEL 61
#elif (PRODUCT_ID & 0xff00) == 0x0200
#    define KEYBALL_MODEL 39
#elif (PRODUCT_ID & 0xff00) == 0x0300
#    define KEYBALL_MODEL 147
#elif (PRODUCT_ID & 0xff00) == 0x0400
#    define KEYBALL_MODEL 44
#elif (PRODUCT_ID & 0xff00) == 0x0600
// keyball-rp2040-firmware（Keyball39 RP2040版）。基板形状はKeyball39と同一のため39として扱う。
#    define KEYBALL_MODEL 39
#elif (PRODUCT_ID & 0xff00) == 0x0700
// keyball-rp2040-firmware（Keyball+ RP2040版）。基板形状はAVR版Keyball+と同一のため139として扱う。
#    define KEYBALL_MODEL 139
#endif

#define KEYBALL_OLED_MAX_PRESSING_KEYCODES 6

//////////////////////////////////////////////////////////////////////////////
// Types

enum keyball_keycodes {
    KBC_RST  = QK_KB_0, // Keyball configuration: reset to default
    KBC_SAVE = QK_KB_1, // Keyball configuration: save to EEPROM

    CPI_I100 = QK_KB_2, // CPI +100 CPI
    CPI_D100 = QK_KB_3, // CPI -100 CPI
    CPI_I1K  = QK_KB_4, // CPI +1000 CPI
    CPI_D1K  = QK_KB_5, // CPI -1000 CPI

    // In scroll mode, motion from primary trackball is treated as scroll
    // wheel.
    SCRL_TO  = QK_KB_6, // Toggle scroll mode
    SCRL_MO  = QK_KB_7, // Momentary scroll mode
    SCRL_DVI = QK_KB_8, // Increment scroll divider
    SCRL_DVD = QK_KB_9, // Decrement scroll divider

    SSNP_VRT = QK_KB_13, // Set scroll snap mode as vertical
    SSNP_HOR = QK_KB_14, // Set scroll snap mode as horizontal
    SSNP_FRE = QK_KB_15, // Set scroll snap mode as disable (free scroll)

    // Auto mouse layer control keycodes.
    // Only works when POINTING_DEVICE_AUTO_MOUSE_ENABLE is defined.
    AML_TO   = QK_KB_10, // Toggle automatic mouse layer
    AML_I50  = QK_KB_11, // Increment automatic mouse layer timeout
    AML_D50  = QK_KB_12, // Decrement automatic mouse layer timeout

    // ジェスチャー: 押している間トラックボールを上下左右に振るとキー送出。
    // GST_HOLD〜4はジェスチャーモード1〜4の手動切替キー（押している間だけ
    // レイヤー連動より優先。各モードの中身はKeyball Linkのジェスチャー設定で変更）
    GST_HOLD  = QK_KB_16,
    GST_HOLD2 = QK_KB_19,
    GST_HOLD3 = QK_KB_20,
    GST_HOLD4 = QK_KB_21,

    // 超低速（精密作業）モード: 押している間だけCPIを下げる
    PRC_MO   = QK_KB_17,

    // タイムアウトを待たず自動マウスレイヤーを即座に解除
    AML_OFF  = QK_KB_18,

    // User customizable 32 keycodes.
    KEYBALL_SAFE_RANGE = QK_USER_0,
};

typedef union {
    uint32_t raw;
    struct {
        uint8_t cpi : 7;
        uint8_t sdiv : 3;  // scroll divider
#ifdef POINTING_DEVICE_AUTO_MOUSE_ENABLE
        uint8_t amle : 1;  // automatic mouse layer enabled
        uint16_t amlto : 5; // automatic mouse layer timeout
#endif
#if KEYBALL_SCROLLSNAP_ENABLE == 2
        uint8_t ssnap : 2; // scroll snap mode
#endif
        uint8_t accel : 4; // pointer acceleration (0=off, 1-10)
        uint8_t magic : 8; // レイアウト検証用（kb_settings.cと同じ仕組み）。
                            // フィールドを追加・変更したら必ず値を変える。
    };
} keyball_config_t;

// 単純な連番ではなく偶然一致しにくい値にする（kb_settings.cのMAGIC_VALUEと同様の考え方）
#define KEYBALL_CONFIG_MAGIC 0x5B

typedef struct {
    uint8_t ballcnt; // count of balls: support only 0 or 1, for now
} keyball_info_t;

typedef struct {
    int16_t x;
    int16_t y;
} keyball_motion_t;

typedef uint8_t keyball_cpi_t;

typedef enum {
    KEYBALL_SCROLLSNAP_MODE_VERTICAL   = 0,
    KEYBALL_SCROLLSNAP_MODE_HORIZONTAL = 1,
    KEYBALL_SCROLLSNAP_MODE_FREE       = 2,
} keyball_scrollsnap_mode_t;

typedef struct {
    bool this_have_ball;
    bool that_enable;
    bool that_have_ball;

    keyball_motion_t this_motion;
    keyball_motion_t that_motion;

    uint8_t cpi_value;
    bool    cpi_changed;

    bool     scroll_mode;
    uint32_t scroll_mode_changed;
    uint8_t  scroll_div;

#if KEYBALL_SCROLLSNAP_ENABLE == 1
    uint32_t scroll_snap_last;
    int8_t   scroll_snap_tension_h;
#elif KEYBALL_SCROLLSNAP_ENABLE == 2
    keyball_scrollsnap_mode_t scrollsnap_mode;
#endif

    uint16_t       last_kc;
    keypos_t       last_pos;
    report_mouse_t last_mouse;

    // Buffer to indicate pressing keys.
    char pressing_keys[KEYBALL_OLED_MAX_PRESSING_KEYCODES + 1];

#ifdef GESTURE_ENABLE
    // ジェスチャー連動LEDウェーブ用のトリガー記録（RIPPLEと同じ固定スロット方式）。
    // 両ハーフで同時に光らせるため、マスター側はkeyball_gesture_wave_trigger()で
    // ローカルに記録すると同時に一方向RPC(KEYBALL_GESTURE_WAVE)でスレーブにも伝える。
    // rgb_matrix_user.incのGESTURE_WAVEエフェクトが読み取り専用で参照する。
#define KEYBALL_GESTURE_WAVE_SLOT_COUNT 6
    uint16_t gesture_wave_start[KEYBALL_GESTURE_WAVE_SLOT_COUNT];
    uint8_t  gesture_wave_dir[KEYBALL_GESTURE_WAVE_SLOT_COUNT];  // 0=上 1=下 2=左 3=右
    bool     gesture_wave_active[KEYBALL_GESTURE_WAVE_SLOT_COUNT];
    // ウェーブの速さ（kb_gesture_wave_speed_getと同じ0-255）。2026-09-09発覚:
    // 分割両ハーフはそれぞれ自分のEEPROMを持っており、Web UIのSET系コマンドは
    // USB接続中の片方（＝マスター）にしか届かないため、kb_gesture_wave_speed_get()を
    // スレーブ側でそのまま呼ぶとマスターと異なる（未設定/古い）値を読んでしまい、
    // ウェーブの速さが片方のハーフだけ違って見える不具合になっていた。そのため
    // 発火のたびにマスターが読んだ実際の値をこのフィールドへ書き込み、
    // KEYBALL_GESTURE_WAVE RPCの一部としてスレーブにも配って共有する
    // （rgb_matrix_user.incはkb_gesture_wave_speed_get()を直接呼ばず、これを読む）。
    uint8_t gesture_wave_speed;
#endif
} keyball_t;

typedef enum {
    KEYBALL_ADJUST_PENDING   = 0,
    KEYBALL_ADJUST_PRIMARY   = 1,
    KEYBALL_ADJUST_SECONDARY = 2,
} keyball_adjust_t;

//////////////////////////////////////////////////////////////////////////////
// Exported values (touch carefully)

extern keyball_t keyball;

//////////////////////////////////////////////////////////////////////////////
// Hook points

/// keyball_on_adjust_layout is called when the keyboard layout adjustted
void keyball_on_adjust_layout(keyball_adjust_t v);

/// keyball_on_apply_motion_to_mouse_move applies trackball's motion m to r as
/// mouse movement.
/// You can change the default algorithm by override this function.
void keyball_on_apply_motion_to_mouse_move(keyball_motion_t *m, report_mouse_t *r, bool is_left);

/// keyball_on_apply_motion_to_mouse_scroll applies trackball's motion m to r
/// as mouse scroll.
/// You can change the default algorithm by override this function.
void keyball_on_apply_motion_to_mouse_scroll(keyball_motion_t *m, report_mouse_t *r, bool is_left);

//////////////////////////////////////////////////////////////////////////////
// Public API functions

/// keyball_oled_render_ballinfo renders ball information to OLED.
/// It uses just 21 columns to show the info.
void keyball_oled_render_ballinfo(void);

/// keyball_oled_render_keyinfo renders last processed key information to OLED.
/// It shows column, row, key code, and key name (if available).
void keyball_oled_render_keyinfo(void);

/// keyball_oled_render_layerinfo renders current layer status information to
/// OLED.  It shows layer mask with number (1~f) for active layers and '_' for
/// inactive layers.
void keyball_oled_render_layerinfo(void);

/// keyball_get_scroll_mode gets current scroll mode.
bool keyball_get_scroll_mode(void);

/// keyball_set_scroll_mode modify scroll mode.
void keyball_set_scroll_mode(bool mode);

/// keyball_get_precision_mode gets whether ultra-slow (precision) mode is currently active.
bool keyball_get_precision_mode(void);

/// keyball_set_precision_key reflects the PRC_MO key's press/release state into precision mode.
/// Combined with keyball_set_precision_layer via OR: precision mode is active while either
/// source requests it, so holding the key while the precision layer is also active (or vice
/// versa) does not prematurely restore the saved CPI.
void keyball_set_precision_key(bool pressed);

/// keyball_set_precision_layer reflects whether the configured precision-mode layer is
/// currently the highest active layer. See keyball_set_precision_key for how the two sources
/// combine.
void keyball_set_precision_layer(bool on);

#ifdef GESTURE_ENABLE
/// keyball_gesture_wave_trigger records a gesture-fire event (direction: 0=up,1=down,2=left,
/// 3=right, matching keymap.c's gesture direction encoding) for the GESTURE_WAVE LED effect,
/// and — when called on the master — also forwards it to the slave half via a one-way split
/// transaction so both halves animate together. Call only when a real key was actually sent
/// by the gesture (kc != 0); an unassigned direction shouldn't trigger the effect.
void keyball_gesture_wave_trigger(uint8_t direction);

#ifdef RGB_MATRIX_ENABLE
// gesture_wave_active[]のいずれかが発火中かどうかを見て、現在のRGB_MATRIXモードを
// 強制的にGESTURE_WAVEへ上書き/復帰する。通常LED・レイヤー連動LEDのどちらが選ばれて
// いても関係なく、ジェスチャー発火の瞬間だけウェーブを最優先で表示するための仕組み
// （2026-09-09発覚: ウェーブをLED効果の選択肢の1つとして実装していた旧方式だと、
// レイヤー連動LEDが別の効果を選んでいる間はウェーブ用のRGB_MATRIXモードに切り替わって
// いないため描画関数自体が呼ばれず、「ジェスチャーとレイヤー連動LEDがぶつかる」問題に
// なっていた）。両ハーフ独立に動作するため、housekeeping_task_kbから両ハーフで
// 毎スキャン呼ぶこと。
void keyball_gesture_wave_task(void);

/// keyball_gesture_wave_overriding reports whether the gesture-wave LED effect is currently
/// being force-displayed (overriding the normal/layer-linked LED). kb_hid.c's UG_*キー同期
/// 処理（kb_led_config_sync_from_rgb_matrix）が、ウェーブの一時的な色をkb_led_configへ
/// 誤って書き込まないようにするために使う。
bool keyball_gesture_wave_overriding(void);
#endif
#endif

#ifdef RGBLIGHT_ENABLE
/// keyball_seasonal_led_task renders the seasonal LED effects (Halloween/Easter crossfade)
/// whenever the currently-effective LED config (global, or the active layer's override)
/// selects one. It is self-contained: it independently re-derives the effective config every
/// call from EEPROM + the local layer_state, so it works correctly on both split halves
/// without needing a dedicated sync channel. No-op when the effective effect_id is a regular
/// RGBLIGHT mode (rgblight_task() already handles that).
/// RGBLIGHT-only: the RGB_MATRIX build implements Halloween/Easter as ordinary self-contained
/// custom effects (rgb_matrix_user.inc) instead, so it doesn't need this external driver.
/// Call every matrix_scan_user tick.
void keyball_seasonal_led_task(void);
#endif

#if defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)
/// keyball_apply_normal_led applies the stored "normal" (layer 0) LED config to the live
/// RGBLIGHT/RGB_MATRIX display right now (via the _noeeprom variants — kb_led_config in
/// kb_settings is the persisted source of truth, not the backend's own eeconfig). Call at
/// boot, and after SET_LED when no layer override is currently active.
void keyball_apply_normal_led(void);

/// keyball_apply_layer_led reflects the layer-linked LED feature for the given highest active
/// layer: applies that layer's override live if enabled, otherwise (including when the
/// feature itself is off) reverts to the normal LED config via keyball_apply_normal_led().
/// Call from layer_state_set_user with get_highest_layer(state).
void keyball_apply_layer_led(uint8_t hl);

/// keyball_layer_led_overriding reports whether a layer's LED override is currently being
/// displayed (as opposed to the normal/layer-0 config). SET_LED uses this to decide whether
/// changing the normal config should also change what's currently on screen.
bool keyball_layer_led_overriding(void);
#endif // defined(RGBLIGHT_ENABLE) || defined(RGB_MATRIX_ENABLE)

/// keyball_get_scrollsnap_mode gets current scroll snap mode.
keyball_scrollsnap_mode_t keyball_get_scrollsnap_mode(void);

/// keyball_set_scrollsnap_mode change scroll snap mode.
void keyball_set_scrollsnap_mode(keyball_scrollsnap_mode_t mode);

/// keyball_get_scroll_div gets current scroll divider.
/// See also keyball_set_scroll_div for the scroll divider's detail.
uint8_t keyball_get_scroll_div(void);

/// keyball_set_scroll_div changes scroll divider.
///
/// The scroll divider is the number that divides the raw value when applying
/// trackball motion to scrolling.  The CPI value of the trackball is very
/// high, so if you apply it to scrolling as is, it will scroll too much.
/// In order to adjust the scroll amount to be appropriate, it is applied after
/// dividing it by a scroll divider.  The actual denominator is determined by
/// the following formula:
///
///   denominator = 2 ^ (div - 1) ^2
///
/// Valid values are between 1 and 7, KEYBALL_SCROLL_DIV_DEFAULT is used when 0
/// is specified.
void keyball_set_scroll_div(uint8_t div);

/// keyball_get_cpi gets current CPI of trackball.
/// The actual CPI value is the returned value multiplied by 100:
///
///     CPI = v * 100
uint8_t keyball_get_cpi(void);

/// keyball_set_cpi changes CPI of trackball.
/// Valid values are 0 to 120. If it is 0, KEYBALL_CPI_DEFAULT will be used,
/// otherwise the actual CPI value will be the set value multiplied by 100:
///
///     CPI = v * 100
///
/// In addition, if you do not upload SROM, the maximum value will be limited
/// to 35 (3500CPI).
void keyball_set_cpi(uint8_t cpi);

/// keyball_get_accel gets current pointer acceleration level (0=off, 1-10).
uint8_t keyball_get_accel(void);

/// keyball_set_accel sets pointer acceleration level (0=off, 1-10).
void keyball_set_accel(uint8_t accel);
