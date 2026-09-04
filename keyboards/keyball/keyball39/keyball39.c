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
// v2ではLAYOUT_left_ball/right_ball相当の実キー構造に沿って番号を再割当て。
// v3（本人確認済み）: ボール側は親指キーが2つ（無ボール側は3つ）で、内側(col0側)の
// 親指キーが存在しない。col0の親指位置(row3,col0)は専用LEDを持たず、代わりに
// 近くにあるアンダーグローLED（ラベル1,2＝idx0,1）を起点にする。
// v4: via.jsonをKLE本家(keyboard-layout-editor.com)に読み込んで本人が確認・修正した
// 配置データに基づき全面再構成。これにより判明した点：
//   - 無ボール側は親指位置が6箇所(col0-5)すべて実在する（3箇所と誤認していた）。
//     col1,2,3の親指はLEDが無いため、近くのアンダーグロー(idx45)を起点として流用。
//   - 座標はvia.jsonの列ずれ・親指キーの傾き(rx,ry,r)を含む本物のKLE座標を使用。
//     rx/ryは指定が無ければ直前の値を引き継ぐ(スティッキー)仕様だが、この点を誤って
//     実装しており親指ファン部分の座標が大きくズレていたのを2回に分けて修正した。
// col3の中段(row1,row2、ボール側のみ)は依然として実際の対応が未確認（トラックボール
// 周辺でLEDが足りていない箇所の推測）。当面はcol2の同じ行のLEDを流用している。
//
// matrix_coの行0-3=ボール側・行4-7=無ボール側という割り当ても未検証の仮定。
// 波紋を実際に押してみて、押した手と逆側のLEDが光るようであれば行0-3と4-7を
// 入れ替えること（もしくはis_keyboard_left()寄りの判定に修正すること）。
led_config_t g_led_config = {
    {
        // キーマトリクス(8行 x 6列) → LEDインデックス
        { 6, 10, 13, 16, 17, NO_LED },
        { 7, 11, 14, 14, 18, NO_LED },
        { 8, 12, 15, 15, 19, NO_LED },
        { 0, NO_LED, NO_LED, NO_LED, 20, 21 },
        { 36, 32, 28, 25, 22, NO_LED },
        { 37, 33, 29, 26, 23, NO_LED },
        { 38, 34, 30, 27, 24, NO_LED },
        { 39, 45, 45, 45, 35, 31 },
    },
    {
        // LEDインデックス(0-45) → 物理座標(x:0-224, y:0-64)。
        // via.json（VIA用KLEレイアウト定義）をLED Calibrator上で本人がKLE本家で
        // 確認・修正した配置データから算出。回転キー(親指ファン部分)のrx/ry解釈の
        // 誤りを2回修正済み（rx/ryは指定されなければ直前の値を引き継ぐスティッキー
        // な原点であり、通常の行送りとは独立している）。
        {0,55}, {0,55}, {70,4}, {0,9}, {0,40}, {70,35}, {0,9}, {0,25}, {0,40}, {0,55},
        {18,4}, {18,19}, {18,35}, {35,0}, {35,15}, {35,31}, {53,2}, {70,4}, {70,19}, {70,35},
        {70,50}, {84,50}, {154,4}, {154,19}, {154,35}, {171,2}, {171,17}, {171,33}, {189,0}, {189,15},
        {189,31}, {137,64}, {206,4}, {206,19}, {206,35}, {153,57}, {224,9}, {224,25}, {224,40}, {224,55},
        {224,9}, {154,4}, {224,40}, {154,35}, {224,55}, {189,46},
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
