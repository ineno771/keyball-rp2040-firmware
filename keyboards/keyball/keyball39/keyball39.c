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
// LED物理配置（2026-09-04、v2: LAYOUT macroの実キー構造とビルドガイドの配線順番号を
// 突き合わせて再構成）。
//
// 発光順（実測済み）: ボール側(22個)のラベル1→22が先、続けて無ボール側(24個)の
// ラベル1→24。ボール側はアンダーグロー=ラベル1-6・キー連動=ラベル7-22（16個）、
// 無ボール側はキー連動=ラベル1-18（18個）・アンダーグロー=ラベル19-24。
//
// v1（列×行の総当たりグリッド）は写真の列区切りを目分量で読んだだけで実際の
// キー配置と対応しておらず、波紋が「バラバラ」に見える原因になっていた。
// v2ではLAYOUT_left_ball/right_ball相当の実キー構造（col0:4キー(親指含む)、
// col1-3:3キーずつ、col4:3キー+親指、col5:親指のみ）に沿って番号を再割当てして
// いる。ただし列3-5付近（写真の1-6・10・14番あたり）の正確な列区分・親指キーの
// 判別は写真からの推定であり確証はない。実際に押して不自然な箇所があれば、
// その列（col3-5）のmatrix_co割り当てを優先的に見直すこと。
//
// matrix_coの行0-3=ボール側・行4-7=無ボール側という割り当ても未検証の仮定。
// 波紋を実際に押してみて、押した手と逆側のLEDが光るようであれば行0-3と4-7を
// 入れ替えること（もしくはis_keyboard_left()寄りの判定に修正すること）。
led_config_t g_led_config = {
    {
        // キーマトリクス(8行 x 6列) → LEDインデックス
        { 6, 10, 13, 16, 17, NO_LED },
        { 7, 11, 14, NO_LED, 18, NO_LED },
        { 8, 12, 15, NO_LED, 19, NO_LED },
        { 9, NO_LED, NO_LED, NO_LED, 20, 21 },
        { 36, 32, 28, 25, 22, NO_LED },
        { 37, 33, 29, 26, 23, NO_LED },
        { 38, 34, 30, 27, 24, NO_LED },
        { 39, NO_LED, NO_LED, NO_LED, 35, 31 },
    },
    {
        // LEDインデックス(0-45) → 物理座標(x:0-224, y:0-64)
        {10,4}, {10,60}, {50,62}, {96,58}, {100,30}, {96,6}, {16,8}, {16,24}, {16,40}, {16,56},
        {36,8}, {36,24}, {36,40}, {56,8}, {56,24}, {56,40}, {76,8}, {96,8}, {96,24}, {96,40},
        {96,56}, {116,56}, {216,8}, {216,24}, {216,40}, {196,8}, {196,24}, {196,40}, {176,8}, {176,24},
        {176,40}, {236,56}, {156,8}, {156,24}, {156,40}, {216,56}, {136,8}, {136,24}, {136,40}, {136,56},
        {246,4}, {246,60}, {196,62}, {140,58}, {130,30}, {140,6},
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
