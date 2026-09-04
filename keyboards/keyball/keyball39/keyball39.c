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
    0b00011111,
    0b00011111,
    0b00011111,
    0b00111111,
    0b00011111,
    0b00011111,
    0b00011111,
    0b00111111,
};
// clang-format on

#ifdef RGB_MATRIX_ENABLE
// LED物理配置（2026-09-04、実機のLED_TEST診断＋ビルドガイドの配線順番号をもとに概算）。
//
// 実測で判明した発光順: ボール側(22個)のラベル1→22が先に光り、続けて無ボール側(24個)の
// ラベル1→24が光る。ボール側にはアンダーグロー(キーに紐付かない)ラベル1-6、キー連動
// ラベル7-22があり、無ボール側はキー連動ラベル1-18、アンダーグローラベル19-24。
// これをファームウェアのLEDインデックス(0-45)に変換すると:
//   idx  0- 5: ボール側アンダーグロー(ラベル1-6)
//   idx  6-21: ボール側キー連動(ラベル7-22、16個)
//   idx 22-39: 無ボール側キー連動(ラベル1-18、18個)
//   idx 40-45: 無ボール側アンダーグロー(ラベル19-24)
//
// pointの座標は「列×行の大まかなグリッド近似」であり、実際のキー配置を正確になぞった
// ものではない（写真から1つ1つのLEDの正確な物理位置を読み取るのは精度に限界があった
// ため）。波紋演出は多少のズレでも見た目上大きな違和感にはなりにくいという前提で採用。
// 見た目に問題があれば個別に補正すること。
//
// matrix_coの行0-3=ボール側・行4-7=無ボール側という割り当ては未検証の仮定。
// 波紋を実際に押してみて、押した手と逆側のLEDが光るようであれば行0-3と4-7を
// 入れ替えること（もしくはis_keyboard_left()寄りの判定に修正すること）。
led_config_t g_led_config = {
    {
        // キーマトリクス(8行 x 6列) → LEDインデックス
        {  6,  7,  8,  9, NO_LED, NO_LED },
        { 10, 11, 12, 13, NO_LED, NO_LED },
        { 14, 15, 16, 17, NO_LED, NO_LED },
        { 18, 19, 20, 21, NO_LED, NO_LED },
        { 22, 23, 24, 25, 26, 27 },
        { 28, 29, 30, 31, 32, 33 },
        { 34, 35, 36, 37, 38, 39 },
        { NO_LED, NO_LED, NO_LED, NO_LED, NO_LED, NO_LED },
    },
    {
        // LEDインデックス(0-45) → 物理座標(x:0-224, y:0-64)
        {10,4}, {10,60}, {50,62}, {96,58}, {100,30}, {96,6}, {20,8}, {44,8}, {68,8}, {92,8},
        {20,24}, {44,24}, {68,24}, {92,24}, {20,40}, {44,40}, {68,40}, {92,40}, {20,56}, {44,56},
        {68,56}, {92,56}, {125,10}, {142,10}, {159,10}, {176,10}, {193,10}, {210,10}, {125,26}, {142,26},
        {159,26}, {176,26}, {193,26}, {210,26}, {125,42}, {142,42}, {159,42}, {176,42}, {193,42}, {210,42},
        {216,4}, {216,60}, {176,62}, {130,58}, {126,30}, {130,6},
    },
    {
        // LEDインデックス → フラグ
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
    },
};
#endif

void keyball_on_adjust_layout(keyball_adjust_t v) {
#ifdef RGBLIGHT_ENABLE
    // adjust RGBLIGHT's clipping and effect ranges
    uint8_t lednum_this = keyball.this_have_ball ? 22 : 24;
    uint8_t lednum_that = !keyball.that_enable ? 0 : keyball.that_have_ball ? 22 : 24;
    rgblight_set_clipping_range(is_keyboard_left() ? 0 : lednum_that, lednum_this);
    rgblight_set_effect_range(0, lednum_this + lednum_that);
#endif
}
