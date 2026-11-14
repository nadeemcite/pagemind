"""Contract v1: fixtures validate against the schemas, and the server's real responses do too."""

import json
from pathlib import Path

import pytest
from jsonschema import Draft202012Validator

from tests.test_api import AUTH, FakeProvider, make_client

CONTRACT = Path(__file__).resolve().parent.parent / "contract"


def schema(name: str) -> Draft202012Validator:
    s = json.loads((CONTRACT / name).read_text(encoding="utf-8"))
    Draft202012Validator.check_schema(s)
    return Draft202012Validator(s)


def fixtures(kind: str) -> list[Path]:
    files = sorted((CONTRACT / "fixtures" / kind).glob("*.json"))
    assert files, f"no {kind} fixtures"
    return files


REQUEST = schema("ask-request.schema.json")
RESPONSE = schema("ask-response.schema.json")
INFO = schema("info-response.schema.json")
ERROR = schema("error.schema.json")


@pytest.mark.parametrize("path", fixtures("requests"), ids=lambda p: p.name)
def test_request_fixtures_valid_and_accepted(path):
    body = json.loads(path.read_text(encoding="utf-8"))
    REQUEST.validate(body)
    client, _ = make_client()
    r = client.post("/v1/ask", json=body, headers=AUTH)
    assert r.status_code == 200, r.text
    RESPONSE.validate(r.json())


@pytest.mark.parametrize("path", fixtures("responses"), ids=lambda p: p.name)
def test_response_fixtures_valid(path):
    body = json.loads(path.read_text(encoding="utf-8"))
    (INFO if path.name.startswith("info") else RESPONSE).validate(body)


@pytest.mark.parametrize("path", fixtures("errors"), ids=lambda p: p.name)
def test_error_fixtures_valid(path):
    ERROR.validate(json.loads(path.read_text(encoding="utf-8")))


def test_every_error_code_has_a_fixture():
    codes = set(json.loads((CONTRACT / "error.schema.json").read_text())["properties"]["error"]["enum"])
    have = {json.loads(p.read_text())["error"] for p in fixtures("errors")}
    assert codes == have


def test_info_response_matches_schema():
    client, _ = make_client()
    INFO.validate(client.get("/v1/info", headers=AUTH).json())


def _error_cases():
    big = {"text": "x" * 5000}
    return [
        ("401-unauthorized", lambda c: c.post("/v1/ask", json={"text": "x"})),
        ("422-invalid_request", lambda c: c.post("/v1/ask", json={"task": "nope", "text": "x"}, headers=AUTH)),
        ("413-text_too_long", lambda c: c.post("/v1/ask", json=big, headers=AUTH)),
        ("413-context_too_long", lambda c: c.post("/v1/ask", json={"text": "x", "context": "y" * 3000}, headers=AUTH)),
        ("413-body_too_large", lambda c: c.post("/v1/ask", content=b"x" * 40000, headers=AUTH)),
    ]


@pytest.mark.parametrize("name,call", _error_cases(), ids=[n for n, _ in _error_cases()])
def test_server_errors_match_fixtures(name, call):
    client, _ = make_client()
    r = call(client)
    expected = json.loads((CONTRACT / "fixtures" / "errors" / f"{name}.json").read_text())
    assert r.status_code == int(name.split("-")[0])
    assert r.json() == expected
    ERROR.validate(r.json())


def test_server_rate_limit_and_provider_errors_match_fixtures():
    client, _ = make_client(requests_per_minute=1)
    client.post("/v1/ask", json={"text": "x"}, headers=AUTH)
    r = client.post("/v1/ask", json={"text": "x"}, headers=AUTH)
    assert (r.status_code, r.json()) == (429, {"error": "rate_limited"})

    client, _ = make_client(provider=FakeProvider(fail=True))
    r = client.post("/v1/ask", json={"text": "x"}, headers=AUTH)
    assert (r.status_code, r.json()) == (502, {"error": "provider_error"})
