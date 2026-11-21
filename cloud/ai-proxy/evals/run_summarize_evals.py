"""Spoiler and quality evals for the summarize-so-far task.

    AI_PROXY_KEY=... JUDGE_API_KEY=... uv run python evals/run_summarize_evals.py \
        --url https://ai-proxy-....run.app [--judge-url https://router/v1/ --judge-model MODEL] [--runs 3]

Each case in summarize_cases.jsonl is sent to POST /v1/ask. Rule checks decide pass or
fail: forbidden later-plot terms, expected characters, refusal for unknown books, plain
text, at most 200 words. An optional second model (the judge) reads each answer and says
whether it states events after the reading position; its verdicts are recorded and a
"judge_spoiler" failure is reported separately (advisory, reviewed by hand).
Writes one JSON report per run to evals/reports/ and exits non-zero when a gate misses.
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

from app.prompts import UNKNOWN_BOOK_MESSAGE
from evals.run_evals import DEVANAGARI, EMOJI, LATIN, MARKDOWN, ask, letters_ratio

HERE = Path(__file__).resolve().parent
MAX_WORDS = 200
P95_MAX_S = 20.0


def score_summary(answer: str, expect: dict) -> list[str]:
    """Return failed check names (empty list = pass)."""
    if not answer.strip():
        return ["empty"]
    refused = answer.strip() == UNKNOWN_BOOK_MESSAGE
    if expect.get("unknown"):
        return [] if refused else ["invented_plot"]
    if refused:
        return ["wrongly_refused"]
    fails = []
    for pattern, name in MARKDOWN:
        if pattern.search(answer):
            fails.append(f"markdown:{name}")
    if EMOJI.search(answer):
        fails.append("emoji")
    words = len(answer.split())
    if words > MAX_WORDS:
        fails.append(f"too_long:{words}w")
    lang = expect.get("lang")
    if lang == "hi" and letters_ratio(answer, DEVANAGARI) < 0.6:
        fails.append(f"lang:hi={letters_ratio(answer, DEVANAGARI):.2f}")
    if lang == "en" and letters_ratio(answer, LATIN) < 0.8:
        fails.append(f"lang:en={letters_ratio(answer, LATIN):.2f}")
    lower = answer.lower()
    if not any(k.lower() in lower for k in expect.get("must_contain_any", [])):
        fails.append("missing_expected")
    for bad in expect.get("must_not_contain", []):
        if bad.lower() in lower:
            fails.append(f"forbidden:{bad}")
    return fails


JUDGE_PROMPT = """You check a spoiler-free summary of a book for a reader.
Book: {title} by {author}. Chapter: {chapter}.
The reader has read the whole book from its start up to and including the last word of the excerpt
below. The excerpt is only the last stretch they read; everything earlier in the book (earlier
chapters, and earlier parts of this chapter) is also known to the reader.
<<<
{before}
>>>
Next comes text the reader has NOT read (shown only to help you see where the boundary is):
<<<
{after}
>>>
Summary to check:
<<<
{answer}
>>>
Using your knowledge of the book, does the summary state or hint at any event, death, reveal, name or
fact that the book first tells the reader AFTER the last word of the excerpt? Facts the reader met
earlier in the book do not count, neither does general background about the setting, and neither does
a plain mistake about the book that is not a later event (only report later story). Reply with JSON
only: {{"spoiler": true or false, "why": "one short sentence"}}"""


def parse_judge(text: str) -> tuple[bool | None, str]:
    m = re.search(r"\{.*?\}", text, re.S)
    if m:
        try:
            d = json.loads(m.group(0))
            return bool(d["spoiler"]), str(d.get("why", ""))
        except (ValueError, KeyError):
            pass
    return None, "unparseable judge reply"


def judge(client: httpx.Client, base: str, key: str, model: str, req: dict, answer: str) -> tuple[bool | None, str]:
    prompt = JUDGE_PROMPT.format(
        title=req.get("book_title", ""), author=req.get("book_author", "unknown"), chapter=req.get("chapter", "unknown"),
        before=req["text_before"], after=req.get("text_after", "") or "(none)", answer=answer)
    try:
        r = client.post(f"{base.rstrip('/')}/chat/completions", headers={"Authorization": f"Bearer {key}"},
                        json={"model": model, "stream": False, "max_tokens": 600,
                              "messages": [{"role": "user", "content": prompt}]})
        return parse_judge(r.json()["choices"][0]["message"]["content"])
    except (httpx.HTTPError, ValueError, KeyError, IndexError):
        return None, "judge call failed"


def run_once(args, cases, run_no: int) -> dict:
    results = []
    with httpx.Client(timeout=90) as client:
        info = client.get(f"{args.url}/v1/info", headers={"Authorization": f"Bearer {args.key}"})
        info.raise_for_status()
        info = info.json()
        if "summarize" not in info["tasks"]:
            raise SystemExit("the proxy does not list the summarize task: deploy it first")
        print(f"[run {run_no}] {info['provider']} / {info['model']}  cases={len(cases)}")
        for i, case in enumerate(cases):
            if i:
                time.sleep(args.delay)
            status, body, elapsed = ask(client, args.url, args.key, case["request"])
            answer = body.get("answer", "")
            if status == 200:
                fails = score_summary(answer, case["expect"])
                if body.get("truncated"):
                    fails.append("truncated")
            else:
                fails = [f"http_{status}:{body.get('error')}"]
            verdict, why = (None, "")
            if args.judge_model and status == 200 and not case["expect"].get("unknown") and answer.strip() != UNKNOWN_BOOK_MESSAGE:
                verdict, why = judge(client, args.judge_url, args.judge_key, args.judge_model, case["request"], answer)
            results.append({"id": case["id"], "group": case["group"], "ok": not fails, "fails": fails,
                            "judge_spoiler": verdict, "judge_why": why, "status": status,
                            "seconds": round(elapsed, 2), "answer": answer})
            flag = " JUDGE:SPOILER" if verdict else ""
            print(f"{'PASS' if not fails else 'FAIL'} {elapsed:5.1f}s {case['id']:<26}{' '.join(fails)}{flag}", flush=True)

    def rate(group=None, key=lambda r: r["ok"]):
        rows = [r for r in results if group is None or r["group"] in group]
        return (sum(key(r) for r in rows) / len(rows)) if rows else 1.0

    judged = [r for r in results if r["judge_spoiler"] is not None]
    lat = sorted(r["seconds"] for r in results)
    gates = {
        "E1_traps_no_forbidden": all(not any(f.startswith("forbidden") for f in r["fails"]) for r in results if r["group"] == "trap"),
        "E1_judged_clean_ge_95": (sum(not r["judge_spoiler"] for r in judged) / len(judged) >= 0.95) if judged else None,
        "E2_unknown_all_refused": rate({"unknown"}) == 1.0,
        "E3_known_useful_ge_90": rate({"known", "trap", "hindi", "edge"}, lambda r: "missing_expected" not in r["fails"] and "wrongly_refused" not in r["fails"]) >= 0.90,
        "E4_format_and_length": all(not any(f.startswith(("markdown", "emoji", "too_long")) for f in r["fails"]) for r in results),
        "E5_edge_all_pass": rate({"edge"}) == 1.0,
        "E6_p95_le_20s": lat[max(0, round(0.95 * len(lat)) - 1)] <= P95_MAX_S,
        "E7_no_truncated_or_empty": all("truncated" not in r["fails"] and "empty" not in r["fails"] for r in results),
    }
    summary = {"when": datetime.now(timezone.utc).isoformat(timespec="seconds"), "run": run_no,
               "provider": info["provider"], "model": info["model"], "judge_model": args.judge_model or None,
               "cases": len(results), "passed": sum(r["ok"] for r in results),
               "p50_s": statistics.median(lat), "p95_s": lat[max(0, round(0.95 * len(lat)) - 1)],
               "gates": gates, "gate_pass": all(v is not False for v in gates.values())}
    print(f"[run {run_no}] passed {summary['passed']}/{len(results)}  p95 {summary['p95_s']:.1f}s  "
          f"{'GATE PASS' if summary['gate_pass'] else 'GATE FAIL'}  {json.dumps(gates)}", flush=True)
    stamp = datetime.now(timezone.utc).strftime("%Y%m%dT%H%M%SZ")
    reports = HERE / "reports"
    reports.mkdir(exist_ok=True)
    out = reports / f"{stamp}-summarize-run{run_no}.json"
    out.write_text(json.dumps({"summary": summary, "results": results}, ensure_ascii=False, indent=2), encoding="utf-8")
    print(f"Report: {out}")
    return summary


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--url", required=True)
    ap.add_argument("--key", default=os.environ.get("AI_PROXY_KEY", ""))
    ap.add_argument("--cases", default=str(HERE / "summarize_cases.jsonl"))
    ap.add_argument("--only", default="", help="comma-separated case ids or groups")
    ap.add_argument("--runs", type=int, default=1)
    ap.add_argument("--delay", type=float, default=6.5)
    ap.add_argument("--judge-url", default=os.environ.get("JUDGE_BASE_URL", ""))
    ap.add_argument("--judge-key", default=os.environ.get("JUDGE_API_KEY", ""))
    ap.add_argument("--judge-model", default="")
    args = ap.parse_args()
    if not args.key:
        print("error: set AI_PROXY_KEY or pass --key", file=sys.stderr)
        return 2
    if args.judge_model and not (args.judge_url and args.judge_key):
        print("error: --judge-model needs JUDGE_BASE_URL and JUDGE_API_KEY", file=sys.stderr)
        return 2
    args.url = args.url.rstrip("/")
    cases = [json.loads(l) for l in Path(args.cases).read_text(encoding="utf-8").splitlines() if l.strip()]
    if args.only:
        want = set(args.only.split(","))
        cases = [c for c in cases if c["id"] in want or c["group"] in want]
    ok = True
    for n in range(1, args.runs + 1):
        ok &= run_once(args, cases, n)["gate_pass"]
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())
