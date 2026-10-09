#!/usr/bin/env python3
# OLEDアニメーション（frames/anim_*.h、1コマ512バイトの非圧縮データ）を圧縮して
# frames_packed/anim_*.h を生成する（2026-10-09、ファームウェア容量削減のため）。
#
# 使い方: このファイルのあるディレクトリで `python3 pack_anim_frames.py`
# （元データ frames/anim_*.h を差し替えた時は必ず実行し直すこと）。
#
# 圧縮形式（anim_frames.cのkb_anim_frame()が元に戻す）:
#   各コマ = 種別1バイト + PackBits形式のデータ
#     種別 0: そのコマ単体（キーフレーム）
#     種別 1: 直前のコマとのXOR差分
#   PackBits: 制御バイトc
#     c = 0〜127   → 続くc+1バイトをそのまま
#     c = 128〜255 → 次の1バイトを c-125 回（3〜130回）繰り返す
#   先頭と KEYFRAME_INTERVAL コマごとは必ずキーフレームにする（途中から再生する時に
#   元に戻す手間を最大でもこのコマ数分に抑えるため）。それ以外は小さい方を採用。
import glob
import os
import re

FRAME_BYTES = 512
KEYFRAME_INTERVAL = 30

HERE = os.path.dirname(os.path.abspath(__file__))
SRC_DIR = os.path.join(HERE, '..', 'frames')
OUT_DIR = os.path.join(HERE, '..', 'frames_packed')


def packbits(b):
    out = bytearray()
    lit = bytearray()

    def flush():
        nonlocal lit
        while lit:
            chunk = lit[:128]
            out.append(len(chunk) - 1)
            out.extend(chunk)
            lit = lit[128:]

    i, n = 0, len(b)
    while i < n:
        j = i
        while j < n and b[j] == b[i] and j - i < 130:
            j += 1
        if j - i >= 3:
            flush()
            out.append(j - i + 125)
            out.append(b[i])
        else:
            lit.extend(b[i:j])
        i = j
    flush()
    return bytes(out)


def unpack_into(p, buf, xor):
    i = pos = 0
    while i < len(p):
        c = p[i]
        i += 1
        if c < 128:
            seg = p[i:i + c + 1]
            i += c + 1
        else:
            seg = bytes([p[i]]) * (c - 125)
            i += 1
        for v in seg:
            buf[pos] = (buf[pos] ^ v) if xor else v
            pos += 1
    assert pos == FRAME_BYTES


def main():
    os.makedirs(OUT_DIR, exist_ok=True)
    total_raw = total_packed = 0
    for path in sorted(glob.glob(os.path.join(SRC_DIR, 'anim_*.h'))):
        name = os.path.splitext(os.path.basename(path))[0]
        text = open(path).read()
        data = bytes(int(x, 16) for x in re.findall(r'0x([0-9A-Fa-f]{2})', text))
        assert len(data) % FRAME_BYTES == 0, name
        frames = [data[k * FRAME_BYTES:(k + 1) * FRAME_BYTES] for k in range(len(data) // FRAME_BYTES)]

        stream = bytearray()
        offsets = []
        for k, fr in enumerate(frames):
            offsets.append(len(stream))
            key = packbits(fr)
            if k % KEYFRAME_INTERVAL == 0:
                stream += bytes([0]) + key
                continue
            delta = packbits(bytes(a ^ b for a, b in zip(fr, frames[k - 1])))
            stream += (bytes([1]) + delta) if len(delta) < len(key) else (bytes([0]) + key)
        offsets.append(len(stream))

        # 生成結果を元に戻して、元データと完全一致することを確認する
        buf = bytearray(FRAME_BYTES)
        for k, fr in enumerate(frames):
            seg = stream[offsets[k]:offsets[k + 1]]
            unpack_into(seg[1:], buf, seg[0] == 1)
            assert bytes(buf) == fr, f'{name} frame {k} mismatch'

        out = [
            f'// {os.path.basename(path)} を tools/pack_anim_frames.py で圧縮したもの（自動生成・手で編集しないこと）。',
            f'// {len(frames)}コマ、元 {len(data)} バイト → {len(stream)} バイト。',
            f'static const uint8_t {name}_data[{len(stream)}] = {{',
        ]
        for i in range(0, len(stream), 24):
            out.append('    ' + ', '.join(f'0x{v:02X}' for v in stream[i:i + 24]) + ',')
        out.append('};')
        out.append(f'static const uint32_t {name}_offsets[{len(offsets)}] = {{')
        for i in range(0, len(offsets), 12):
            out.append('    ' + ', '.join(str(v) for v in offsets[i:i + 12]) + ',')
        out.append('};')
        out.append(f'const kb_anim_t {name} = {{ {name}_data, {name}_offsets, {len(frames)} }};')
        open(os.path.join(OUT_DIR, f'{name}.h'), 'w').write('\n'.join(out) + '\n')

        total_raw += len(data)
        total_packed += len(stream) + len(offsets) * 4
        print(f'{name:16s} {len(frames):4d}コマ {len(data):7d} → {len(stream):7d} バイト')
    print(f'合計 {total_raw} → {total_packed} バイト（位置表を含む）')


if __name__ == '__main__':
    main()
