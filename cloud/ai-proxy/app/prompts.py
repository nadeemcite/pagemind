"""Prompts live on the server so they can change without reflashing the device."""

from enum import Enum


class Task(str, Enum):
    EXPLAIN = "explain"      # explain the selected passage
    DEFINE = "define"        # meaning of a word/phrase in this context
    TRANSLATE = "translate"  # translate the selection into the reader's language
    ASK = "ask"              # free question about the selection


# The e-reader shows plain text on a small e-ink screen.
SYSTEM_PROMPT = (
    "You are a reading assistant on a small e-ink e-reader. "
    "Answer in plain text only: no Markdown, no bullet symbols, no headings, no emoji. "
    "Be concise: at most about 120 words unless the user asks for more. "
    "Only use the text you are given. Never reveal plot events beyond the given text."
)

_TASK_INSTRUCTIONS = {
    Task.EXPLAIN: "Explain the selected passage in simple words.",
    Task.DEFINE: "Give the meaning of the selected word or phrase as used in this context.",
    Task.TRANSLATE: "Translate the selected text. Keep the meaning and tone.",
    Task.ASK: "Answer the reader's question about the selected text.",
}


def build_user_prompt(
    task: Task,
    text: str,
    *,
    context: str = "",
    question: str = "",
    language: str = "",
    book_title: str = "",
    book_author: str = "",
) -> str:
    parts = [_TASK_INSTRUCTIONS[task]]
    if language:
        parts.append(f"Write the answer in this language: {language}.")
    if book_title:
        book = f'Book: "{book_title}"' + (f" by {book_author}" if book_author else "")
        parts.append(book)
    if context:
        parts.append(f"Surrounding text:\n<<<\n{context}\n>>>")
    parts.append(f"Selected text:\n<<<\n{text}\n>>>")
    if task is Task.ASK and question:
        parts.append(f"Reader's question: {question}")
    return "\n\n".join(parts)
