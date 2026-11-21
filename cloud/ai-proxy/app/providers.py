"""AI providers. Each one turns (system, user) prompts into a plain-text answer."""

from dataclasses import dataclass
from typing import Protocol

from .config import Settings


class ProviderError(Exception):
    """The provider failed or returned nothing usable."""


@dataclass
class Answer:
    text: str
    model: str
    truncated: bool  # the answer hit the output-token limit


class Provider(Protocol):
    async def complete(self, system: str, user: str, max_tokens: int) -> Answer: ...


class OpenAIProvider:
    def __init__(self, settings: Settings):
        from openai import AsyncOpenAI

        self._model = settings.openai_model
        self._client = AsyncOpenAI(
            api_key=settings.openai_api_key or "unused",
            base_url=settings.openai_base_url or None,
            timeout=settings.provider_timeout_s,
            max_retries=1,
        )

    async def complete(self, system: str, user: str, max_tokens: int) -> Answer:
        import openai

        try:
            resp = await self._client.chat.completions.create(
                model=self._model,
                messages=[
                    {"role": "system", "content": system},
                    {"role": "user", "content": user},
                ],
                max_completion_tokens=max_tokens,
                # Some OpenAI-compatible routers stream by default; the proxy needs one JSON body.
                stream=False,
            )
        except openai.APIError as e:
            raise ProviderError(f"OpenAI error: {e.__class__.__name__}") from e

        if not resp.choices or not resp.choices[0].message.content:
            raise ProviderError("OpenAI returned an empty answer")
        choice = resp.choices[0]
        return Answer(
            text=choice.message.content.strip(),
            model=resp.model,
            truncated=choice.finish_reason == "length",
        )


class OllamaProvider:
    def __init__(self, settings: Settings):
        from ollama import AsyncClient

        headers = {"Authorization": f"Bearer {settings.ollama_api_key}"} if settings.ollama_api_key else None
        self._model = settings.ollama_model
        self._think = settings.ollama_think
        self._client = AsyncClient(host=settings.ollama_host, headers=headers, timeout=settings.provider_timeout_s)

    async def complete(self, system: str, user: str, max_tokens: int) -> Answer:
        import httpx
        import ollama

        try:
            resp = await self._client.chat(
                model=self._model,
                messages=[
                    {"role": "system", "content": system},
                    {"role": "user", "content": user},
                ],
                options={"num_predict": max_tokens},
                think=self._think,
            )
        except (ollama.ResponseError, httpx.HTTPError) as e:
            raise ProviderError(f"Ollama error: {e.__class__.__name__}") from e

        content = (resp.message.content or "").strip() if resp.message else ""
        if not content:
            raise ProviderError("Ollama returned an empty answer")
        return Answer(text=content, model=resp.model or self._model, truncated=resp.done_reason == "length")


def make_provider(settings: Settings) -> Provider:
    if settings.provider == "ollama":
        return OllamaProvider(settings)
    return OpenAIProvider(settings)
