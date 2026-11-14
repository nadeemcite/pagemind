"""HTTP API used by the e-reader (contract v1, see contract/README.md).

GET  /health                              -> {"ok": true}
GET  /v1/info   (Authorization: Bearer)    -> server limits; doubles as a key check
POST /v1/ask    (Authorization: Bearer)    -> {"answer": "...", "model": "...", "truncated": false}

Errors are always {"error": "<code>"} with an HTTP status.
"""

import hashlib
import hmac
import logging
import time
from collections import defaultdict, deque

from fastapi import Depends, FastAPI, Header, HTTPException, Request
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse
from pydantic import BaseModel, Field

from .config import Settings
from .logs import event, setup_logging
from .prompts import SYSTEM_PROMPT, Task, build_user_prompt
from .providers import Provider, ProviderError, make_provider

API_VERSION = 1


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


class InfoResponse(BaseModel):
    api: int
    tasks: list[str]
    max_input_chars: int
    max_context_chars: int
    max_question_chars: int
    provider: str
    model: str


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


class BodySizeLimit:
    """ASGI middleware: reject request bodies over `max_bytes`; buffers at most `max_bytes`."""

    def __init__(self, app, max_bytes: int):
        self.app = app
        self.max_bytes = max_bytes

    async def __call__(self, scope, receive, send):
        if scope["type"] != "http":
            return await self.app(scope, receive, send)

        for name, value in scope.get("headers", []):
            if name == b"content-length":
                try:
                    too_big = int(value) > self.max_bytes
                except ValueError:
                    too_big = True
                if too_big:
                    return await _send_error(send, 413, "body_too_large")

        # Read the whole body here (it is small) so an oversized one never reaches FastAPI,
        # which would turn a mid-read failure into a generic 400.
        chunks, received = [], 0
        while True:
            message = await receive()
            if message["type"] != "http.request":
                return await self.app(scope, _replay([message]), send)
            body = message.get("body", b"")
            received += len(body)
            if received > self.max_bytes:
                return await _send_error(send, 413, "body_too_large")
            chunks.append(body)
            if not message.get("more_body", False):
                break

        await self.app(scope, _replay([{"type": "http.request", "body": b"".join(chunks), "more_body": False}]), send)


def _replay(messages):
    pending = list(messages)

    async def receive():
        if pending:
            return pending.pop(0)
        return {"type": "http.disconnect"}

    return receive


async def _send_error(send, status: int, code: str) -> None:
    body = ('{"error":"%s"}' % code).encode()
    await send({"type": "http.response.start", "status": status,
                "headers": [(b"content-type", b"application/json"), (b"content-length", str(len(body)).encode())]})
    await send({"type": "http.response.body", "body": body})


def create_app(settings: Settings | None = None, provider: Provider | None = None) -> FastAPI:
    setup_logging()
    settings = settings or Settings()
    problems = settings.problems()
    if problems:
        raise RuntimeError("Invalid configuration: " + "; ".join(problems))

    provider = provider or make_provider(settings)
    model_name = settings.ollama_model if settings.provider == "ollama" else settings.openai_model
    limiter = RateLimiter(settings.requests_per_minute)
    app = FastAPI(title="AI proxy", docs_url=None, redoc_url=None, openapi_url=None)
    app.add_middleware(BodySizeLimit, max_bytes=settings.max_body_bytes)

    def device_id(authorization: str = Header(default="")) -> str:
        scheme, _, token = authorization.partition(" ")
        if scheme.lower() == "bearer" and token:
            for known in settings.device_tokens:
                if hmac.compare_digest(token.encode(), known.encode()):
                    # Short, non-reversible id for logs and rate limiting.
                    return hashlib.sha256(token.encode()).hexdigest()[:12]
        raise HTTPException(status_code=401, detail="unauthorized")

    @app.get("/health")
    async def health() -> dict:
        return {"ok": True}

    @app.get("/v1/info", response_model=InfoResponse)
    async def info(_: str = Depends(device_id)) -> InfoResponse:
        return InfoResponse(
            api=API_VERSION,
            tasks=[t.value for t in Task],
            max_input_chars=settings.max_input_chars,
            max_context_chars=settings.max_context_chars,
            max_question_chars=500,
            provider=settings.provider,
            model=model_name,
        )

    @app.post("/v1/ask", response_model=AskResponse)
    async def ask(req: AskRequest, device: str = Depends(device_id)) -> AskResponse:
        if not limiter.allow(device):
            raise HTTPException(status_code=429, detail="rate_limited")
        if len(req.text) > settings.max_input_chars:
            raise HTTPException(status_code=413, detail="text_too_long")
        if len(req.context) > settings.max_context_chars:
            raise HTTPException(status_code=413, detail="context_too_long")

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
            event(logging.WARNING, "provider_error", device=device, task=req.task.value, error=str(e))
            raise HTTPException(status_code=502, detail="provider_error") from e

        event(
            logging.INFO,
            "ask",
            device=device,
            task=req.task.value,
            in_chars=len(user_prompt),
            out_chars=len(answer.text),
            truncated=answer.truncated,
            ms=int((time.monotonic() - started) * 1000),
        )
        return AskResponse(answer=answer.text, model=answer.model, truncated=answer.truncated)

    @app.exception_handler(HTTPException)
    async def http_error(_: Request, exc: HTTPException) -> JSONResponse:
        return JSONResponse(status_code=exc.status_code, content={"error": exc.detail})

    @app.exception_handler(RequestValidationError)
    async def validation_error(_: Request, exc: RequestValidationError) -> JSONResponse:
        # Same flat shape as every other error, so the device parses one format.
        return JSONResponse(status_code=422, content={"error": "invalid_request"})

    return app
