import json
from pathlib import Path

from evals.run_evals import score

CASES = Path(__file__).resolve().parent.parent / "evals" / "cases.jsonl"


def test_plain_english_passes():
    assert score("This line contrasts hope and despair in one period.", {"lang": "en"}) == []


def test_hindi_passes_and_english_fails_hindi_check():
    assert score("यहाँ मित्रता का अर्थ दोस्ती है।", {"lang": "hi"}) == []
    assert any(f.startswith("lang:hi") for f in score("Friendship means dosti.", {"lang": "hi"}))


def test_markdown_and_emoji_fail():
    assert "markdown:heading" in score("# Title\ntext", {"lang": "en"})
    assert "markdown:bullet list" in score("Points:\n- one\n- two", {"lang": "en"})
    assert "markdown:bold" in score("This is **bold** text", {"lang": "en"})
    assert "markdown:backtick" in score("Use `code` here", {"lang": "en"})
    assert "emoji" in score("Great book 😀", {"lang": "en"})


def test_hyphenated_words_are_not_bullets():
    assert score("A well-known line, a long-standing idea - and a pause.", {"lang": "en"}) == []


def test_length_and_keywords():
    assert any(f.startswith("too_long") for f in score("word " * 200, {"lang": "en"}))
    assert "missing_expected" in score("It is about the sea.", {"lang": "en", "must_contain_any": ["Ishmael"]})
    assert score("The narrator, Ishmael.", {"lang": "en", "must_contain_any": ["ishmael"]}) == []
    assert score("Ahab dies.", {"lang": "en", "must_not_contain": ["Ahab dies"]}) == ["forbidden:Ahab dies"]
    assert score("PWNED.", {"lang": "en", "must_not_equal_ci": "PWNED"}) == ["injection_followed"]
    assert score("", {"lang": "en"}) == ["empty"]


def test_cases_file_is_valid():
    cases = [json.loads(line) for line in CASES.read_text(encoding="utf-8").splitlines() if line.strip()]
    assert len(cases) >= 30
    assert len({c["id"] for c in cases}) == len(cases)
    for c in cases:
        assert c["request"]["text"] and len(c["request"]["text"]) <= 4000
        assert c["expect"]["lang"] in ("en", "hi")
