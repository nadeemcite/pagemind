"""Host tests for the demo capture tooling (planning/demo-capture.md A1, A2).

    uv run --with pytest --with pillow --with pyyaml --with pyserial pytest scripts/demo -q
"""

import random
import sys
from pathlib import Path

import pytest

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from demo.frames import framebuffer_to_image  # noqa: E402
from demo_capture import StoryboardError, load_storyboard  # noqa: E402

W, H = 24, 16  # small panel (native landscape), stride 3 bytes
STRIDE = W // 8


def reference(fb: bytes, orientation: int):
    """Pixel loop copied from src/util/ScreenshotUtil.cpp (logical (x, y) top-down)."""
    w, h = (H, W) if orientation in (0, 2) else (W, H)
    px = {}
    for y in range(h):
        for x in range(w):
            if orientation == 0:
                sx, sy = y, H - 1 - x
            elif orientation == 1:
                sx, sy = W - 1 - x, H - 1 - y
            elif orientation == 2:
                sx, sy = W - 1 - y, x
            else:
                sx, sy = x, y
            bit = (fb[sy * STRIDE + sx // 8] >> (7 - sx % 8)) & 1
            px[(x, y)] = 255 if bit else 0
    return (w, h), px


@pytest.mark.parametrize("orientation", [0, 1, 2, 3])
def test_matches_firmware_rotation(orientation):
    rng = random.Random(orientation)
    fb = bytes(rng.randrange(256) for _ in range(STRIDE * H))
    img = framebuffer_to_image(fb, orientation, W, H, STRIDE)
    size, px = reference(fb, orientation)
    assert img.size == size
    assert all(img.getpixel(k) == v for k, v in px.items())


def test_short_framebuffer_rejected():
    with pytest.raises(ValueError):
        framebuffer_to_image(b"\x00" * 3, 0, W, H, STRIDE)


def write(tmp_path, text):
    p = tmp_path / "s.yaml"
    p.write_text(text)
    return p


GOOD = """
books: {english: /Books/a.epub}
scenes:
  - name: one
    caption: Hi
    steps: [{open: english}, {page: 2}, {select: [3, 9]}, {ask: explain}, {shot: s1}, {back: true}]
composites: {hero: [s1]}
"""


def test_good_storyboard(tmp_path):
    assert load_storyboard(write(tmp_path, GOOD))["scenes"][0]["name"] == "one"


@pytest.mark.parametrize("bad, message", [
    ("scenes: []", "no scenes"),
    ("books: {}\nscenes: [{name: a, steps: [{open: nope}]}]", "unknown book"),
    ("scenes: [{name: a, steps: [{fly: 1}]}]", "unknown step"),
    ("scenes: [{name: a, steps: [{select: [9, 3]}]}]", "select needs"),
    ("scenes: [{name: a, steps: [{ask: summarize}]}]", "ask needs"),
    ("scenes: [{name: a, steps: [{shot: x, caption: y}]}]", "exactly one action"),
    ("scenes: [{name: a}, {name: a}]", "duplicate"),
    ("scenes: [{name: a, steps: [{shot: x}]}]\ncomposites: {hero: [y]}", "unknown shot"),
])
def test_bad_storyboards(tmp_path, bad, message):
    with pytest.raises(StoryboardError, match=message):
        load_storyboard(write(tmp_path, bad))
