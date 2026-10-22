# by claude

import io
import struct
from pathlib import Path

import numpy as np
from PIL import Image

SAMPLES = 16
SIZES = [16, 20, 24, 32, 40, 48, 64, 256]
README_ICON_SIZE = 96
TILE = (28, 28, 28)
BORDER = (70, 70, 72)
GLYPH = (255, 255, 255)


def sample_grid(size):
    n = size * SAMPLES
    v = (np.arange(n) + 0.5) / SAMPLES
    return np.meshgrid(v, v)


def coverage(mask, size):
    return mask.reshape(size, SAMPLES, size, SAMPLES).mean(axis=(1, 3))


def round_rect(x, y, x0, y0, x1, y1, radius):
    inside = (x >= x0) & (x < x1) & (y >= y0) & (y < y1)
    cx = np.clip(x, x0 + radius, x1 - radius)
    cy = np.clip(y, y0 + radius, y1 - radius)
    return inside & ((x - cx) ** 2 + (y - cy) ** 2 <= radius * radius)


def glyph(x, y, size, glyph_size):
    center = size / 2
    stroke = max(1, round(glyph_size / 12))
    outer = round(glyph_size * 0.45)
    ring_inner = outer - stroke
    fill_radius = ring_inner - stroke
    distance_squared = (x - center) ** 2 + (y - center) ** 2
    is_on_ring = (distance_squared <= outer * outer) & (distance_squared > ring_inner * ring_inner)
    is_in_half = (x < center) & (distance_squared <= fill_radius * fill_radius)
    return is_on_ring | is_in_half


def render(size):
    x, y = sample_grid(size)
    radius = max(2.0, size * 0.22)
    border = max(1, round(size / 32))
    pad = round(size * 0.16)
    glyph_size = size - 2 * pad
    tile = round_rect(x, y, 0, 0, size, size, radius)
    tile_inner = round_rect(x, y, border, border, size - border, size - border, radius - border)
    tile_alpha = coverage(tile, size)
    border_alpha = coverage(tile & ~tile_inner, size)
    glyph_alpha = coverage(glyph(x, y, size, glyph_size), size)
    rgb = np.empty((size, size, 3))
    for channel in range(3):
        base = TILE[channel] * (1 - border_alpha) + BORDER[channel] * border_alpha
        rgb[..., channel] = base * (1 - glyph_alpha) + GLYPH[channel] * glyph_alpha
    rgba = np.dstack((rgb, tile_alpha * 255))
    pixels = np.clip(rgba + 0.5, 0, 255).astype(np.uint8)
    return Image.fromarray(pixels, "RGBA")


def write_ico(path, images):
    blobs = []
    for image in images:
        buffer = io.BytesIO()
        image.save(buffer, "PNG")
        blobs.append(buffer.getvalue())
    header = struct.pack("<HHH", 0, 1, len(images))
    offset = len(header) + 16 * len(images)
    entries = b""
    for image, blob in zip(images, blobs):
        side = image.width if image.width < 256 else 0
        entries += struct.pack("<BBBBHHII", side, side, 0, 0, 1, 32, len(blob), offset)
        offset += len(blob)
    Path(path).write_bytes(header + entries + b"".join(blobs))


def main():
    root = Path(__file__).resolve().parent.parent
    images = [render(size) for size in SIZES]
    write_ico(root / "res" / "app.ico", images)
    readme_icon = render(README_ICON_SIZE)
    readme_icon.save(root / "docs" / "icon.png")


if __name__ == "__main__":
    main()
