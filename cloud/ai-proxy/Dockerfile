FROM python:3.13-slim

COPY --from=ghcr.io/astral-sh/uv:0.9 /uv /usr/local/bin/uv

WORKDIR /srv
ENV UV_COMPILE_BYTECODE=1 UV_LINK_MODE=copy PYTHONUNBUFFERED=1

# Dependencies first, so code changes reuse this layer.
COPY pyproject.toml uv.lock ./
RUN uv sync --frozen --no-dev --no-install-project

COPY app ./app

# Cloud Run sets $PORT (8080 by default).
CMD ["sh", "-c", "exec /srv/.venv/bin/uvicorn app.main:create_app --factory --host 0.0.0.0 --port ${PORT:-8080} --no-access-log"]
