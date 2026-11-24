#!/usr/bin/env python3
"""Generates the weather condition icons (ui/icons/weather_*.cpp).

Same format as the other icons: 74x74, LV_COLOR_FORMAT_RGB565A8 (white, the
colour comes from the theme's image_recolor), line style with round caps.
weather_partly_cloudy_day.cpp (sun behind a cloud) is the "partly cloudy" day icon.

Every icon is scaled so its artwork's longer side is FILL px, with the line width
kept at STROKE after scaling, so all icons fill their footprint equally.

  python3 gen_weather_icons.py        (needs Pillow; run from this directory)
"""
import math
from PIL import Image, ImageChops, ImageDraw

SIZE = 74           # icon size in px
SS = 8              # supersampling
STROKE0 = 4.0       # final line width in px
FILL = 70           # final size of the artwork's longer side in px
STROKE = STROKE0    # line width used while drawing (compensates for the scaling)


def canvas():
    return Image.new("L", (SIZE * SS, SIZE * SS), 0)


def px(v):
    return v * SS


def disc(d, x, y, r):
    d.ellipse([px(x - r), px(y - r), px(x + r), px(y + r)], fill=255)


def stroke_path(points, width=None):
    """Round-capped polyline."""
    width = STROKE if width is None else width
    m = canvas()
    d = ImageDraw.Draw(m)
    for (x0, y0), (x1, y1) in zip(points, points[1:]):
        steps = max(2, int(math.hypot(x1 - x0, y1 - y0) * SS))
        for i in range(steps + 1):
            t = i / steps
            disc(d, x0 + (x1 - x0) * t, y0 + (y1 - y0) * t, width / 2)
    return m


def stroke_arc(cx, cy, r, a0, a1, width=None):
    pts = []
    n = max(8, int(abs(a1 - a0) * r))
    for i in range(n + 1):
        a = a0 + (a1 - a0) * i / n
        pts.append((cx + r * math.cos(a), cy + r * math.sin(a)))
    return stroke_path(pts, width)


def sub(a, b):
    return ImageChops.subtract(a, b)


def add(*masks):
    out = masks[0]
    for m in masks[1:]:
        out = ImageChops.lighter(out, m)
    return out


def cloud_fill(cx, cy, k, grow):
    """Filled cloud silhouette, centred on (cx, cy), scaled by k, grown by `grow` px."""
    m = canvas()
    d = ImageDraw.Draw(m)

    def c(x, y, r):
        disc(d, cx + x * k, cy + y * k, r * k + grow)

    c(-15, 11, 9.5)
    c(15, 11, 9.5)
    c(-8, -3, 11)
    c(7, -6, 14.5)
    c(0, 4, 11)   # fills the hole the shrunk shapes would leave in the middle
    d.rectangle([px(cx - 15 * k), px(cy + 1.5 * k - grow), px(cx + 15 * k), px(cy + 20.5 * k + grow)], fill=255)
    return m


def cloud(cx, cy, k):
    g = STROKE / 2
    return sub(cloud_fill(cx, cy, k, g), cloud_fill(cx, cy, k, -g))


def sun(cx, cy, r=11, ray_in=17.5, ray_out=25):
    parts = [stroke_arc(cx, cy, r, 0, 2 * math.pi)]
    for i in range(8):
        a = i * math.pi / 4
        parts.append(stroke_path([(cx + ray_in * math.cos(a), cy + ray_in * math.sin(a)),
                                  (cx + ray_out * math.cos(a), cy + ray_out * math.sin(a))]))
    return add(*parts)


def moon(cx, cy, r, cut_dx, cut_dy, cut_r):
    """Crescent: circle A minus circle B, outlined."""
    bx, by = cx + cut_dx, cy + cut_dy
    dist = math.hypot(cut_dx, cut_dy)
    base = math.atan2(cut_dy, cut_dx)
    # intersection angles on A and on B
    ta = math.acos((r * r + dist * dist - cut_r * cut_r) / (2 * r * dist))
    tb = math.acos((cut_r * cut_r + dist * dist - r * r) / (2 * cut_r * dist))
    # outer arc of A: the long way around, away from B
    outer = stroke_arc(cx, cy, r, base + ta, base + 2 * math.pi - ta)
    # inner arc: part of B inside A, bulging away from B's centre direction
    inner = stroke_arc(bx, by, cut_r, base + math.pi - tb, base + math.pi + tb)
    return add(outer, inner)


def bbox_center(mask):
    box = mask.getbbox()
    return ((box[0] + box[2]) / 2 / SS, (box[1] + box[3]) / 2 / SS)


def centered(mask):
    """Shifts the artwork so its bounding box is centred in the icon."""
    cx, cy = bbox_center(mask)
    return ImageChops.offset(mask, round((SIZE / 2 - cx) * SS), round((SIZE / 2 - cy) * SS))


def dots(points, r=None):
    r = STROKE * 0.7 if r is None else r
    m = canvas()
    d = ImageDraw.Draw(m)
    for x, y in points:
        disc(d, x, y, r)
    return m


def partly_cloudy_day():
    cl = cloud(36, 47, 0.8)
    gap = cloud_fill(36, 47, 0.8, STROKE / 2 + 2.5)
    return add(sub(sun(47, 27, r=10, ray_in=16, ray_out=23), gap), cl)


def partly_cloudy_night():
    # crescent behind a cloud, with a gap around the cloud
    cl = cloud(43, 47, 0.85)
    gap = cloud_fill(43, 47, 0.85, STROKE / 2 + 2.5)
    return add(sub(moon(29, 27, 15, 8, -6, 12.5), gap), cl)


def rain():
    return add(cloud(37, 27, 0.95), *[stroke_path([(x, 53), (x - 4, 62)]) for x in (26, 37, 48)])


def thunderstorm():
    return add(cloud(37, 27, 0.95), stroke_path([(40, 48), (32, 58), (41, 58), (33, 69)]))


def snow():
    return add(cloud(37, 27, 0.95), dots([(27, 55), (37, 63), (47, 55)]))


def fog():
    return add(cloud(37, 27, 0.95), stroke_path([(21, 54), (53, 54)]), stroke_path([(29, 63), (45, 63)]))


ICONS = {
    "weather_partly_cloudy_day": partly_cloudy_day,
    "weather_sunny": lambda: sun(37, 37),
    "weather_clear_night": lambda: moon(34, 38, 20, 10, -8, 17),
    "weather_partly_cloudy_night": partly_cloudy_night,
    "weather_cloudy": lambda: cloud(37, 37, 1.2),
    "weather_rain": rain,
    "weather_thunderstorm": thunderstorm,
    "weather_snow": snow,
    "weather_fog": fog,
}


def fitted(draw):
    """Draws the icon, scales it so its longer side is FILL px and centres it. The line width is drawn thinner
    (or thicker) by the scale factor so it ends up STROKE0 wide; a few passes make that converge."""
    global STROKE
    STROKE = STROKE0
    for _ in range(4):
        mask = draw()
        box = mask.getbbox()
        longer = max(box[2] - box[0], box[3] - box[1]) / SS
        factor = FILL / longer
        STROKE = STROKE0 / factor
    mask = draw()
    box = mask.getbbox()
    art = mask.crop(box)
    art = art.resize((round(art.width * factor), round(art.height * factor)), Image.LANCZOS)
    out = canvas()
    out.paste(art, ((out.width - art.width) // 2, (out.height - art.height) // 2))
    STROKE = STROKE0
    return out


def icons():
    return {name: fitted(draw) for name, draw in ICONS.items()}


HEADER = """
#if defined(LV_LVGL_H_INCLUDE_SIMPLE)
#include "lvgl.h"
#elif defined(LV_LVGL_H_INCLUDE_SYSTEM)
#include <lvgl.h>
#elif defined(LV_BUILD_TEST)
#include "../lvgl.h"
#else
#include "lvgl/lvgl.h"
#endif

#ifndef LV_ATTRIBUTE_MEM_ALIGN
#define LV_ATTRIBUTE_MEM_ALIGN
#endif

#ifndef LV_ATTRIBUTE_{U}
#define LV_ATTRIBUTE_{U}
#endif

static const
LV_ATTRIBUTE_MEM_ALIGN LV_ATTRIBUTE_LARGE_CONST LV_ATTRIBUTE_{U}
uint8_t {n}_map[] = {{
"""

FOOTER = """
}};

extern const lv_image_dsc_t {n} = {{
  .header = {{
    .magic = LV_IMAGE_HEADER_MAGIC,
    .cf = LV_COLOR_FORMAT_RGB565A8,
    .flags = 0,
    .w = {w},
    .h = {w},
    .stride = {stride},
    .reserved_2 = 0,
  }},
  .data_size = sizeof({n}_map),
  .data = {n}_map,
  .reserved = NULL,
}};
"""


def write(name, mask):
    alpha = mask.resize((SIZE, SIZE), Image.BOX)
    a = alpha.tobytes()
    rows = []
    for y in range(SIZE):  # RGB565 plane: white
        rows.append(",".join(["0xff"] * (SIZE * 2)) + ",")
    for y in range(SIZE):  # A8 plane
        rows.append(",".join("0x%02x" % v for v in a[y * SIZE:(y + 1) * SIZE]) + ",")
    with open(name + ".cpp", "w") as f:
        f.write(HEADER.format(U=name.upper(), n=name))
        f.write("\n".join("    " + r for r in rows))
        f.write(FOOTER.format(n=name, w=SIZE, stride=SIZE * 2))


if __name__ == "__main__":
    for n, m in icons().items():
        write(n, m)
        print("wrote", n + ".cpp")
