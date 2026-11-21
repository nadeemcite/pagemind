"""Settings, read once from environment variables (Cloud Run env vars and secrets)."""

import os
from dataclasses import dataclass, field


def _int(name: str, default: int) -> int:
    raw = os.environ.get(name, "").strip()
    return int(raw) if raw else default


@dataclass(frozen=True)
class Settings:
    # Which provider answers: "openai" or "ollama".
    provider: str = field(default_factory=lambda: os.environ.get("PROVIDER", "openai").strip().lower())

    # OpenAI
    openai_api_key: str = field(default_factory=lambda: os.environ.get("OPENAI_API_KEY", "").strip())
    openai_model: str = field(default_factory=lambda: os.environ.get("OPENAI_MODEL", "").strip())
    # Optional: any OpenAI-compatible endpoint (e.g. a self-hosted server).
    openai_base_url: str = field(default_factory=lambda: os.environ.get("OPENAI_BASE_URL", "").strip())

    # Ollama: https://ollama.com (cloud, needs OLLAMA_API_KEY) or your own server URL.
    ollama_host: str = field(default_factory=lambda: os.environ.get("OLLAMA_HOST", "https://ollama.com").strip())
    ollama_api_key: str = field(default_factory=lambda: os.environ.get("OLLAMA_API_KEY", "").strip())
    ollama_model: str = field(default_factory=lambda: os.environ.get("OLLAMA_MODEL", "").strip())
    # Thinking models spend the output budget on hidden reasoning; short answers do not need it.
    ollama_think: bool = field(
        default_factory=lambda: os.environ.get("OLLAMA_THINK", "false").strip().lower() in ("1", "true", "yes")
    )

    # Comma-separated tokens; each device sends one as "Authorization: Bearer <token>".
    device_tokens: tuple[str, ...] = field(
        default_factory=lambda: tuple(
            t.strip() for t in os.environ.get("DEVICE_TOKENS", "").split(",") if t.strip()
        )
    )

    # Limits that keep requests cheap and answers small enough for the e-reader.
    max_input_chars: int = field(default_factory=lambda: _int("MAX_INPUT_CHARS", 4000))
    max_context_chars: int = field(default_factory=lambda: _int("MAX_CONTEXT_CHARS", 2000))
    max_output_tokens: int = field(default_factory=lambda: _int("MAX_OUTPUT_TOKENS", 400))
    # Summarize-so-far: each text window, and a larger answer budget (router models reason first).
    max_window_chars: int = field(default_factory=lambda: _int("MAX_WINDOW_CHARS", 3000))
    summarize_max_output_tokens: int = field(default_factory=lambda: _int("SUMMARIZE_MAX_OUTPUT_TOKENS", 900))
    requests_per_minute: int = field(default_factory=lambda: _int("REQUESTS_PER_MINUTE", 10))
    # Raw request body cap, checked before JSON parsing (UTF-8 and JSON escapes make bytes > chars).
    max_body_bytes: int = field(default_factory=lambda: _int("MAX_BODY_BYTES", 32768))
    provider_timeout_s: int = field(default_factory=lambda: _int("PROVIDER_TIMEOUT_S", 30))

    def problems(self) -> list[str]:
        """Configuration errors that make the service unusable."""
        errors = []
        if not self.device_tokens:
            errors.append("DEVICE_TOKENS is empty")
        if self.provider == "openai":
            if not self.openai_api_key and not self.openai_base_url:
                errors.append("OPENAI_API_KEY is empty")
            if not self.openai_model:
                errors.append("OPENAI_MODEL is empty")
        elif self.provider == "ollama":
            if not self.ollama_model:
                errors.append("OLLAMA_MODEL is empty")
        else:
            errors.append(f"PROVIDER must be 'openai' or 'ollama', got '{self.provider}'")
        return errors
