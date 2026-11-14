import pytest
from fastapi.testclient import TestClient

from app.config import Settings
from app.main import create_app
from app.prompts import Task, build_user_prompt
from app.providers import Answer, ProviderError

TOKEN = "test-device-token"
AUTH = {"Authorization": f"Bearer {TOKEN}"}


class FakeProvider:
    def __init__(self, fail: bool = False):
        self.fail = fail
        self.calls: list[tuple[str, str, int]] = []

    async def complete(self, system: str, user: str, max_tokens: int) -> Answer:
        self.calls.append((system, user, max_tokens))
        if self.fail:
            raise ProviderError("boom")
        return Answer(text="An answer.", model="fake-model", truncated=False)


def make_client(provider=None, **overrides) -> tuple[TestClient, FakeProvider]:
    provider = provider or FakeProvider()
    base = dict(provider="openai", openai_api_key="k", openai_model="m", device_tokens=(TOKEN,))
    base.update(overrides)
    return TestClient(create_app(Settings(**base), provider)), provider


def test_health_needs_no_token():
    client, _ = make_client()
    assert client.get("/healthz").json() == {"ok": True}


def test_ask_returns_answer():
    client, provider = make_client()
    r = client.post("/v1/ask", json={"task": "define", "text": "मित्रता", "language": "Hindi"}, headers=AUTH)
    assert r.status_code == 200
    assert r.json() == {"answer": "An answer.", "model": "fake-model", "truncated": False}
    _, user, max_tokens = provider.calls[0]
    assert "मित्रता" in user and "Hindi" in user
    assert max_tokens == 400


@pytest.mark.parametrize("headers", [{}, {"Authorization": "Bearer wrong"}, {"Authorization": TOKEN}])
def test_rejects_bad_token(headers):
    client, provider = make_client()
    r = client.post("/v1/ask", json={"text": "x"}, headers=headers)
    assert r.status_code == 401
    assert r.json() == {"error": "unauthorized"}
    assert provider.calls == []


def test_rejects_long_text():
    client, provider = make_client(max_input_chars=10)
    r = client.post("/v1/ask", json={"text": "x" * 11}, headers=AUTH)
    assert r.status_code == 413
    assert provider.calls == []


def test_rate_limit():
    client, _ = make_client(requests_per_minute=2)
    codes = [client.post("/v1/ask", json={"text": "x"}, headers=AUTH).status_code for _ in range(3)]
    assert codes == [200, 200, 429]


def test_provider_error_is_502():
    client, _ = make_client(provider=FakeProvider(fail=True))
    r = client.post("/v1/ask", json={"text": "x"}, headers=AUTH)
    assert r.status_code == 502
    assert r.json() == {"error": "AI provider error"}


def test_invalid_config_fails_at_start():
    with pytest.raises(RuntimeError, match="DEVICE_TOKENS"):
        create_app(Settings(provider="openai", openai_api_key="k", openai_model="m", device_tokens=()), FakeProvider())


def test_question_only_used_for_ask_task():
    explain = build_user_prompt(Task.EXPLAIN, "text", question="why?")
    ask = build_user_prompt(Task.ASK, "text", question="why?")
    assert "why?" not in explain
    assert "why?" in ask
