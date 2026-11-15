# /// script
# requires-python = ">=3.11"
# dependencies = ["pyserial>=3.5", "pyyaml>=6", "pillow>=10"]
# ///
"""Drive a PageMind demo build over USB and capture presentable screenshots.

    uv run scripts/demo_capture.py demo/storyboard.yaml [--port /dev/cu.usbmodem101] [--video]

Needs the pagemind_x4pro_demo firmware (PAGEMIND_DEMO=1). Writes raw PNGs,
framed PNGs, README composites and (with --video) an MP4 and GIF to the
storyboard's output folder. See planning/demo-capture.md.
"""

import argparse
import glob
import json
import subprocess
import sys
import time
from pathlib import Path

import serial
import yaml

sys.path.insert(0, str(Path(__file__).resolve().parent))
from demo.frames import device_frame, framebuffer_to_image, on_background  # noqa: E402

STEP_KINDS = {"open", "page", "goto", "language", "select", "picker", "ask", "back", "wait_for", "sleep", "shot"}
TASKS = {"explain", "define", "translate", "ask"}


class StoryboardError(ValueError):
    pass


def load_storyboard(path: Path) -> dict:
    """Read and validate the storyboard before touching the device."""
    data = yaml.safe_load(path.read_text(encoding="utf-8")) or {}
    books = data.get("books") or {}
    scenes = data.get("scenes") or []
    if not scenes:
        raise StoryboardError("no scenes")
    names = set()
    for i, scene in enumerate(scenes):
        where = f"scene {i + 1} ({scene.get('name', '?')})"
        if not scene.get("name") or scene["name"] in names:
            raise StoryboardError(f"{where}: missing or duplicate name")
        names.add(scene["name"])
        for step in scene.get("steps") or []:
            if not isinstance(step, dict) or len(step) != 1:
                raise StoryboardError(f"{where}: each step needs exactly one action: {step!r}")
            (kind, value), = step.items()
            if kind not in STEP_KINDS:
                raise StoryboardError(f"{where}: unknown step '{kind}'")
            if kind == "open" and value not in books:
                raise StoryboardError(f"{where}: unknown book '{value}'")
            if kind == "select" and not (isinstance(value, list) and len(value) == 2
                                         and all(isinstance(v, int) and v >= 0 for v in value) and value[0] <= value[1]):
                raise StoryboardError(f"{where}: select needs [first, last] word indices")
            if kind == "ask" and value not in TASKS:
                raise StoryboardError(f"{where}: ask needs one of {sorted(TASKS)}")
            if kind == "page" and not isinstance(value, int):
                raise StoryboardError(f"{where}: page needs a number like 3 or -1")
            if kind == "goto" and not (isinstance(value, list) and len(value) == 2
                                       and all(isinstance(v, int) and v >= 0 for v in value)):
                raise StoryboardError(f"{where}: goto needs [spine, page]")
            if kind == "language" and not isinstance(value, str):
                raise StoryboardError(f"{where}: language needs a name like English")
    for name in ("hero", "steps", "hindi"):
        for shot in (data.get("composites") or {}).get(name, []):
            if not any(shot == s.get("shot") for sc in scenes for s in sc.get("steps") or [] if "shot" in s):
                raise StoryboardError(f"composite '{name}' uses unknown shot '{shot}'")
    return data


class Device:
    def __init__(self, port: str):
        self.ser = serial.Serial(port, 115200, timeout=0.5)
        time.sleep(0.5)
        self.ser.reset_input_buffer()

    def _line(self, deadline: float) -> str | None:
        while time.time() < deadline:
            raw = self.ser.readline()
            if raw:
                return raw.decode("utf-8", "replace").rstrip("\r\n")
        return None

    def command(self, cmd: str, timeout: float = 10) -> str:
        self.ser.write(f"CMD:{cmd}\n".encode())
        verb = cmd.split(" ", 1)[0]
        deadline = time.time() + timeout
        while (line := self._line(deadline)) is not None:
            if line.startswith(f"DEMO:OK {verb}"):
                return line[len(f"DEMO:OK {verb}"):].strip()
            if line.startswith(f"DEMO:ERR {verb}"):
                raise RuntimeError(f"device refused {cmd!r}: {line}")
        raise TimeoutError(f"no reply to {cmd!r}")

    def state(self) -> str:
        return self.command("STATE")

    def wait_for(self, activity: str, timeout: float = 60) -> None:
        deadline = time.time() + timeout
        while time.time() < deadline:
            try:
                if self.state() == activity:
                    return
            except (TimeoutError, serial.SerialException):
                pass  # the device may be restarting (Ask AI exit)
            time.sleep(1)
        raise TimeoutError(f"screen '{activity}' did not appear within {timeout}s (now: {self.state()})")

    def screenshot(self):
        self.ser.write(b"CMD:SHOT\n")
        deadline = time.time() + 20
        while (line := self._line(deadline)) is not None:
            if line.startswith("DEMO:SHOT "):
                size, orient, pw, ph, stride = (int(v) for v in line.split()[1:6])
                data = b""
                while len(data) < size and time.time() < deadline:
                    data += self.ser.read(size - len(data))
                if len(data) != size:
                    raise RuntimeError(f"screenshot short: {len(data)}/{size} bytes")
                return framebuffer_to_image(data, orient, pw, ph, stride)
        raise TimeoutError("no screenshot")


def find_port(arg: str | None) -> str:
    if arg:
        return arg
    ports = sorted(glob.glob("/dev/cu.usbmodem*") + glob.glob("/dev/ttyACM*"))
    if not ports:
        sys.exit("no device found: connect the X4 Pro by USB and wake it")
    return ports[0]


def run(board: dict, dev: Device, out: Path) -> list[dict]:
    raw_dir, framed_dir = out / "raw", out / "framed"
    raw_dir.mkdir(parents=True, exist_ok=True)
    framed_dir.mkdir(parents=True, exist_ok=True)
    settle = float(board.get("settle_seconds", 2.5))
    # Leave whatever screen a previous run stopped on.
    for _ in range(4):
        if dev.state() in ("EpubReader", "Home"):
            break
        dev.command("BACK")
        time.sleep(2)
    manifest = []
    for scene in board["scenes"]:
        print(f"== {scene['name']}")
        for step in scene.get("steps") or []:
            (kind, value), = step.items()
            if kind == "open":
                dev.command(f"OPEN {board['books'][value]}")
                dev.wait_for("EpubReader", 60)
            elif kind == "page":
                dev.command(f"PAGE {value}")
                time.sleep(2.5)
            elif kind == "goto":
                dev.command(f"GOTO {value[0]} {value[1]}")
                time.sleep(5)  # the chapter may be laid out first
            elif kind == "language":
                dev.command(f"LANG {value}")
            elif kind == "select":
                dev.command(f"SELECT {value[0]} {value[1]}")
                dev.wait_for("ClipSelection", 20)
            elif kind == "picker":
                dev.command("PICKER")
                dev.wait_for("AiAsk", 20)
            elif kind == "ask":
                dev.command(f"ASK {value}")
                dev.wait_for("DictionaryDefinition", 90)
            elif kind == "back":
                dev.command("BACK")
                time.sleep(1.5)
            elif kind == "wait_for":
                dev.wait_for(value, 90)
            elif kind == "sleep":
                time.sleep(float(value))
            if kind == "shot":
                time.sleep(settle)  # let the e-ink refresh finish
                img = dev.screenshot()
                raw = raw_dir / f"{value}.png"
                img.save(raw)
                device_frame(img).save(framed_dir / f"{value}.png")
                manifest.append({"scene": scene["name"], "shot": value, "caption": scene.get("caption", ""),
                                 "file": str(raw.relative_to(out))})
                print(f"   saved {raw.name}")
            else:
                print(f"   {kind}: {value}")
    (out / "manifest.json").write_text(json.dumps(manifest, indent=2, ensure_ascii=False))
    return manifest


def composites(board: dict, out: Path) -> None:
    from PIL import Image

    comp = board.get("composites") or {}
    framed = lambda name: Image.open(out / "framed" / f"{name}.png")  # noqa: E731
    readme = out / "readme"
    readme.mkdir(exist_ok=True)
    for name, shots in comp.items():
        if not shots:
            continue
        imgs = [framed(s) for s in shots]
        scale = 0.6 if len(imgs) > 2 else 0.8
        imgs = [i.resize((round(i.width * scale), round(i.height * scale))) for i in imgs]
        on_background(imgs, caption=(board.get("captions") or {}).get(name, "")).save(readme / f"{name}.png",
                                                                                      optimize=True)
        print(f"   composite readme/{name}.png")


def video(manifest: list[dict], out: Path, seconds: float = 4.0) -> None:
    from PIL import Image

    frames = out / "video"
    frames.mkdir(exist_ok=True)
    size = (1920, 1080)
    paths = []
    for i, m in enumerate(manifest):
        img = Image.open(out / "framed" / f"{m['shot']}.png")
        scale = (size[1] * 0.78) / img.height
        img = img.resize((round(img.width * scale), round(img.height * scale)))
        slide = on_background([img], caption=m.get("caption", ""), size=size)
        p = frames / f"slide{i:02d}.png"
        slide.save(p)
        paths.append(p)
    fade = 0.5
    inputs, filters = [], []
    for i, p in enumerate(paths):
        inputs += ["-loop", "1", "-t", str(seconds), "-i", str(p)]
    chain = "[0:v]"
    for i in range(1, len(paths)):
        offset = i * (seconds - fade)
        filters.append(f"{chain}[{i}:v]xfade=transition=fade:duration={fade}:offset={offset}[v{i}]")
        chain = f"[v{i}]"
    filt = ";".join(filters) if filters else "[0:v]null[v0]"
    last = chain if filters else "[v0]"
    mp4 = out / "pagemind-demo.mp4"
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", *inputs, "-filter_complex", filt, "-map", last,
                    "-pix_fmt", "yuv420p", "-r", "30", str(mp4)], check=True)
    gif = out / "pagemind-demo.gif"
    subprocess.run(["ffmpeg", "-y", "-loglevel", "error", "-i", str(mp4), "-vf",
                    "fps=8,scale=720:-1:flags=lanczos,split[a][b];[a]palettegen[p];[b][p]paletteuse",
                    str(gif)], check=True)
    print(f"   video {mp4.name}, {gif.name}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("storyboard", type=Path)
    ap.add_argument("--port")
    ap.add_argument("--out", type=Path)
    ap.add_argument("--video", action="store_true", help="also make an MP4 and GIF (needs ffmpeg)")
    ap.add_argument("--check", action="store_true", help="only validate the storyboard")
    args = ap.parse_args()
    try:
        board = load_storyboard(args.storyboard)
    except StoryboardError as e:
        print(f"storyboard error: {e}", file=sys.stderr)
        return 2
    if args.check:
        print("storyboard ok")
        return 0
    out = args.out or (args.storyboard.parent / board.get("output", "out"))
    dev = Device(find_port(args.port))
    print(f"device screen: {dev.state()}")
    manifest = run(board, dev, out)
    composites(board, out)
    if args.video:
        video(manifest, out)
    print(f"done: {out}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
