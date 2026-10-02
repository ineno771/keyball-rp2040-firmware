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
// LED配置。【暫定・仮置き】2026-10-02時点では実機の配線情報が未入手のため、全LEDを
// キー連動扱い・キーとの対応なし(NO_LED)・座標は左右に等間隔で並べただけの仮の値にして
// ある（ソリッド・ブリージング等の全体点灯系は動くが、リアクティブ等のキー反応系・
// アンダーグローの区別・波紋の位置は正しく動かない）。本人から配線順を受け取り次第、
// Keyball+（keyballplus.c）と同じ形式で確定値に置き換えること。
// LEDインデックスの並び: ボール搭載側29個(idx0-28) → 非搭載側30個(idx29-58)
// （config.hのRGB_MATRIX_SPLIT { 29, 30 }と対応。ボール搭載側を
// is_keyboard_left()=true側として扱う前提はKeyball+と同じ。keymap.c参照）。
led_config_t g_led_config = {
    {
        // キーマトリクス(8行 x 6列) → LEDインデックス（暫定: 対応なし）
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    },
    {
        // LEDインデックス → 物理座標(x,y)（暫定）
        {0,32}, {3,32}, {7,32}, {10,32}, {14,32}, {17,32}, {21,32}, {25,32}, {28,32}, {32,32},
        {35,32}, {39,32}, {42,32}, {46,32}, {50,32}, {53,32}, {57,32}, {60,32}, {64,32}, {67,32},
        {71,32}, {75,32}, {78,32}, {82,32}, {85,32}, {89,32}, {92,32}, {96,32}, {100,32}, {124,32},
        {127,32}, {130,32}, {134,32}, {137,32}, {141,32}, {144,32}, {148,32}, {151,32}, {155,32}, {158,32},
        {161,32}, {165,32}, {168,32}, {172,32}, {175,32}, {179,32}, {182,32}, {186,32}, {189,32}, {192,32},
        {196,32}, {199,32}, {203,32}, {206,32}, {210,32}, {213,32}, {217,32}, {220,32}, {224,32},
    },
    {
        // LEDインデックス → フラグ（暫定: 全てキー連動扱い）
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
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
