# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow>=10"]
# ///
"""LinkedIn kit from the last demo capture (no device needed).

    uv run scripts/demo/linkedin.py demo/out [--episode 1]

Writes demo/out/linkedin/: a 4:5 video (title card, scenes with taps, end card),
a cover image, a carousel PDF and post.txt. Needs ffmpeg.
"""

import argparse
import json
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from demo.frames import ACCENT, load_font  # noqa: E402

W, H = 1080, 1350  # LinkedIn 4:5
BG = (246, 246, 243)
INK = (28, 28, 30)
MUTED = (110, 110, 110)
REPO = "github.com/nadeemcite/pagemind"


def font(size: int, bold: bool = False):
    if bold:
        from PIL import ImageFont

        for name, index in (("/System/Library/Fonts/HelveticaNeue.ttc", 1), ("/System/Library/Fonts/Helvetica.ttc", 1)):
            try:
                return ImageFont.truetype(name, size, index=index)
            except OSError:
                continue
    return load_font(size)


def centered(d: ImageDraw.ImageDraw, y: float, text: str, f, fill=INK) -> float:
    w = d.textlength(text, font=f)
    d.text(((W - w) / 2, y), text, font=f, fill=fill)
    return y + f.size * 1.25


def pill(d: ImageDraw.ImageDraw, y: float, text: str, f) -> float:
    w = d.textlength(text, font=f)
    pad = f.size * 0.55
    box = ((W - w) / 2 - pad * 1.6, y, (W + w) / 2 + pad * 1.6, y + f.size + 2 * pad)
    d.rounded_rectangle(box, radius=(box[3] - box[1]) / 2, fill=ACCENT)
    d.text(((W - w) / 2, y + pad - f.size * 0.08), text, font=f, fill=(255, 255, 255))
    return box[3]


def device(out: Path, shot: str, height: int) -> Image.Image:
    img = Image.open(out / "framed" / f"{shot}.png")
    s = height / img.height
    return img.resize((round(img.width * s), round(img.height * s)), Image.Resampling.LANCZOS)


def title_card(episode: int, out: Path) -> Image.Image:
    c = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(c)
    y = pill(d, 150, f"Weekend Builds #{episode}", font(40, True)) + 60
    y = centered(d, y, "My e-reader now", font(84, True))
    y = centered(d, y, "explains what", font(84, True))
    y = centered(d, y, "I'm reading", font(84, True)) + 30
    centered(d, y, "Select. Tap Ask AI. Read the answer on e-ink.", font(38), MUTED)
    dev = device(out, "04-explain", 640)
    c.paste(dev, ((W - dev.width) // 2, H - dev.height - 40), dev)
    return c


def scene(out: Path, shot: str, caption: str) -> Image.Image:
    c = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(c)
    dev = device(out, shot, 1060)
    c.paste(dev, ((W - dev.width) // 2, 40), dev)
    centered(d, H - 190, caption, font(52, True))
    return c


def end_card(episode: int, out: Path) -> Image.Image:
    c = Image.new("RGB", (W, H), BG)
    d = ImageDraw.Draw(c)
    left, right = device(out, "07-translate-english", 560), device(out, "08-answer-spanish", 560)
    gap = 30
    x = (W - left.width - right.width - gap) // 2
    c.paste(left, (x, 90), left)
    c.paste(right, (x + left.width + gap, 90), right)
    y = 90 + left.height + 50
    y = centered(d, y, "Your own cloud. Any language.", font(56, True))
    y = centered(d, y + 6, "Open source · weekend hobby project", font(38), MUTED) + 30
    pill(d, y, REPO, font(38, True))
    return c


def make_video(slides: list[tuple[Image.Image, float]], path: Path, tmp: Path) -> None:
    tmp.mkdir(parents=True, exist_ok=True)
    inputs, filters = [], []
    for i, (img, secs) in enumerate(slides):
        p = tmp / f"s{i:02d}.png"
        img.save(p)
        inputs += ["-loop", "1", "-t", f"{secs}", "-i", str(p)]
    fade, offset, chain = 0.35, 0.0, "[0:v]"
    for i in range(1, len(slides)):
        offset += slides[i - 1][1] - fade
        filters.append(f"{chain}[{i}:v]xfade=transition=fade:duration={fade}:offset={offset:.2f}[v{i}]")
        chain = f"[v{i}]"
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", *inputs, "-filter_complex", ";".join(filters), "-map", chain,
                    "-c:v", "libx264", "-preset", "slow", "-crf", "18", "-pix_fmt", "yuv420p", "-r", "30",
                    "-movflags", "+faststart", str(path)], check=True)


POST = """My $99 e-reader now explains what I'm reading. 🤯
Select a passage, tap Ask AI, and get the answer right on the e-ink page. Built over a weekend.

📖 Explain, define, translate, or ask anything about the passage
🌍 Any language: I tried Hindi, English and Spanish
🔒 Runs on my own cloud. Only the current page is sent, so no spoilers

Weekend Builds #{episode}. Pure hobby, open source, built on the CrossPoint Reader firmware. Link in the comments 👇

Next weekend: [teaser] 👀

#WeekendBuilds #eink #AI #OpenSource #Xteink
"""

COMMENT = """Code, setup guide and full demo: https://{repo}
⭐ Stars welcome, and tell me what I should build next weekend!
"""


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("out", type=Path)
    ap.add_argument("--episode", type=int, default=1)
    args = ap.parse_args()
    out = args.out
    manifest = {m["shot"]: m for m in json.loads((out / "manifest.json").read_text())}
    kit = out / "linkedin"
    kit.mkdir(exist_ok=True)

    cap = lambda s: manifest[s].get("caption", "")  # noqa: E731
    slides = [(title_card(args.episode, out), 2.2)]
    for shot in ("01-reading", "02-select", "03-picker", "04-explain", "05-define",
                 "06-other-language", "07-translate-english", "08-answer-spanish"):
        if manifest[shot].get("taps"):
            slides.append((scene(out, f"{shot}-plain", cap(shot)), 1.3))
            slides.append((scene(out, shot, cap(shot)), 1.8))
        else:
            slides.append((scene(out, shot, cap(shot)), 2.6))
    slides.append((end_card(args.episode, out), 3.5))

    make_video(slides, kit / f"pagemind-weekend-builds-{args.episode}.mp4", kit / "frames")
    slides[0][0].save(kit / "cover.png", optimize=True)
    # Carousel: every slide except the untapped duplicates of tapped scenes.
    keep = [img for img, secs in slides if secs != 1.3]
    keep[0].save(kit / "carousel.pdf", save_all=True, append_images=keep[1:], resolution=150)
    (kit / "post.txt").write_text(POST.format(episode=args.episode))
    (kit / "first-comment.txt").write_text(COMMENT.format(repo=REPO))
    total = sum(s for _, s in slides) - 0.35 * (len(slides) - 1)
    print(f"done: {kit} ({len(slides)} slides, about {total:.0f} s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
