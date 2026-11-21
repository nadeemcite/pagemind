"""Unit tests for the summarize eval scorer and the case file (no network)."""

import json
from collections import Counter
from pathlib import Path

from jsonschema import Draft202012Validator

from app.prompts import UNKNOWN_BOOK_MESSAGE
from evals.run_summarize_evals import parse_judge, score_summary

HERE = Path(__file__).resolve().parent.parent
CASES = [json.loads(l) for l in (HERE / "evals" / "summarize_cases.jsonl").read_text(encoding="utf-8").splitlines() if l.strip()]
REQUEST = Draft202012Validator(json.loads((HERE / "contract" / "ask-request.schema.json").read_text(encoding="utf-8")))

KNOWN = {"lang": "en", "must_contain_any": ["Bennet"], "must_not_contain": ["Wickham"]}


def test_good_summary_passes():
    assert score_summary("Mr. Bennet hears from his wife that Netherfield Park is let.", KNOWN) == []


def test_forbidden_later_fact_fails():
    assert score_summary("Mr. Bennet's daughter later meets Wickham.", KNOWN) == ["forbidden:Wickham"]


def test_missing_expected_fails():
    assert "missing_expected" in score_summary("A man hears some news about a house.", KNOWN)


def test_known_book_refused_fails():
    assert "wrongly_refused" in score_summary(UNKNOWN_BOOK_MESSAGE, KNOWN)


def test_unknown_book_must_be_refused():
    assert score_summary(UNKNOWN_BOOK_MESSAGE, {"unknown": True}) == []
    fails = score_summary("Ilse takes the ferry and meets a man in a green coat who knows the chart.", {"unknown": True})
    assert fails == ["invented_plot"]


def test_summary_length_limit_is_200_words():
    assert any(f.startswith("too_long") for f in score_summary("Bennet " + "word " * 210, KNOWN))
    assert score_summary("Bennet " + "word " * 150, KNOWN) == []


def test_format_checks_apply():
    assert "markdown:bold" in score_summary("**Bennet** is here.", KNOWN)
    assert "emoji" in score_summary("Bennet 😀", KNOWN)


def test_parse_judge():
    assert parse_judge('{"spoiler": false, "why": "ok"}') == (False, "ok")
    assert parse_judge('Sure. ```json\n{"spoiler": true, "why": "mentions the duel"}\n```') == (True, "mentions the duel")
    assert parse_judge("no idea")[0] is None


def test_case_file_shape_and_coverage():
    ids = [c["id"] for c in CASES]
    assert len(ids) == len(set(ids))
    groups = Counter(c["group"] for c in CASES)
    assert len(CASES) >= 30
    assert groups["unknown"] >= 6 and groups["trap"] >= 6 and groups["edge"] >= 4 and groups["hindi"] >= 3


def test_every_case_request_matches_contract_schema():
    for c in CASES:
        REQUEST.validate(c["request"])
        assert c["request"]["task"] == "summarize", c["id"]


def test_expectations_are_complete():
    for c in CASES:
        e = c["expect"]
        if e.get("unknown"):
            continue
        assert e["must_contain_any"], c["id"]
        assert e["must_not_contain"], f"{c['id']}: spoiler cases need forbidden terms"


def test_traps_hold_the_future_in_text_after():
    # A trap only works when the later events really sit in text_after and are forbidden in the answer.
    for c in (c for c in CASES if c["group"] == "trap"):
        assert len(c["request"]["text_after"]) > 100, c["id"]
        assert len(c["expect"]["must_not_contain"]) >= 3, c["id"]


def test_edge_cases_cover_missing_pieces():
    edges = [c["request"] for c in CASES if c["group"] == "edge"]
    assert any(not r["text_after"] for r in edges)          # chapter end
    assert any(len(r["text_before"]) < 60 for r in edges)   # chapter start
    assert any("chapter" not in r for r in edges)           # no chapter name
    assert any("book_author" not in r for r in edges)       # no author


def test_forbidden_terms_are_not_in_the_text_the_reader_has_read():
    # A forbidden term that already appears in text_before would fail a correct answer.
    for c in CASES:
        before = c["request"]["text_before"].lower()
        for term in c["expect"].get("must_not_contain", []):
            assert term.lower() not in before, f"{c['id']}: '{term}' is in text_before"
