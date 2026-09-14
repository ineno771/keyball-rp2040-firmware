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
// LED物理配置（暫定・未校正）。
//
// Keyball+はKeyball39と同じ物理キー配置（マトリクス8行×6列、行0-3=トラックボール側・
// 行4-7=非搭載側、行3/7の親指キー構成も同一）だが、LED数が異なる
// （Keyball39=46個(22+24) に対し Keyball+=55個(26+29)、トラックボール搭載側26個・
// 非搭載側29個。数値はAVR版keyball-plus-firmwareのconfig.hから流用した実機確認済みの値）。
//
// 以下のマトリクス→LEDインデックス対応、およびLEDインデックス→物理座標(x,y)は、
// Keyball39の実機で行った「1キーずつ押してどのLEDが光るか確認する」校正作業
// （keyball39.cのコメント参照）と同様の検証をKeyball+ではまだ行っていないため、
// 配線順・座標とも暫定値（キー位置の並び通りに機械的に番号を振っただけ）。
// ビルド・起動・基本機能（マトリクス・トラックボール・OLED・単色/固定エフェクト等）は
// この値のままで動作するが、押した位置に反応する演出（リアクティブ・リップル・
// タイピングヒートマップ等）は実際のLEDと対応しない可能性が高い。
// 実機到着後、Keyball LinkのSettingsタブ「LED位置実測（開発用）」機能で1個ずつ
// 点灯確認しながらこの配列を書き直すこと。
led_config_t g_led_config = {
    {
        // キーマトリクス(8行 x 6列) → LEDインデックス
        { 10, 14, 17, 20, 23, NO_LED },
        { 11, 15, 18, 21, 24, NO_LED },
        { 12, 16, 19, 22, 25, NO_LED },
        { 13, NO_LED, NO_LED, NO_LED, 0, 1 },
        { 40, 36, 32, 29, 26, NO_LED },
        { 41, 37, 33, 30, 27, NO_LED },
        { 42, 38, 34, 31, 28, NO_LED },
        { 43, 39, 35, 44, 44, 44 },
    },
    {
        // LEDインデックス(0-54) → 物理座標(x,y)。暫定値（機械的なグリッド配置）。
        {10,60}, {30,60}, {50,60}, {70,60}, {90,60}, {110,60}, {130,60}, {20,64}, {60,64}, {100,64},
        {0,0}, {0,18}, {0,36}, {15,54}, {35,0}, {35,18}, {35,36}, {70,0}, {70,18}, {70,36},
        {105,0}, {105,18}, {105,36}, {140,0}, {140,18}, {140,36},
        {140,0}, {140,18}, {140,36}, {105,0}, {105,18}, {105,36}, {70,0}, {70,18}, {70,36}, {70,54},
        {35,0}, {35,18}, {35,36}, {35,54}, {0,0}, {0,18}, {0,36}, {0,54},
        {10,60}, {30,60}, {50,60}, {70,60}, {90,60}, {110,60}, {130,60}, {20,64}, {50,64}, {80,64}, {110,64},
    },
    {
        // LEDインデックス → フラグ。トラックボール側: アンダーグロー10個(idx0-9)+
        // キー連動16個(idx10-25)=26個。非搭載側: キー連動18個(idx26-43)+
        // アンダーグロー11個(idx44-54)=29個。
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT, LED_FLAG_KEYLIGHT,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW, LED_FLAG_UNDERGLOW,
        LED_FLAG_UNDERGLOW,
    },
};
#endif

void keyball_on_adjust_layout(keyball_adjust_t v) {
#ifdef RGBLIGHT_ENABLE
    // adjust RGBLIGHT's clipping and effect ranges
    // Keyball+はLED数が左右非対称（トラックボール側26個・非搭載側29個）のため、
    // Keyball39用の値をそのまま使うと実LED数とズレて起動直後にLEDの点灯範囲外が生じる
    uint8_t lednum_this = keyball.this_have_ball ? 26 : 29;
    uint8_t lednum_that = !keyball.that_enable ? 0 : keyball.that_have_ball ? 26 : 29;
    rgblight_set_clipping_range(is_keyboard_left() ? 0 : lednum_that, lednum_this);
    rgblight_set_effect_range(0, lednum_this + lednum_that);
#endif
}
