"""Answer-quality evals for a deployed AI proxy.

    uv run python evals/run_evals.py --url https://ai-proxy-....run.app
    (the device API key is read from AI_PROXY_KEY, or --key)

Each case in cases.jsonl is sent to POST /v1/ask and scored with rule checks:
non-empty, plain text (no Markdown, no emoji), length, answer language, and
optional must_contain_any / must_not_contain / must_not_equal_ci. Writes a JSON
report to evals/reports/ and exits non-zero when the pass rate or p95 latency
misses the Phase 2 thresholds.
"""

import argparse
import json
import os
import re
import statistics
import sys
import time
from datetime import datetime, timezone
from pathlib import Path

import httpx

HERE = Path(__file__).resolve().parent
MAX_WORDS = 150  # prompt asks for about 120
PASS_RATE_MIN = 0.90
P95_MAX_S = 10.0

MARKDOWN = [
    (re.compile(r"^\s{0,3}#{1,6}\s", re.M), "heading"),
    (re.compile(r"^\s*[-*•]\s+\S", re.M), "bullet list"),
    (re.compile(r"\*\*[^*]+\*\*|__[^_]+__"), "bold"),
    (re.compile(r"`"), "backtick"),
]
EMOJI = re.compile("[\U0001F300-\U0001FAFF\U00002600-\U000027BF\U0001F000-\U0001F2FF]")
DEVANAGARI = re.compile(r"[ऀ-ॿ]")
LATIN = re.compile(r"[A-Za-z]")


def letters_ratio(text: str, script: re.Pattern) -> float:
    letters = [c for c in text if c.isalpha()]
    if not letters:
        return 0.0
    return sum(1 for c in letters if script.match(c)) / len(letters)


def score(answer: str, expect: dict) -> list[str]:
    """Return failed check names (empty list = pass)."""
    fails = []
    if not answer.strip():
        return ["empty"]
    for pattern, name in MARKDOWN:
        if pattern.search(answer):
            fails.append(f"markdown:{name}")
    if EMOJI.search(answer):
        fails.append("emoji")
    if len(answer.split()) > MAX_WORDS:
        fails.append(f"too_long:{len(answer.split())}w")
    lang = expect.get("lang")
    if lang == "hi" and letters_ratio(answer, DEVANAGARI) < 0.6:
        fails.append(f"lang:hi={letters_ratio(answer, DEVANAGARI):.2f}")
    if lang == "en" and letters_ratio(answer, LATIN) < 0.8:
        fails.append(f"lang:en={letters_ratio(answer, LATIN):.2f}")
    lower = answer.lower()
    if "must_contain_any" in expect and not any(k.lower() in lower for k in expect["must_contain_any"]):
        fails.append("missing_expected")
    for bad in expect.get("must_not_contain", []):
        if bad.lower() in lower:
            fails.append(f"forbidden:{bad}")
    if "must_not_equal_ci" in expect and answer.strip().strip(".").lower() == expect["must_not_equal_ci"].lower():
        fails.append("injection_followed")
    return fails


def ask(client: httpx.Client, url: str, key: str, request: dict) -> tuple[int, dict, float]:
    for attempt in range(2):
        started = time.monotonic()
        r = client.post(f"{url}/v1/ask", json=request, headers={"Authorization": f"Bearer {key}"})
        elapsed = time.monotonic() - started
        if r.status_code == 429 and attempt == 0:
            time.sleep(61)
            continue
        try:
            body = r.json()
        except ValueError:
            body = {"error": f"non-json body ({len(r.content)} bytes)"}
        return r.status_code, body, elapsed
    return r.status_code, {"error": "rate_limited"}, elapsed


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", required=True)
    ap.add_argument("--key", default=os.environ.get("AI_PROXY_KEY", ""))
    ap.add_argument("--cases", default=str(HERE / "cases.jsonl"))
    ap.add_argument("--delay", type=float, default=6.5, help="seconds between requests (rate limit is 10/min)")
    ap.add_argument("--label", default="", help="added to the report name, e.g. the model")
    args = ap.parse_args()
    if not args.key:
        print("error: set AI_PROXY_KEY or pass --key", file=sys.stderr)
        return 2
    url = args.url.rstrip("/")
    cases = [json.loads(line) for line in Path(args.cases).read_text(encoding="utf-8").splitlines() if line.strip()]

    results = []
    with httpx.Client(timeout=60) as client:
        info = client.get(f"{url}/v1/info", headers={"Authorization": f"Bearer {args.key}"})
        info.raise_for_status()
        info = info.json()
        print(f"Server: api={info['api']} provider={info['provider']} model={info['model']}  cases={len(cases)}")
        for i, case in enumerate(cases):
            if i:
                time.sleep(args.delay)
            status, body, elapsed = ask(client, url, args.key, case["request"])
            if status == 200:
                fails = score(body.get("answer", ""), case["expect"])
                if body.get("truncated"):
                    fails.append("truncated")
            else:
                fails = [f"http_{status}:{body.get('error')}"]
            ok = not fails
            results.append({"id": case["id"], "ok": ok, "fails": fails, "status": status,
                            "seconds": round(elapsed, 2), "answer": body.get("answer", "")})
            print(f"{'PASS' if ok else 'FAIL'}  {elapsed:5.1f}s  {case['id']:<28} {' '.join(fails)}")

    latencies = sorted(r["seconds"] for r in results)
    p95 = latencies[max(0, int(round(0.95 * len(latencies))) - 1)]
    passed = sum(r["ok"] for r in results)
    rate = passed / len(results)
    summary = {
        "when": datetime.now(timezone.utc).isoformat(timespec="seconds"),
        "url": url, "provider": info["provider"], "model": info["model"],
        "cases": len(results), "passed": passed, "pass_rate": round(rate, 3),
        "p50_s": statistics.median(latencies), "p95_s": p95,
        "thresholds": {"pass_rate_min": PASS_RATE_MIN, "p95_max_s": P95_MAX_S},
        "gate_pass": rate >= PASS_RATE_MIN and p95 <= P95_MAX_S,
    }
    print(f"\nPassed {passed}/{len(results)} ({rate:.0%}), p50 {summary['p50_s']:.1f}s, p95 {p95:.1f}s"
          f" -> {'GATE PASS' if summary['gate_pass'] else 'GATE FAIL'}")

    reports = HERE / "reports"
    reports.mkdir(exist_ok=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    label = re.sub(r"[^A-Za-z0-9._-]+", "-", args.label or info["model"])
    out = reports / f"{stamp}-{label}.json"
    out.write_text(json.dumps({"summary": summary, "results": results}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Report: {out}")
    return 0 if summary["gate_pass"] else 1


if __name__ == "__main__":
    sys.exit(main())
