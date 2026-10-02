/*
Copyright 2021 @Yowkees
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
    0b01110111,
    0b01110111,
    0b01110111,
    0b11110111,
    0b11110111,
    0b01110111,
    0b01110111,
    0b01110111,
    0b11110111,
    0b11110111,
};
// clang-format on

#ifdef RGB_MATRIX_ENABLE
// LED配置。【暫定・仮置き】2026-10-02時点では実機の配線情報が未入手のため、全LEDを
// キー連動扱い・キーとの対応なし(NO_LED)・座標は左右に等間隔で並べただけの仮の値にして
// ある（ソリッド・ブリージング等の全体点灯系は動くが、リアクティブ等のキー反応系・
// アンダーグローの区別・波紋の位置は正しく動かない）。本人から配線順を受け取り次第、
// Keyball+（keyballplus.c）と同じ形式で確定値に置き換えること。
// LEDインデックスの並び: ボール搭載側34個(idx0-33) → 非搭載側37個(idx34-70)
// （config.hのRGB_MATRIX_SPLIT { 34, 37 }と対応。ボール搭載側を
// is_keyboard_left()=true側として扱う前提はKeyball+と同じ。keymap.c参照）。
led_config_t g_led_config = {
    {
        // キーマトリクス(10行 x 8列) → LEDインデックス（暫定: 対応なし）
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    },
    {
        // LEDインデックス → 物理座標(x,y)（暫定）
        {0,32}, {3,32}, {6,32}, {9,32}, {12,32}, {15,32}, {18,32}, {21,32}, {24,32}, {27,32},
        {30,32}, {33,32}, {36,32}, {39,32}, {42,32}, {45,32}, {48,32}, {51,32}, {54,32}, {57,32},
        {60,32}, {63,32}, {66,32}, {69,32}, {72,32}, {75,32}, {78,32}, {81,32}, {84,32}, {87,32},
        {90,32}, {93,32}, {96,32}, {100,32}, {124,32}, {126,32}, {129,32}, {132,32}, {135,32}, {137,32},
        {140,32}, {143,32}, {146,32}, {149,32}, {151,32}, {154,32}, {157,32}, {160,32}, {162,32}, {165,32},
        {168,32}, {171,32}, {174,32}, {176,32}, {179,32}, {182,32}, {185,32}, {187,32}, {190,32}, {193,32},
        {196,32}, {199,32}, {201,32}, {204,32}, {207,32}, {210,32}, {212,32}, {215,32}, {218,32}, {221,32},
        {224,32},
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
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
    },
};
#endif

void keyball_on_adjust_layout(keyball_adjust_t v) {
#ifdef RGBLIGHT_ENABLE
    // adjust RGBLIGHT's clipping and effect ranges
    uint8_t lednum_this = keyball.this_have_ball ? 34 : 37;
    uint8_t lednum_that = !keyball.that_enable ? 0 : keyball.that_have_ball ? 34 : 37;
    rgblight_set_clipping_range(is_keyboard_left() ? 0 : lednum_that, lednum_this);
    rgblight_set_effect_range(0, lednum_this + lednum_that);
#endif
}
