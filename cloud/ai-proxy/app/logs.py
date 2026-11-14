"""JSON log lines on stderr, which Cloud Logging parses into structured entries."""

import json
import logging
import sys

log = logging.getLogger("ai-proxy")


class JsonFormatter(logging.Formatter):
    def format(self, record: logging.LogRecord) -> str:
        entry = {"severity": record.levelname, "message": record.getMessage()}
        entry.update(getattr(record, "fields", {}))
        return json.dumps(entry, ensure_ascii=False)


class _StderrHandler(logging.StreamHandler):
    """Writes to whatever sys.stderr is now (test capture and servers may replace it)."""

    def emit(self, record: logging.LogRecord) -> None:
        self.stream = sys.stderr
        super().emit(record)


def setup_logging() -> None:
    if log.handlers:
        return
    handler = _StderrHandler()
    handler.setFormatter(JsonFormatter())
    log.addHandler(handler)
    log.setLevel(logging.INFO)
    log.propagate = False


def event(level: int, message: str, **fields) -> None:
    """Log one event. Callers pass sizes and ids only, never book text or answers."""
    log.log(level, message, extra={"fields": fields})
