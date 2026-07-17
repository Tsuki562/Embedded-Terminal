from __future__ import annotations

import json
import math
from pathlib import Path

from PIL import Image, ImageDraw, ImageFont


BASE_DIR = Path(__file__).resolve().parent
SRC = BASE_DIR / "original.jpg"
FINAL_OUT = BASE_DIR / "annotated_final.png"
OVERLAY_OUT = BASE_DIR / "annotation_overlay.png"
DATA_OUT = BASE_DIR / "annotation_labels.json"

FONT_PATH = Path(r"C:\Windows\Fonts\msyh.ttc")
FONT = ImageFont.truetype(str(FONT_PATH), 34)
FONT_SMALL = ImageFont.truetype(str(FONT_PATH), 30)

COLORS = {
    "camera": (255, 204, 0, 255),
    "board": (80, 255, 130, 255),
    "battery": (80, 160, 255, 255),
    "wheel": (255, 120, 80, 255),
    "motor": (210, 120, 255, 255),
}

LABELS = [
    {
        "id": "camera",
        "text": "摄像头安装位置",
        "box": [365, 115],
        "targets": [[565, 210]],
        "font": "large",
    },
    {
        "id": "board",
        "text": "主控板",
        "box": [1220, 410],
        "targets": [[1060, 600]],
        "font": "small",
    },
    {
        "id": "battery",
        "text": "电池位置",
        "box": [1235, 765],
        "targets": [[1345, 805]],
        "font": "small",
    },
    {
        "id": "motor",
        "text": "电机",
        "box": [720, 825],
        "targets": [[810, 905], [1130, 915]],
        "font": "small",
    },
    {
        "id": "wheel",
        "text": "车轮",
        "box": [1180, 1080],
        "targets": [[1240, 1015]],
        "font": "small",
    },
]

WHEEL_OUTLINES = [
    [505, 890, 842, 1205],
    [1080, 865, 1405, 1175],
]


def draw_arrow(draw: ImageDraw.ImageDraw, start, end, color, width=6) -> None:
    draw.line([start, end], fill=color, width=width)
    ang = math.atan2(end[1] - start[1], end[0] - start[0])
    head_len = 24
    head_ang = math.radians(28)
    p1 = (
        end[0] - head_len * math.cos(ang - head_ang),
        end[1] - head_len * math.sin(ang - head_ang),
    )
    p2 = (
        end[0] - head_len * math.cos(ang + head_ang),
        end[1] - head_len * math.sin(ang + head_ang),
    )
    draw.polygon([end, p1, p2], fill=color)
    r = 7
    draw.ellipse([end[0] - r, end[1] - r, end[0] + r, end[1] + r], fill=color)


def anchor_from_rect(rect, target):
    x1, y1, x2, y2 = rect
    cx, cy = (x1 + x2) / 2, (y1 + y2) / 2
    tx, ty = target
    dx, dy = tx - cx, ty - cy
    candidates = []

    if dx != 0:
        for x in (x1, x2):
            t = (x - cx) / dx
            y = cy + t * dy
            if t > 0 and y1 <= y <= y2:
                candidates.append((t, x, y))

    if dy != 0:
        for y in (y1, y2):
            t = (y - cy) / dy
            x = cx + t * dx
            if t > 0 and x1 <= x <= x2:
                candidates.append((t, x, y))

    if candidates:
        _, x, y = min(candidates, key=lambda item: item[0])
        return x, y
    return cx, cy


def label_box(draw: ImageDraw.ImageDraw, item) -> list[int]:
    text = item["text"]
    x, y = item["box"]
    color = COLORS[item["id"]]
    font = FONT if item["font"] == "large" else FONT_SMALL
    pad_x, pad_y = 18, 11

    bbox = draw.textbbox((0, 0), text, font=font)
    tw, th = bbox[2] - bbox[0], bbox[3] - bbox[1]
    rect = [x, y, x + tw + pad_x * 2, y + th + pad_y * 2]

    draw.rounded_rectangle(
        [rect[0] + 4, rect[1] + 4, rect[2] + 4, rect[3] + 4],
        radius=10,
        fill=(0, 0, 0, 160),
    )
    draw.rounded_rectangle(rect, radius=10, fill=(10, 16, 20, 225), outline=color, width=4)
    draw.text((x + pad_x, y + pad_y - 2), text, font=font, fill=(255, 255, 255, 255))

    for target in item["targets"]:
        draw_arrow(draw, anchor_from_rect(rect, target), target, color)

    return rect


def main() -> None:
    base = Image.open(SRC).convert("RGBA")
    final = base.copy()
    overlay = Image.new("RGBA", base.size, (0, 0, 0, 0))

    final_draw = ImageDraw.Draw(final)
    overlay_draw = ImageDraw.Draw(overlay)

    label_data = []
    for item in LABELS:
        final_rect = label_box(final_draw, item)
        overlay_rect = label_box(overlay_draw, item)
        label_data.append({**item, "rendered_box": final_rect, "overlay_box": overlay_rect})

    for box in WHEEL_OUTLINES:
        final_draw.ellipse(box, outline=COLORS["wheel"], width=5)
        overlay_draw.ellipse(box, outline=COLORS["wheel"], width=5)

    final.convert("RGB").save(FINAL_OUT, quality=95)
    overlay.save(OVERLAY_OUT)

    DATA_OUT.write_text(
        json.dumps(
            {
                "source": str(SRC.name),
                "final": str(FINAL_OUT.name),
                "overlay": str(OVERLAY_OUT.name),
                "image_size": base.size,
                "labels": label_data,
                "wheel_outlines": WHEEL_OUTLINES,
            },
            ensure_ascii=False,
            indent=2,
        ),
        encoding="utf-8",
    )


if __name__ == "__main__":
    main()
