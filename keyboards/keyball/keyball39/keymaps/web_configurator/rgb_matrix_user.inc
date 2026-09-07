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

// 以下4つはRGBLIGHT版に存在したがRGB_MATRIXに組み込みが無いエフェクトの再現。
// いずれも片側ハーフのLED範囲(led_min〜led_max)だけを使ってローカルに完結させて
// いるため、RGBLIGHT版で問題になっていたスプリット両ハーフ間の同期は考慮不要。
RGB_MATRIX_EFFECT(SNAKE)
RGB_MATRIX_EFFECT(KNIGHT)
RGB_MATRIX_EFFECT(CHRISTMAS)
RGB_MATRIX_EFFECT(ALTERNATING)

// ハロウィン・イースター（季節限定）。RGBLIGHT版は単色クロスフェードだったが、
// 本人希望によりクリスマスと同じ「市松模様に交互点灯」する動きに揃えている。
RGB_MATRIX_EFFECT(HALLOWEEN)
RGB_MATRIX_EFFECT(EASTER)

// TWINKLE: 基本は設定色で常時点灯し、ランダムなLEDが時々明るく煌めいてから元に戻る
// （星の瞬きのような演出）。組み込みのRAINDROPS/STARLIGHTは常に色相・明るさが
// 揺れ動いてしまい「基本は常時点灯」という要望に合わなかったため自作した。
RGB_MATRIX_EFFECT(TWINKLE)

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

// SNAKE: 明るい先頭(head)がLED配列上を巡回し、後ろに向かって徐々に暗くなる
// 尻尾を引く（RGBLIGHT_MODE_SNAKEの再現）。片側ハーフのLED範囲内で完結する。
static bool SNAKE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t count = led_max - led_min;
    if (count == 0) return rgb_matrix_check_finished_leds(led_max);

    // speedが大きいほど短い間隔でheadが1つ進む（50〜400ms）
    uint16_t interval = 400 - ((uint16_t)rgb_matrix_config.speed * 350 / 255);
    static uint16_t last_tick = 0;
    static uint8_t  head      = 0;
    if (timer_elapsed(last_tick) >= interval) {
        last_tick = timer_read();
        head      = (head + 1) % count;
    }

    // 尻尾のフェードは付けず、配線順(インデックス)で1個のLEDだけが点灯して進む
    // （複数箇所が同時に光ってバラバラに見えるという指摘を受けて変更）。
    HSV hsv = rgb_matrix_config.hsv;
    for (uint8_t i = led_min; i < led_max; i++) {
        uint8_t pos = i - led_min;
        HSV     out = hsv;
        if (pos != head) out.v = 0;
        RGB rgb = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// KNIGHT: 明るい点がLED配列の両端で折り返しながら往復する（ナイトライダー/Cylon
// スキャナー、RGBLIGHT_MODE_KNIGHTの再現）。
static bool KNIGHT(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t count = led_max - led_min;
    if (count == 0) return rgb_matrix_check_finished_leds(led_max);

    uint16_t interval = 300 - ((uint16_t)rgb_matrix_config.speed * 250 / 255);  // 50〜300ms
    static uint16_t last_tick = 0;
    static int8_t   pos       = 0;
    static int8_t   dir       = 1;
    if (timer_elapsed(last_tick) >= interval) {
        last_tick = timer_read();
        pos += dir;
        if (pos >= (int8_t)count - 1) { pos = count - 1; dir = -1; }
        if (pos <= 0) { pos = 0; dir = 1; }
    }

    const uint8_t TAIL_LEN = 4;
    HSV           hsv      = rgb_matrix_config.hsv;
    for (uint8_t i = led_min; i < led_max; i++) {
        int16_t dist = pos - (int16_t)(i - led_min);
        if (dist < 0) dist = -dist;
        HSV out = hsv;
        out.v   = (dist < TAIL_LEN) ? (uint8_t)((uint16_t)hsv.v * (TAIL_LEN - dist) / TAIL_LEN) : 0;
        RGB rgb = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// CHRISTMAS: 赤と緑を交互に配置し、一定間隔で入れ替える（RGBLIGHT_MODE_CHRISTMASの再現）。
// 複数色を市松模様に配置し、一定間隔でずらしていく（CHRISTMAS・HALLOWEEN・EASTERの
// 共通実装）。色数2なら「0→1→0→1…」と交互点灯、3なら3色が順にずれていく。
static void render_checkerboard(effect_params_t* params, const uint8_t* hues, uint8_t count, uint16_t* last_tick, uint8_t* phase) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    uint16_t interval = 1500 - ((uint16_t)rgb_matrix_config.speed * 1400 / 255);  // 100〜1500ms
    if (timer_elapsed(*last_tick) >= interval) {
        *last_tick = timer_read();
        *phase     = (uint8_t)((*phase + 1) % count);
    }

    for (uint8_t i = led_min; i < led_max; i++) {
        uint8_t slot = (uint8_t)(((i - led_min) + *phase) % count);
        HSV     out  = {hues[slot], rgb_matrix_config.hsv.s, rgb_matrix_config.hsv.v};
        RGB     rgb  = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
}

static bool CHRISTMAS(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    static const uint8_t hues[] = {0, 85};  // 赤・緑
    static uint16_t      last_tick = 0;
    static uint8_t       phase     = 0;
    render_checkerboard(params, hues, 2, &last_tick, &phase);
    return rgb_matrix_check_finished_leds(led_max);
}

// HALLOWEEN: オレンジ×紫の市松模様（色相はkeyball.cのRGBLIGHT版seasonal_palette_forと
// 揃えてある）。
static bool HALLOWEEN(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    static const uint8_t hues[] = {21, 191};  // オレンジ・紫
    static uint16_t      last_tick = 0;
    static uint8_t       phase     = 0;
    render_checkerboard(params, hues, 2, &last_tick, &phase);
    return rgb_matrix_check_finished_leds(led_max);
}

// EASTER: パステルピンク・グリーン・イエローの3色が順にずれていく市松模様。
static bool EASTER(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    static const uint8_t hues[] = {228, 90, 42};  // パステルピンク・グリーン・イエロー
    static uint16_t      last_tick = 0;
    static uint8_t       phase     = 0;
    render_checkerboard(params, hues, 3, &last_tick, &phase);
    return rgb_matrix_check_finished_leds(led_max);
}

static bool TWINKLE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // LEDごとに「今瞬いている最中か」「いつ始まったか」を持つ（REACTIVE_KEYSと同じ
    // 発想で、LEDの数だけ独立したスロットを持つため取りこぼしや誤検出の心配がない）。
    static uint16_t twinkle_start[RGB_MATRIX_LED_COUNT];
    static bool     twinkle_active[RGB_MATRIX_LED_COUNT];
    static uint16_t spawn_last_tick = 0;

    // 瞬きの発生頻度: speedが大きいほど短い間隔で新しい瞬きが生まれる
    // 最速でもそこまで慌ただしくならないよう、全体的に遅めのレンジにしてある
    uint16_t spawn_interval = 3000 - ((uint16_t)rgb_matrix_config.speed * 2200 / 255);  // 800〜3000ms
    uint8_t  count          = led_max - led_min;
    if (count > 0 && timer_elapsed(spawn_last_tick) >= spawn_interval) {
        spawn_last_tick     = timer_read();
        uint8_t idx         = led_min + random8_max(count);
        twinkle_start[idx]  = timer_read();
        twinkle_active[idx] = true;
    }

    const uint16_t TWINKLE_MS = 1500;  // 瞬き1回分の長さ（明るくなって元に戻るまで）
    for (uint8_t i = led_min; i < led_max; i++) {
        HSV hsv = rgb_matrix_config.hsv;  // 基本は常時点灯（設定色そのまま）
        if (twinkle_active[i]) {
            uint16_t elapsed = timer_elapsed(twinkle_start[i]);
            if (elapsed >= TWINKLE_MS) {
                twinkle_active[i] = false;
            } else {
                // 0→山→0で明るさを一瞬持ち上げつつ彩度を落とし、白っぽく煌めかせる
                uint8_t  t     = (uint8_t)(((uint32_t)elapsed * 255) / TWINKLE_MS);
                uint8_t  boost = t < 128 ? (uint8_t)(t * 2) : (uint8_t)((255 - t) * 2);
                hsv.s          = (uint8_t)((uint16_t)hsv.s * (255 - boost) / 255);
                uint16_t v     = (uint16_t)hsv.v + ((uint16_t)(255 - hsv.v) * boost / 255);
                hsv.v          = v > 255 ? 255 : (uint8_t)v;
            }
        }
        RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

// ALTERNATING: LED範囲を前半・後半の2グループに分け、明暗を一定間隔で入れ替える
// （RGBLIGHT_MODE_ALTERNATINGの再現）。片側ハーフ内で完結させるため、RGBLIGHT版で
// 課題になっていたスプリット両ハーフ間の同期方式は考慮不要。
static bool ALTERNATING(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);
    uint8_t count = led_max - led_min;
    if (count == 0) return rgb_matrix_check_finished_leds(led_max);

    uint16_t interval = 1500 - ((uint16_t)rgb_matrix_config.speed * 1400 / 255);
    static uint16_t last_tick = 0;
    static bool     phase     = false;
    if (timer_elapsed(last_tick) >= interval) {
        last_tick = timer_read();
        phase     = !phase;
    }

    uint8_t half = count / 2;
    HSV     hsv  = rgb_matrix_config.hsv;
    for (uint8_t i = led_min; i < led_max; i++) {
        bool first_group = (i - led_min) < half;
        HSV  out         = hsv;
        out.v            = (first_group == phase) ? hsv.v : 0;
        RGB rgb          = rgb_matrix_hsv_to_rgb(out);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
