"""Framebuffer conversion and presentation images for the demo capture."""

from PIL import Image, ImageDraw, ImageFilter, ImageFont

# GfxRenderer::Orientation
PORTRAIT, LANDSCAPE_CW, PORTRAIT_INVERTED, LANDSCAPE_CCW = range(4)


def framebuffer_to_image(data: bytes, orientation: int, panel_w: int, panel_h: int, stride: int) -> Image.Image:
    """1-bit panel framebuffer (bit set = white, MSB first) to an image in reader orientation.

    Mirrors the logical-to-panel transform in src/util/ScreenshotUtil.cpp.
    """
    if len(data) < stride * panel_h:
        raise ValueError(f"framebuffer too short: {len(data)} < {stride * panel_h}")
    panel = Image.frombytes("1", (panel_w, panel_h), bytes(data[: stride * panel_h]), "raw", "1", stride)
    # Panel image is in LandscapeCounterClockwise (native) orientation.
    if orientation == LANDSCAPE_CCW:
        img = panel
    elif orientation == PORTRAIT:
        img = panel.transpose(Image.Transpose.ROTATE_270)  # out(x,y) = panel(y, H-1-x)
    elif orientation == PORTRAIT_INVERTED:
        img = panel.transpose(Image.Transpose.ROTATE_90)  # out(x,y) = panel(W-1-y, x)
    elif orientation == LANDSCAPE_CW:
        img = panel.transpose(Image.Transpose.ROTATE_180)
    else:
        raise ValueError(f"unknown orientation {orientation}")
    return img.convert("L")


def load_font(size: int) -> ImageFont.ImageFont:
    for name in ("/System/Library/Fonts/SFNS.ttf", "/System/Library/Fonts/Helvetica.ttc",
                 "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf"):
        try:
            return ImageFont.truetype(name, size)
        except OSError:
            continue
    return ImageFont.load_default()


def device_frame(screen: Image.Image, scale: float = 1.0) -> Image.Image:
    """Screen inside a simple e-reader bezel with a soft shadow, on a transparent background."""
    screen = screen.convert("RGB")
    if scale != 1.0:
        screen = screen.resize((round(screen.width * scale), round(screen.height * scale)), Image.Resampling.LANCZOS)
    # Slight paper tint so the screen reads as e-ink, not a white rectangle.
    paper = Image.new("RGB", screen.size, (244, 242, 236))
    ink = Image.new("RGB", screen.size, (24, 24, 24))
    screen = Image.composite(paper, ink, screen.convert("L").point(lambda v: 255 if v > 127 else 0))

    side = round(min(screen.size) * 0.06)
    chin = round(side * 1.6)
    body_w, body_h = screen.width + 2 * side, screen.height + side + chin
    margin = round(side * 1.5)
    out = Image.new("RGBA", (body_w + 2 * margin, body_h + 2 * margin), (0, 0, 0, 0))

    shadow = Image.new("RGBA", out.size, (0, 0, 0, 0))
    ImageDraw.Draw(shadow).rounded_rectangle(
        (margin, margin + side // 3, margin + body_w, margin + body_h + side // 3), radius=side, fill=(0, 0, 0, 90))
    out = Image.alpha_composite(out, shadow.filter(ImageFilter.GaussianBlur(side // 2)))

    body = ImageDraw.Draw(out)
    body.rounded_rectangle((margin, margin, margin + body_w, margin + body_h), radius=side, fill=(38, 38, 40))
    out.paste(screen, (margin + side, margin + side))
    return out


def on_background(images: list[Image.Image], caption: str = "", gap: int = 48,
                  bg=(246, 246, 243), size: tuple[int, int] | None = None) -> Image.Image:
    """Lay framed images side by side on a light background, with an optional caption."""
    w = sum(i.width for i in images) + gap * (len(images) - 1)
    h = max(i.height for i in images)
    font = load_font(max(28, h // 22))
    cap_h = round(font.size * 2.2) if caption else 0
    pad = gap
    canvas_w, canvas_h = (w + 2 * pad, h + 2 * pad + cap_h) if size is None else size
    canvas = Image.new("RGB", (canvas_w, canvas_h), bg)
    x = (canvas_w - w) // 2
    y = (canvas_h - cap_h - h) // 2
    for img in images:
        canvas.paste(img, (x, y + (h - img.height) // 2), img if img.mode == "RGBA" else None)
        x += img.width + gap
    if caption:
        d = ImageDraw.Draw(canvas)
        tw = d.textlength(caption, font=font)
        d.text(((canvas_w - tw) / 2, canvas_h - cap_h + (cap_h - font.size) / 2 - pad / 3), caption,
               font=font, fill=(40, 40, 40))
    return canvas
