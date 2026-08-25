# Keyball RP2040 Firmware

SparkFun Pro Micro RP2040を使用した、Keyballシリーズ向けの大容量フラッシュ版ファームウェアです。既存の[keyball-link-firmware](https://github.com/Yowkees/keyball-link-firmware)（ATmega32u4版、フラッシュ32KB）で容量制約により分けていた「LED版」「通常版（マクロ・ジェスチャー）」を、RP2040の大容量フラッシュ（16MB）により1つに統合し、全機能を同時有効化しています。

現在対応しているのはKeyball39のみです。基板は無改修、既存の12ピンPro Microソケット（コンスルー）にそのままSparkFun Pro RP2040を挿す前提です。

keyball-link-firmware（ATmega32u4版）・keyball-plus-firmware（Keyball+用）とは**別の独立したプロジェクト**として管理しています。

## 構成

| パス | 内容 |
|---|---|
| `keyboards/keyball/keyball39/` | Keyball39本体（`keymaps/web_configurator` が本ファームのキーマップ） |
| `keyboards/keyball/lib/` | Keyballシリーズ共通ライブラリ（keyball-link-firmwareと同じ内容をベースに、RP2040固有の対応を追加） |
| `keyboards/keyball/drivers/pmw3360/` | トラックボールセンサードライバ（RP2040/ChibiOS対応のSPI分周修正済み） |
| `patches/0001-usb-descriptor-jis-country-code.patch` | QMK本体修正: USBディスクリプタの国コードをJIS(15)に設定（macOSでJIS配列と認識させる） |
| `patches/0002-auto-mouse-runtime-threshold.patch` | QMK本体修正: 自動マウスレイヤーのしきい値を実行時に変更可能にする |

## ビルド方法

ベースにした QMK Firmware のコミット: `594558ec7b9ac1963870447778426682065e0d20`

```bash
# 1. QMK Firmware を取得してベースコミットに合わせる
git clone https://github.com/qmk/qmk_firmware.git
cd qmk_firmware
git checkout 594558ec7b9ac1963870447778426682065e0d20
make git-submodule

# 2. QMK本体へのパッチを適用
git apply /path/to/keyball-rp2040-firmware/patches/*.patch

# 3. keyball一式を配置
cp -R /path/to/keyball-rp2040-firmware/keyboards/keyball keyboards/

# 4. ビルド（.uf2 が生成される）
qmk compile -kb keyball/keyball39 -km web_configurator
```

## RP2040への書き込み（BOOTSEL + UF2）

1. SparkFun Pro RP2040の背面にあるBOOTSELボタンを押しながらUSB接続する
2. 「RPI-RP2」という名前のUSBメモリとして認識される
3. `qmk_firmware/keyball_keyball39_web_configurator.uf2` をそのドライブにドラッグ＆ドロップでコピーする
4. 自動的に再起動してファームウェアが反映される

Keyball LinkからのWeb書き込み対応（AVR版と同様にブラウザから直接書き込めるようにする機能）は未対応です。今後の別タスクとして検討中。

## AVR版との違い（今回のポート作業で必要だった変更点）

- USB PIDを新規発行（`0x0600`）。AVR版keyball39（`0x0200`）と衝突しないようにするため。それに伴い `lib/keyball/keyball.h` の `KEYBALL_MODEL` 判定にRP2040版のケースを追加。
- `keyboard.json` に `"pin_compatible": "promicro"` を追加。QMKの `CONVERT_TO=sparkfun_pm2040` コンバーターが認識するために必須。
- `keyboards/keyball/keyball39/rules.mk` に `CONVERT_TO = sparkfun_pm2040` を追加。
- トラックボールドライバ（`drivers/pmw3360/pmw3360.c`）のSPI速度計算がAVR専用の `F_CPU` マクロに依存していたため、QMK公式の`pmw33xx_common`ドライバと同じ書き方（`__AVR__`で分岐、非AVR環境は固定値64）に修正。
- `web_configurator` キーマップの「LED版/通常版」分岐を廃止し、RGBLIGHT・マクロ・ジェスチャーを常時同時有効化。
- ファームウェアバージョン（`kb_version.h`）はAVR版と別系統として `0.1.0` から開始。
