# /// script
# requires-python = ">=3.11"
# dependencies = ["pillow>=10"]
# ///
"""16:9 YouTube video from the demo capture and a voiceover (no device needed).

    uv run scripts/demo/youtube.py demo/out VOICEOVER_DIR OUT_DIR

VOICEOVER_DIR holds one MP3 per segment of scripts/demo/youtube_script.json.
Each card is shown for its segment's audio plus a short pause. Writes the
MP4, an SRT caption file and the cards. Needs ffmpeg.
"""

import json
import subprocess
import sys
from pathlib import Path

from PIL import Image, ImageDraw, ImageFilter

sys.path.insert(0, str(Path(__file__).resolve().parent.parent))
from demo.frames import ACCENT, load_font  # noqa: E402

W, H = 1920, 1080
BG = (246, 246, 243)
INK = (28, 28, 30)
MUTED = (110, 110, 110)
PAUSE = 0.45  # seconds of silence after each segment
FADE = 0.35
HERE = Path(__file__).resolve().parent


def font(size: int, bold: bool = False):
    if bold:
        from PIL import ImageFont

        for name, index in (("/System/Library/Fonts/HelveticaNeue.ttc", 1), ("/System/Library/Fonts/Helvetica.ttc", 1)):
            try:
                return ImageFont.truetype(name, size, index=index)
            except OSError:
                continue
    return load_font(size)


class Cards:
    def __init__(self, demo: Path):
        self.demo = demo

    def device(self, shot: str, height: int) -> Image.Image:
        img = Image.open(self.demo / "framed" / f"{shot}.png")
        s = height / img.height
        return img.resize((round(img.width * s), round(img.height * s)), Image.Resampling.LANCZOS)

    @staticmethod
    def canvas():
        c = Image.new("RGB", (W, H), BG)
        return c, ImageDraw.Draw(c)

    @staticmethod
    def text_block(d, x, y, lines, size, bold=True, fill=INK, gap=1.2):
        f = font(size, bold)
        for line in lines:
            d.text((x, y), line, font=f, fill=fill)
            y += f.size * gap
        return y

    @staticmethod
    def chip(d, x, y, text, size=40, fill=ACCENT, color=(255, 255, 255)):
        f = font(size, True)
        w = d.textlength(text, font=f)
        pad = size * 0.5
        box = (x, y, x + w + 3 * pad, y + size + 2 * pad)
        d.rounded_rectangle(box, radius=(box[3] - box[1]) / 2, fill=fill)
        d.text((x + 1.5 * pad, y + pad - size * 0.08), text, font=f, fill=color)
        return box[2]

    def left_text_right_device(self, title, subtitle, shot, chips=()):
        c, d = self.canvas()
        dev = self.device(shot, 900)
        c.paste(dev, (W - dev.width - 120, (H - dev.height) // 2), dev)
        y = self.text_block(d, 130, 300, title, 92)
        y = self.text_block(d, 130, y + 20, subtitle, 44, bold=False, fill=MUTED, gap=1.35)
        x = 130
        for t in chips:
            x = self.chip(d, x, y + 40, t, 34) + 18
        return c

    def hook(self):
        c, d = self.canvas()
        a, b = self.device("01-reading", 820), self.device("04-explain", 820)
        gap = 60
        x = (W - a.width - b.width - gap) // 2
        c.paste(a, (x, 40), a)
        c.paste(b, (x + a.width + gap, 40), b)
        f = font(76, True)
        t = "Stuck on one sentence?"
        d.text(((W - d.textlength(t, font=f)) / 2, H - 170), t, font=f, fill=INK)
        return c

    def problem(self):
        return self.left_text_right_device(["One confusing line", "is all it takes."], ["Dense classics. Technical books.", "A language you're still learning."],
                                           "01-reading", ("Old language", "Dense ideas", "New words"))

    def trap(self):
        c, d = self.canvas()
        # A phone with notifications, drawn simply.
        pw, ph = 470, 900
        px, py = W - pw - 220, (H - ph) // 2
        d.rounded_rectangle((px, py, px + pw, py + ph), radius=70, fill=(30, 30, 32))
        d.rounded_rectangle((px + 22, py + 22, px + pw - 22, py + ph - 22), radius=52, fill=(236, 236, 240))
        notes = [("Messages", "12 new messages"), ("News", "Breaking: you won't believe…"),
                 ("Social", "3 friends posted"), ("Video", "Recommended for you"), ("Shop", "Sale ends tonight!")]
        y = py + 110
        for app, msg in notes:
            d.rounded_rectangle((px + 50, y, px + pw - 50, y + 120), radius=26, fill=(255, 255, 255))
            d.text((px + 80, y + 22), app, font=font(28, True), fill=INK)
            d.text((px + 80, y + 64), msg, font=font(26), fill=MUTED)
            y += 145
        y = self.text_block(d, 130, 330, ["The fix that", "breaks your focus."], 92)
        self.text_block(d, 130, y + 20, ["Look up one word…", "ten minutes later, you're elsewhere."], 44, bold=False,
                        fill=MUTED, gap=1.35)
        return c

    def idea(self):
        return self.left_text_right_device(["Help that stays", "on the page."], ["What if the answer lived", "right where you're reading?"],
                                           "01-reading")

    def single(self, shot, label):
        c, d = self.canvas()
        dev = self.device(shot, 940)
        c.paste(dev, ((W - dev.width) // 2 + 330, (H - dev.height) // 2), dev)
        self.text_block(d, 130, 420, label, 80)
        return c

    def select(self):
        return self.single("02-select", ["Select.", "Tap Ask AI."])

    def picker(self):
        return self.single("03-picker", ["Choose what", "you need."])

    def explain(self):
        return self.single("04-explain", ["A plain answer,", "on e-ink."])

    def define(self):
        c = self.single("05-define", ["No apps.", "No feed.", "Back to the", "same line."])
        return c

    def languages(self):
        c, d = self.canvas()
        shots = ["06-other-language", "07-translate-english", "08-answer-spanish"]
        devs = [self.device(s, 760) for s in shots]
        gap = 50
        x = (W - sum(v.width for v in devs) - gap * 2) // 2
        for v in devs:
            c.paste(v, (x, 50), v)
            x += v.width + gap
        f = font(70, True)
        t = "Any book. Any language."
        d.text(((W - d.textlength(t, font=f)) / 2, H - 200), t, font=f, fill=INK)
        return c

    def privacy(self):
        c, d = self.canvas()
        self.text_block(d, 0, 0, [], 10)
        f = font(84, True)
        t = "Private by design"
        d.text(((W - d.textlength(t, font=f)) / 2, 130), t, font=f, fill=INK)
        boxes = [("Your e-reader", "only the page on screen"), ("Your own cloud", "your server, your key"),
                 ("AI provider", "OpenAI or Ollama")]
        bw, bh, gap = 460, 260, 130
        x = (W - 3 * bw - 2 * gap) // 2
        y = 420
        for i, (title, sub) in enumerate(boxes):
            d.rounded_rectangle((x, y, x + bw, y + bh), radius=36, fill=(255, 255, 255), outline=(210, 210, 210), width=3)
            tf, sf = font(48, True), font(34)
            d.text((x + (bw - d.textlength(title, font=tf)) / 2, y + 70), title, font=tf, fill=INK)
            d.text((x + (bw - d.textlength(sub, font=sf)) / 2, y + 150), sub, font=sf, fill=MUTED)
            if i < 2:
                ax = x + bw + 20
                d.line((ax, y + bh / 2, ax + gap - 40, y + bh / 2), fill=ACCENT, width=8)
                d.polygon([(ax + gap - 40, y + bh / 2 - 18), (ax + gap - 14, y + bh / 2), (ax + gap - 40, y + bh / 2 + 18)],
                          fill=ACCENT)
            x += bw + gap
        cx = (W - 3 * 240 - 2 * 30) // 2
        for t in ("Open source", "No spoilers", "Your data"):
            cx = self.chip(d, cx, 800, t, 38) + 30
        return c

    def takeaway(self):
        c, d = self.canvas()
        lines = ["The best use of AI", "can be keeping you", "exactly where you are."]
        f = font(96, True)
        y = (H - len(lines) * f.size * 1.25) / 2
        for line in lines:
            d.text(((W - d.textlength(line, font=f)) / 2, y), line, font=f, fill=INK)
            y += f.size * 1.25
        return c

    def outro(self):
        c, d = self.canvas()
        dev = self.device("04-explain", 820)
        c.paste(dev, (W - dev.width - 200, (H - dev.height) // 2), dev)
        y = self.text_block(d, 150, 360, ["PageMind"], 120)
        y = self.text_block(d, 150, y + 10, ["for the Xteink X4 Pro"], 52, bold=False, fill=MUTED)
        self.chip(d, 150, y + 60, "Free · open source · link below", 40)
        return c


def duration(path: Path) -> float:
    out = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", str(path)],
                         capture_output=True, text=True, check=True).stdout
    return float(out.strip())


def srt_time(t: float) -> str:
    ms = round(t * 1000)
    return f"{ms // 3600000:02d}:{ms // 60000 % 60:02d}:{ms // 1000 % 60:02d},{ms % 1000:03d}"


def main() -> int:
    demo, vo, out = (Path(a) for a in sys.argv[1:4])
    out.mkdir(parents=True, exist_ok=True)
    cfg = json.loads((HERE / "youtube_script.json").read_text())
    cards = Cards(demo)
    slides, srt = [], []
    t = 0.0
    for i, seg in enumerate(cfg["segments"]):
        audio = vo / f"{seg['id']}.mp3"
        secs = duration(audio) + PAUSE
        img = getattr(cards, seg["card"])()
        p = out / f"card-{seg['id']}.png"
        img.save(p)
        slides.append((p, secs, audio))
        srt.append(f"{i + 1}\n{srt_time(t)} --> {srt_time(t + secs - PAUSE)}\n{seg['text'].replace('A.I.', 'AI').replace('A.D.H.D.', 'ADHD')}\n")
        t += secs - FADE if i + 1 < len(cfg["segments"]) else secs
    (out / "captions.srt").write_text("\n".join(srt))

    # Audio: each segment padded to its slide length (minus the overlap of the fade).
    inputs, vf, af = [], [], []
    for i, (p, secs, _) in enumerate(slides):
        inputs += ["-loop", "1", "-t", f"{secs:.3f}", "-i", str(p)]
    for _, _, audio in slides:
        inputs += ["-i", str(audio)]
    n = len(slides)
    chain, offset = "[0:v]", 0.0
    for i in range(1, n):
        offset += slides[i - 1][1] - FADE
        vf.append(f"{chain}[{i}:v]xfade=transition=fade:duration={FADE}:offset={offset:.3f}[v{i}]")
        chain = f"[v{i}]"
    for i, (_, secs, _) in enumerate(slides):
        keep = secs - (FADE if i + 1 < n else 0)
        af.append(f"[{n + i}:a]apad,atrim=0:{keep:.3f},asetpts=PTS-STARTPTS[a{i}]")
    af.append("".join(f"[a{i}]" for i in range(n)) + f"concat=n={n}:v=0:a=1[aout]")
    mp4 = out / "pagemind-backpropdaily.mp4"
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", *inputs, "-filter_complex", ";".join(vf + af),
                    "-map", chain, "-map", "[aout]", "-c:v", "libx264", "-preset", "slow", "-crf", "18",
                    "-pix_fmt", "yuv420p", "-r", "30", "-c:a", "aac", "-b:a", "192k", "-movflags", "+faststart",
                    str(mp4)], check=True)
    print(f"done: {mp4} ({duration(mp4):.1f} s)")
    return 0


if __name__ == "__main__":
    sys.exit(main())
