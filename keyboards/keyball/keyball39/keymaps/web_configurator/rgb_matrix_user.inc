// LED_TEST: 指定インデックスのLEDのみ点灯（LED診断用）
// hue フィールドにLEDインデックスを格納。rgb_matrix_config は左右同期済みなので両側で動作する
RGB_MATRIX_EFFECT(LED_TEST)

// SOLID_RIPPLE: 押したキーから波が広がるリアクティブエフェクト
// g_last_hit_tracker（QMKがスレーブに同期済み）を使うため左右両側で動作する
// key-up でも g_last_hit_tracker が発火するため、同座標300ms以内の2重起動を抑制する
RGB_MATRIX_EFFECT(SOLID_RIPPLE)

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

static bool SOLID_RIPPLE(effect_params_t* params) {
    RGB_MATRIX_USE_LIMITS(led_min, led_max);

    // ヒット検出用 static state（各サイドが独立して保持）
    static uint8_t  rip_x[RIPPLE_MAX_HITS];
    static uint8_t  rip_y[RIPPLE_MAX_HITS];
    static uint8_t  rip_index[RIPPLE_MAX_HITS];  // 起点キーのLEDインデックス（左右境界判定に使用）
    static uint16_t rip_start[RIPPLE_MAX_HITS];
    static bool     rip_active[RIPPLE_MAX_HITS];
    static uint8_t  rip_next = 0;

    // g_last_hit_tracker から新規ヒットを検出する。
    // 【v2】以前は「前フレームのtickとの比較」で新規判定していたが、g_last_hit_tracker.count
    // が8件を超えるとQMK側がバッファをシフトして古いヒットを捨てる仕様のため、スロット番号
    // が同じヒットを指し続ける保証がなく、シフトのたびに誤って「新規ヒット」と誤検出して
    // いた（しばらく入力を続けると挙動がおかしくなる不具合の原因）。
    // v2ではスロットの前フレーム値と比較せず、単純に「tickが十分小さい（＝直近で登録された）
    // ヒットか」だけを見て、同座標での重複起動は既存の300ms以内ロジックで防ぐ。
    uint8_t cnt = g_last_hit_tracker.count;
    if (cnt > RIPPLE_MAX_HITS) cnt = RIPPLE_MAX_HITS;
    for (uint8_t j = 0; j < cnt; j++) {
        if (g_last_hit_tracker.tick[j] > 30) continue;  // 十分新しいヒットのみ対象
        uint8_t nx  = g_last_hit_tracker.x[j];
        uint8_t ny  = g_last_hit_tracker.y[j];
        uint8_t nix = g_last_hit_tracker.index[j];
        // 同じ座標から直近300ms以内にリップルが起動済みならスキップ（key-up・連続検出の重複除外）
        bool dup = false;
        for (uint8_t m = 0; m < RIPPLE_MAX_HITS; m++) {
            if (rip_active[m] && rip_x[m] == nx && rip_y[m] == ny &&
                timer_elapsed(rip_start[m]) < 300) {
                dup = true;
                break;
            }
        }
        if (!dup) {
            // 空きスロットを優先して使う。全て使用中なら最も古い（表示が進んだ）ものを
            // 上書きする。以前は空き状況を見ずに rip_next を単純にインクリメントして
            // いたため、表示中の若い波紋を上書きして「途中で終わる」ことがあった
            // （波紋を低速化して寿命が延びたことで、タイピング中に8個を超えやすくなり
            // 顕在化した）。
            uint8_t slot      = rip_next;
            bool    found_free = false;
            for (uint8_t m = 0; m < RIPPLE_MAX_HITS; m++) {
                if (!rip_active[m]) {
                    slot       = m;
                    found_free = true;
                    break;
                }
            }
            if (!found_free) {
                uint16_t oldest_elapsed = 0;
                for (uint8_t m = 0; m < RIPPLE_MAX_HITS; m++) {
                    uint16_t el = timer_elapsed(rip_start[m]);
                    if (el >= oldest_elapsed) {
                        oldest_elapsed = el;
                        slot           = m;
                    }
                }
            }
            rip_x[slot]      = nx;
            rip_y[slot]      = ny;
            rip_index[slot]  = nix;
            rip_start[slot]  = timer_read();
            rip_active[slot] = true;
            rip_next = (slot + 1) % RIPPLE_MAX_HITS;
        }
    }

    // リップルごとの現在 tick を事前計算し、期限切れを deactivate
    // max_dist は uint8_t(sqrt16) の上限 255 なので 255 + RIPPLE_TAIL_LEN で全LEDを通過
    uint16_t rip_tick_val[RIPPLE_MAX_HITS];
    for (uint8_t k = 0; k < RIPPLE_MAX_HITS; k++) {
        rip_tick_val[k] = 0;
        if (!rip_active[k]) continue;
        uint16_t elapsed = timer_elapsed(rip_start[k]);
        // speed設定に依存せず固定速度で計算。
        // 【暫定】確認しやすいよう通常の1/4速に落としている（elapsed/16）。
        // 本実装時は元の速さ（elapsed>>2、もしくはspeed設定に連動）に戻すか検討すること。
        uint16_t t = elapsed >> 4;
        if (t > (uint16_t)(255 + RIPPLE_TAIL_LEN)) {
            rip_active[k] = false;
        } else {
            rip_tick_val[k] = t;
        }
    }

    // 各 LED の輝度を計算（全LED対象、アンダーグロウ含む）
    for (uint8_t i = led_min; i < led_max; i++) {
        HSV hsv = rgb_matrix_config.hsv;
        hsv.v   = 0;
        for (uint8_t k = 0; k < RIPPLE_MAX_HITS; k++) {
            if (!rip_active[k]) continue;
            // 左右スプリット境界で波紋を分離。
            // 【v2】以前はx座標が112以上かどうかの目算値で判定しており、境界付近の
            // 座標を持つLED（アンダーグロー等）で誤判定が起き、反対側のハーフでも
            // 光ってしまう不具合があった。RGB_MATRIX_USE_LIMITSで取得したled_min/
            // led_max は「このハーフが実際に描画を担当するLEDインデックス範囲」を
            // QMKが左右で正しく分けて返す値なので、これを直接使えば座標に依存せず
            // 確実に左右を分離できる。
            if (rip_index[k] < led_min || rip_index[k] >= led_max) continue;
            int16_t dx   = (int16_t)g_led_config.point[i].x - (int16_t)rip_x[k];
            int16_t dy   = (int16_t)g_led_config.point[i].y - (int16_t)rip_y[k];
            uint8_t adx  = dx < 0 ? (uint8_t)(-dx) : (uint8_t)dx;
            uint8_t ady  = dy < 0 ? (uint8_t)(-dy) : (uint8_t)dy;
            // チェビシェフ距離: キーグリッド単位で隣キーが均等に同時点灯
            uint8_t  dist = adx > ady ? adx : ady;
            uint16_t tick = rip_tick_val[k];
            if (tick >= dist) {
                uint16_t tail = tick - dist;
                if (tail < RIPPLE_TAIL_LEN) {
                    uint8_t brightness = 255 - (uint8_t)(tail * 255 / RIPPLE_TAIL_LEN);
                    hsv.v = qadd8(hsv.v, brightness);
                }
            }
        }
        RGB rgb = rgb_matrix_hsv_to_rgb(hsv);
        rgb_matrix_set_color(i, rgb.r, rgb.g, rgb.b);
    }
    return rgb_matrix_check_finished_leds(led_max);
}

#endif // RGB_MATRIX_CUSTOM_EFFECT_IMPLS
