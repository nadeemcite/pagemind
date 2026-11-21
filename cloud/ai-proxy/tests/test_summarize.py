"""Summarize-so-far task (planning/summarize-so-far.md, Phase 1 gates S1-S6)."""

import json
from pathlib import Path

import pytest
from jsonschema import Draft202012Validator, ValidationError

from app.prompts import SUMMARIZE_SYSTEM_PROMPT, UNKNOWN_BOOK_MARKER, UNKNOWN_BOOK_MESSAGE, Task, build_summarize_prompt
from app.providers import Answer
from tests.test_api import AUTH, TOKEN, FakeProvider, make_client

CONTRACT = Path(__file__).resolve().parent.parent / "contract"
REQUEST = Draft202012Validator(json.loads((CONTRACT / "ask-request.schema.json").read_text(encoding="utf-8")))

BASE = {
    "task": "summarize",
    "book_title": "Pride and Prejudice",
    "book_author": "Jane Austen",
    "chapter": "Chapter 3",
    "text_before": "Mr. Bingley was good-looking and gentlemanlike.",
    "text_after": "The evening passed.",
}


def body(**changes):
    out = {**BASE, **changes}
    return {k: v for k, v in out.items() if v is not None}


# S1: schema
def test_schema_accepts_summarize_without_text():
    REQUEST.validate(body())


@pytest.mark.parametrize(
    "bad",
    [
        body(text_before=None),                # required for summarize
        body(text_before=""),                  # must not be empty
        body(text_before="x" * 3001),
        body(text_after="x" * 3001),
        body(chapter="c" * 201),
        body(unknown_field="x"),
        {"task": "explain", "text_before": "x"},  # other tasks still need text
    ],
)
def test_schema_rejects(bad):
    with pytest.raises(ValidationError):
        REQUEST.validate(bad)


def test_schema_allows_empty_after_and_no_chapter():
    REQUEST.validate(body(text_after="", chapter=None))


# S2: other tasks keep working and still need text
def test_explain_still_requires_text():
    client, _ = make_client()
    assert client.post("/v1/ask", json={"task": "explain"}, headers=AUTH).status_code == 422
    assert client.post("/v1/ask", json={"text": "x"}, headers=AUTH).status_code == 200


@pytest.mark.parametrize("changes", [{"text_before": None}, {"text_before": "   "}])
def test_summarize_needs_text_before(changes):
    client, provider = make_client()
    r = client.post("/v1/ask", json=body(**changes), headers=AUTH)
    assert (r.status_code, r.json()) == (422, {"error": "invalid_request"})
    assert provider.calls == []


@pytest.mark.parametrize("field", ["text_before", "text_after"])
def test_summarize_window_over_limit_is_413(field):
    client, provider = make_client(max_window_chars=50)
    r = client.post("/v1/ask", json=body(**{field: "x" * 51}), headers=AUTH)
    assert (r.status_code, r.json()) == (413, {"error": "context_too_long"})
    assert provider.calls == []


# S3: prompt builder
def test_prompt_has_book_chapter_both_windows_and_boundary():
    p = build_summarize_prompt(
        book_title="Moby-Dick", book_author="Herman Melville", chapter="Chapter 1",
        text_before="Call me Ishmael.", text_after="Some years ago.", language="Hindi",
    )
    assert '"Moby-Dick" by Herman Melville' in p
    assert "Chapter 1" in p
    assert "Call me Ishmael." in p and "Some years ago." in p
    assert "Hindi" in p
    # The boundary sits between the two windows.
    assert p.index("Call me Ishmael.") < p.index("READING POSITION") < p.index("Some years ago.")


def test_prompt_without_chapter_or_after_omits_them():
    p = build_summarize_prompt(book_title="B", book_author="", chapter="", text_before="Only this.", text_after="")
    assert "Chapter:" not in p and "Text after" not in p
    assert "Only this." in p and "READING POSITION" in p


def test_prompt_chapter_end_omits_after_section():
    p = build_summarize_prompt(book_title="B", book_author="A", chapter="Ch 9", text_before="x", text_after="")
    assert "Text after" not in p


def test_system_prompt_rules():
    low = SUMMARIZE_SYSTEM_PROMPT.lower()
    assert UNKNOWN_BOOK_MARKER in SUMMARIZE_SYSTEM_PROMPT
    assert "plain text" in low
    assert "own knowledge" in low
    # Boundary rules found necessary by the Phase 2 evals: exact position, same-chapter leaks, self-check.
    assert "exact boundary" in low and "even later in the same chapter" in low
    assert "only include a fact if the reader has already met it" in low
    assert "check every sentence" in low


# S4: unknown book
class UnknownProvider(FakeProvider):
    async def complete(self, system, user, max_tokens):
        self.calls.append((system, user, max_tokens))
        return Answer(text=f"  {UNKNOWN_BOOK_MARKER}\n", model="fake-model", truncated=False)


def test_unknown_book_returns_fixed_message_with_200():
    client, provider = make_client(provider=UnknownProvider())
    r = client.post("/v1/ask", json=body(), headers=AUTH)
    assert r.status_code == 200
    assert r.json() == {"answer": UNKNOWN_BOOK_MESSAGE, "model": "fake-model", "truncated": False}


def test_marker_only_applies_to_summarize():
    client, _ = make_client(provider=UnknownProvider())
    r = client.post("/v1/ask", json={"text": "x"}, headers=AUTH)
    assert r.json()["answer"].strip() == UNKNOWN_BOOK_MARKER


def test_normal_answer_passes_through():
    client, _ = make_client()
    r = client.post("/v1/ask", json=body(), headers=AUTH)
    assert r.json()["answer"] == "An answer."


# S5: output budget and system prompt per task
def test_summarize_budget_and_system_prompt():
    client, provider = make_client(summarize_max_output_tokens=900)
    client.post("/v1/ask", json=body(), headers=AUTH)
    client.post("/v1/ask", json={"text": "x"}, headers=AUTH)
    (sys1, _, tok1), (sys2, _, tok2) = provider.calls
    assert (sys1, tok1) == (SUMMARIZE_SYSTEM_PROMPT, 900)
    assert sys2 != SUMMARIZE_SYSTEM_PROMPT and tok2 == 400


# S6: info
def test_info_lists_summarize_and_window_limit():
    client, _ = make_client(max_window_chars=2500)
    info = client.get("/v1/info", headers=AUTH).json()
    assert "summarize" in info["tasks"]
    assert info["max_window_chars"] == 2500
    assert Task.SUMMARIZE.value == "summarize"


def test_summarize_prompt_reaches_provider():
    client, provider = make_client()
    client.post("/v1/ask", json=body(language="Hindi"), headers=AUTH)
    _, user, _ = provider.calls[0]
    assert "Mr. Bingley" in user and "Hindi" in user and TOKEN not in user
