/*
Copyright 2022 @Yowkees
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

#include QMK_KEYBOARD_H

#include "lib/keyball/keyball.h"

//////////////////////////////////////////////////////////////////////////////

// clang-format off
matrix_row_t matrix_mask[MATRIX_ROWS] = {
    0b00111111,
    0b00111111,
    0b00111111,
    0b00111110,
    0b00111111,
    0b00111111,
    0b00111111,
    0b00111110,
};
// clang-format on

#ifdef RGB_MATRIX_ENABLE
// LED配置（2026-10-05、本人が右手ボールの実機で「LED位置実測（開発用）」を使い1個ずつ
// 確認した配線情報に基づく確定値）。
//
// ボール搭載側（29個、idx0-28）: アンダーグロー10個(idx0-9)→キー連動19個(idx10-28)。
//   キー連動は列ごとに外側から「列0(R00,R10,R20)→列1(R01,R11,R21)→R31→列2→列3→列4→列5」。
//   親指のR34・R35はLEDなし（近くにアンダーグローidx0・1がある）。
// 非搭載側（30個、idx29-58）: キー連動20個(idx29-48)→アンダーグロー10個(idx49-58)。
//   キー連動は内側の列5から外側へ「列5→列4→列3→L32→列2→L31→列1→列0」。
//   親指のL33〜L35はLEDなし（真上にアンダーグローidx56-58がある）。
//
// matrix_coの行0-3=ボール搭載側・行4-7=非搭載側（Keyball+と同じ。ボールが基板本来の
// is_keyboard_left()=false側にある場合はkeymap.cのkb_fixup_led_matrix_rows_if_needed()が
// 行を入れ替える）。物理座標(x,y)はキー配列から算出した概算値（アンダーグローは本人申告の
// 位置から）で、x方向はKeyball39/+と同じく各ハーフの外側の列をボール側はx小・非搭載側は
// x大に置く規則（ジェスチャーウェーブの左右判定がこの規則前提）。
led_config_t g_led_config = {
    {
        // キーマトリクス(8行 x 6列) → LEDインデックス
        {     10,     13,     17,     20,     23,     26 },
        {     11,     14,     18,     21,     24,     27 },
        {     12,     15,     19,     22,     25,     28 },
        { NO_LED,     16, NO_LED, NO_LED, NO_LED, NO_LED },
        {     46,     43,     39,     35,     32,     29 },
        {     47,     44,     40,     36,     33,     30 },
        {     48,     45,     41,     37,     34,     31 },
        { NO_LED,     42,     38, NO_LED, NO_LED, NO_LED },
    },
    {
        // LEDインデックス(0-58) → 物理座標(x,y)
        {75,56}, {60,56}, {60,16}, {45,32}, {15,56}, {0,16}, {0,0}, {8,0}, {38,0}, {53,0},
        {0,0}, {0,16}, {0,32}, {15,0}, {15,16}, {15,32}, {15,48}, {30,0}, {30,16}, {30,32},
        {45,0}, {45,16}, {45,32}, {60,0}, {60,16}, {60,32}, {75,0}, {75,16}, {75,32}, {149,0},
        {149,16}, {149,32}, {164,0}, {164,16}, {164,32}, {179,0}, {179,16}, {179,32}, {194,48}, {194,0},
        {194,16}, {194,32}, {209,48}, {209,0}, {209,16}, {209,32}, {224,0}, {224,16}, {224,32}, {157,0},
        {187,0}, {216,0}, {224,8}, {224,24}, {202,40}, {187,40}, {179,44}, {164,44}, {149,44},
    },
    {
        // LEDインデックス → フラグ
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    },
};
#endif

void keyball_on_adjust_layout(keyball_adjust_t v) {
#ifdef RGBLIGHT_ENABLE
    // adjust RGBLIGHT's clipping and effect ranges
    uint8_t lednum_this = keyball.this_have_ball ? 29 : 30;
    uint8_t lednum_that = !keyball.that_enable ? 0 : keyball.that_have_ball ? 29 : 30;
    rgblight_set_clipping_range(is_keyboard_left() ? 0 : lednum_that, lednum_this);
    rgblight_set_effect_range(0, lednum_this + lednum_that);
#endif
}
