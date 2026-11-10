#!/usr/bin/env python3
"""Извлекает PNG-кадры из App/res/app.ico в assets/textures/app_icon/.

Кадры внутри .ico уже хранятся в формате PNG, поэтому конвертация (и сторонние
библиотеки) не нужны. Запускайте при смене логотипа — после обновления app.ico:

    python3 scripts/linux-desktop/ico-to-png.py
"""

import pathlib
import struct

root = pathlib.Path(__file__).resolve().parents[2]
ico_path = root / "App/res/app.ico"
out_dir = root / "assets/textures/app_icon"

data = ico_path.read_bytes()
count = struct.unpack("<H", data[4:6])[0]

out_dir.mkdir(parents=True, exist_ok=True)
for i in range(count):
    entry = data[6 + 16 * i : 6 + 16 * (i + 1)]
    width = entry[0] or 256
    size, offset = struct.unpack("<II", entry[8:16])
    frame = data[offset : offset + size]
    if not frame.startswith(b"\x89PNG"):
        raise SystemExit(f"Кадр {width}x{width} не является PNG — нужна конвертация")
    out_path = out_dir / f"app_icon_{width}.png"
    out_path.write_bytes(frame)
    print(out_path.relative_to(root))
