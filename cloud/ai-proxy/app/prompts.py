"""Prompts live on the server so they can change without reflashing the device."""

from enum import Enum


class Task(str, Enum):
    EXPLAIN = "explain"      # explain the selected passage
    DEFINE = "define"        # meaning of a word/phrase in this context
    TRANSLATE = "translate"  # translate the selection into the reader's language
    ASK = "ask"              # free question about the selection
    SUMMARIZE = "summarize"  # spoiler-free recap of a well-known book up to the reading position


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


# --- Summarize so far -------------------------------------------------------
# Unlike the other tasks this one uses the model's own knowledge of the book:
# the device sends only two short windows around the reading position.

UNKNOWN_BOOK_MARKER = "UNKNOWN_BOOK"
UNKNOWN_BOOK_MESSAGE = "This book is not well known enough for a reliable summary."
BOUNDARY = "=== READING POSITION: the reader has read everything above this line and nothing below it ==="

SUMMARIZE_SYSTEM_PROMPT = (
    "You help a reader of a well-known book on a small e-ink e-reader. "
    "You are given the book title, author, the chapter, and two short excerpts: the text just before "
    "the reader's current position and the text just after it. "
    "Use your own knowledge of the book to recap the story up to the reading position. "
    "The reading position is the last word of the text before it. Treat it as an exact boundary. "
    "Never reveal anything the book tells the reader after that word, even later in the same chapter: "
    "no later events, names, numbers, relationships, instructions, motives or outcomes. "
    "The text after the position is unread: use it only to work out where in the book the reader is, "
    "never as part of the recap, and do not hint at what comes next. "
    "Only include a fact if the reader has already met it, either in the text before the position or "
    "in earlier chapters. If you are not sure the reader has met a fact yet, leave it out. "
    "A short recap that stops early is better than one that goes a line too far. "
    "Then add one or two sentences of background that help the reader understand the book so far, "
    "limited to the setting, the narrator, and the characters the reader has already met. "
    "Before you answer, check every sentence against that boundary and delete any that goes past it. "
    f"If you do not know this book well enough to describe its plot, reply with exactly {UNKNOWN_BOOK_MARKER} "
    "and nothing else. Do not invent plot or characters. "
    "Answer in plain text only: no Markdown, no bullet symbols, no headings, no emoji. "
    "Use short paragraphs and at most about 180 words."
)


def build_summarize_prompt(
    *,
    book_title: str,
    text_before: str,
    text_after: str = "",
    book_author: str = "",
    chapter: str = "",
    language: str = "",
) -> str:
    parts = ["Summarize the story so far, up to the reading position, without spoilers."]
    if language:
        parts.append(f"Write the answer in this language: {language}.")
    parts.append(f'Book: "{book_title}"' + (f" by {book_author}" if book_author else "") if book_title else "Book: unknown title")
    if chapter:
        parts.append(f"Chapter: {chapter}")
    after = f"\n\nText after the reading position (unread, for locating only):\n<<<\n{text_after}\n>>>" if text_after else ""
    parts.append(f"Text before the reading position:\n<<<\n{text_before}\n>>>\n\n{BOUNDARY}{after}")
    return "\n\n".join(parts)
