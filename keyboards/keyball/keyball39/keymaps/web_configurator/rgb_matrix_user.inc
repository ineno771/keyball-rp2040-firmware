// LED_TEST: 指定インデックスのLEDのみ点灯（LED診断用）
// hue フィールドにLEDインデックスを格納。rgb_matrix_config は左右同期済みなので両側で動作する
RGB_MATRIX_EFFECT(LED_TEST)

// REACTIVE_KEYS: 押したキーのLEDだけが光る（隣接LEDへの波及なし）。
// アンダーグロー(LED_FLAG_UNDERGLOW)は常時、設定色でそのまま点灯し続ける。
// g_last_hit_tracker（両ハーフでそれぞれローカルに更新される）を使うため左右両側で
// 独立して動作する。旧SOLID_RIPPLE（波紋演出）から仕様変更（2026-09-04）。
// LEDごとに直接フェード状態を持つ設計にしたことで、旧実装にあった以下の問題も
// 併せて解消している：8個までしか同時に覚えられない・バッファシフトで誤検出する、
// といった制約や不具合が構造的に起きなくなった（LEDの数だけ独立した状態を持つため）。
RGB_MATRIX_EFFECT(REACTIVE_KEYS)

#ifdef RGB_MATRIX_CUSTOM_EFFECT_IMPLS

static bool LED_TEST(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t idx = rgb_matrix_config.hsv.h;  // hue = テスト対象LEDインデックス（両側で同期済み）
    for (uint8_t i = led_min; i < led_max; i++) {
        if (i == idx) {
            rgb_matrix_set_color(i, 255, 100, 0);
        } else {
            rgb_matrix_set_color(i, 0, 0, 0);
        }
    }
    return rgb_matrix_check_finished_leds(led_max);
}

static bool REACTIVE_KEYS(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // LEDインデックスごとに直近のヒット時刻を保持する（波紋と違い近隣への伝播が
    // ないので、LEDの数だけ独立したスロットを持てば十分。取りこぼしや誤検出の
    // 心配がない）。
    static uint16_t hit_start[RGB_MATRIX_LED_COUNT];
    static bool     hit_active[RGB_MATRIX_LED_COUNT];

    // 新規ヒットを反映
    uint8_t cnt = g_last_hit_tracker.count;
    if (cnt > LED_HITS_TO_REMEMBER) cnt = LED_HITS_TO_REMEMBER;
    for (uint8_t j = 0; j < cnt; j++) {
        if (g_last_hit_tracker.tick[j] > 30) continue;  // 十分新しいヒットのみ対象
        uint8_t idx = g_last_hit_tracker.index[j];
        if (idx < RGB_MATRIX_LED_COUNT) {
            hit_start[idx]  = timer_read();
            hit_active[idx] = true;
        }
    }

    const uint16_t FADE_MS = 250;  // 押してから消えるまでのフェード時間

    for (uint8_t i = led_min; i < led_max; i++) {
        HSV hsv = rgb_matrix_config.hsv;
        if (g_led_config.flags[i] & LED_FLAG_UNDERGLOW) {
            // アンダーグローは常時点灯（設定色そのまま）。
        } else {
            // キー連動LED: 押されていなければ消灯、押された直後はフェードアウト
            hsv.v = 0;
            if (hit_active[i]) {
                uint16_t elapsed = timer_elapsed(hit_start[i]);
                if (elapsed >= FADE_MS) {
                    hit_active[i] = false;
                } else {
                    hsv.v = 255 - (uint8_t)(((uint32_t)elapsed * 255) / FADE_MS);
                }
            }
        }
        RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
