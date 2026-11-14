"""HTTP API used by the e-reader.

POST /v1/ask   (Authorization: Bearer <device token>)
  {"task": "explain", "text": "...", "context": "...", "question": "...",
   "language": "Hindi", "book_title": "...", "book_author": "..."}
-> {"answer": "...", "model": "...", "truncated": false}

GET /healthz  -> {"ok": true}
"""

import hashlib
import hmac
import logging
import time
from collections import defaultdict, deque

from fastapi import Depends, FastAPI, Header, HTTPException, Request
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field

from .config import Settings
from .prompts import SYSTEM_PROMPT, Task, build_user_prompt
from .providers import Provider, ProviderError, make_provider

log = logging.getLogger("ai-proxy")


class AskRequest(BaseModel):
    task: Task = Task.EXPLAIN
    text: str = Field(min_length=1)
    context: str = ""
    question: str = Field(default="", max_length=500)
    language: str = Field(default="", max_length=40)
    book_title: str = Field(default="", max_length=200)
    book_author: str = Field(default="", max_length=200)


class AskResponse(BaseModel):
    answer: str
    model: str
    truncated: bool


class RateLimiter:
    """Per-token sliding window. In-memory, so deploy with one instance (max-instances=1)."""

    def __init__(self, per_minute: int):
        self._per_minute = per_minute
        self._hits: dict[str, deque[float]] = defaultdict(deque)

    def allow(self, key: str) -> bool:
        now = time.monotonic()
        hits = self._hits[key]
        while hits and now - hits[0] > 60:
            hits.popleft()
        if len(hits) >= self._per_minute:
            return False
        hits.append(now)
        return True


def create_app(settings: Settings | None = None, provider: Provider | None = None) -> FastAPI:
    settings = settings or Settings()
    problems = settings.problems()
    if problems:
        raise RuntimeError("Invalid configuration: " + "; ".join(problems))

    provider = provider or make_provider(settings)
    limiter = RateLimiter(settings.requests_per_minute)
    app = FastAPI(title="CrossPoint AI proxy", docs_url=None, redoc_url=None, openapi_url=None)

    def device_id(authorization: str = Header(default="")) -> str:
        scheme, _, token = authorization.partition(" ")
        if scheme.lower() == "bearer" and token:
            for known in settings.device_tokens:
                if hmac.compare_digest(token.encode(), known.encode()):
                    # Short, non-reversible id for logs and rate limiting.
                    return hashlib.sha256(token.encode()).hexdigest()[:12]
        raise HTTPException(status_code=401, detail="unauthorized")

    @app.get("/healthz")
    async def healthz() -> dict:
        return {"ok": True}

    @app.post("/v1/ask", response_model=AskResponse)
    async def ask(req: AskRequest, device: str = Depends(device_id)) -> AskResponse:
        if not limiter.allow(device):
            raise HTTPException(status_code=429, detail="too many requests")
        if len(req.text) > settings.max_input_chars:
            raise HTTPException(status_code=413, detail=f"text longer than {settings.max_input_chars} characters")
        if len(req.context) > settings.max_context_chars:
            raise HTTPException(status_code=413, detail=f"context longer than {settings.max_context_chars} characters")

        user_prompt = build_user_prompt(
            req.task,
            req.text,
            context=req.context,
            question=req.question,
            language=req.language,
            book_title=req.book_title,
            book_author=req.book_author,
        )
        started = time.monotonic()
        try:
            answer = await provider.complete(SYSTEM_PROMPT, user_prompt, settings.max_output_tokens)
        except ProviderError as e:
            log.warning("device=%s task=%s provider error: %s", device, req.task.value, e)
            raise HTTPException(status_code=502, detail="AI provider error") from e

        # Never log book text or answers: only sizes and timing.
        log.info(
            "device=%s task=%s in=%d out=%d ms=%d",
            device,
            req.task.value,
            len(user_prompt),
            len(answer.text),
            int((time.monotonic() - started) * 1000),
        )
        return AskResponse(answer=answer.text, model=answer.model, truncated=answer.truncated)

    @app.exception_handler(HTTPException)
    async def http_error(_: Request, exc: HTTPException) -> JSONResponse:
        # Flat {"error": "..."} body: easy to parse on the device.
        return JSONResponse(status_code=exc.status_code, content={"error": exc.detail})

    return app
