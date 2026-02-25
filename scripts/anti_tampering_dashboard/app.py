#!/usr/bin/env python3
"""
Mini Anti-Tampering warning dashboard (stdlib-only).

- Tails a log file
- Filters warning lines for [ANTI_TAMPERING_*]
- Streams parsed events to browser via Server-Sent Events (SSE)

Usage:
  python3 app.py --log /home/jnuno/Modular-IO-Lib/logs/warn.log
  python3 app.py --log /home/jnuno/Modular-IO-Lib/logfile --port 8008
"""

from __future__ import annotations

import argparse
import json
import os
import queue
import re
import threading
import time
from dataclasses import asdict, dataclass
from http import HTTPStatus
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import Optional


ANTI_TAMPERING_WARN_RE = re.compile(
    r"""
    ^\[(?P<ts>[^\]]+)\]\s*-\s*
    \[(?P<tag>ANTI_TAMPERING_[A-Z_]+)\]\s*
    (?P<msg>.*)$
    """,
    re.VERBOSE,
)

# Special-case parser for the common mismatch warning:
# "... Hash mismatch for file <path> (size=..., verify_fd=...); Stored hash: ...; Computed hash: ..."
MISMATCH_RE = re.compile(
    r"""
    Hash\ mismatch\ for\ file\ (?P<path>.+?)\s*
    \(size=(?P<size>\d+),\s*verify_fd=(?P<verify_fd>-?\d+)\);\s*
    Stored\ hash:\s*(?P<stored>[0-9a-fA-F]+);\s*
    Computed\ hash:\s*(?P<computed>[0-9a-fA-F]+)
    """,
    re.VERBOSE,
)


@dataclass(frozen=True)
class AntiTamperingEvent:
    ts: str
    tag: str
    severity: str  # "warning" | "danger"
    message: str
    path: Optional[str] = None
    hash_path: Optional[str] = None
    size: Optional[int] = None
    verify_fd: Optional[int] = None
    stored: Optional[str] = None
    computed: Optional[str] = None
    raw: Optional[str] = None


class EventBus:
    def __init__(self) -> None:
        self._subscribers: set["queue.Queue[str]"] = set()
        self._lock = threading.Lock()

    def subscribe(self) -> "queue.Queue[str]":
        q: "queue.Queue[str]" = queue.Queue(maxsize=500)
        with self._lock:
            self._subscribers.add(q)
        return q

    def unsubscribe(self, q: "queue.Queue[str]") -> None:
        with self._lock:
            self._subscribers.discard(q)

    def publish(self, payload: str) -> None:
        with self._lock:
            subs = list(self._subscribers)
        for q in subs:
            try:
                q.put_nowait(payload)
            except queue.Full:
                # Drop oldest by clearing a bit, then try again.
                try:
                    _ = q.get_nowait()
                    q.put_nowait(payload)
                except Exception:
                    pass


def parse_line(line: str) -> Optional[AntiTamperingEvent]:
    m = ANTI_TAMPERING_WARN_RE.match(line.strip())
    if not m:
        return None

    ts = m.group("ts")
    tag = m.group("tag")
    msg = m.group("msg").strip()

    # Default: show as yellow warning
    severity = "warning"

    mm = MISMATCH_RE.search(msg)
    if mm:
        severity = "danger"
        return AntiTamperingEvent(
            ts=ts,
            tag=tag,
            severity=severity,
            message=msg,
            path=mm.group("path"),
            size=int(mm.group("size")),
            verify_fd=int(mm.group("verify_fd")),
            stored=mm.group("stored"),
            computed=mm.group("computed"),
            raw=line.rstrip("\n"),
        )

    # Parse: "Hash file <hash_path> does not exist for file <path> ..."
    if "Hash file " in msg and " does not exist for file " in msg:
        try:
            # Keep it simple and robust: split once
            part = msg.split("Hash file ", 1)[1]
            hash_path, rest = part.split(" does not exist for file ", 1)
            file_path = rest.split(".", 1)[0].strip()
            return AntiTamperingEvent(
                ts=ts,
                tag=tag,
                severity=severity,
                message=msg,
                path=file_path,
                hash_path=hash_path.strip(),
                raw=line.rstrip("\n"),
            )
        except Exception:
            pass

    # Parse: "Failed to open verification fd for file <path>"
    if "Failed to open verification fd for file " in msg:
        try:
            file_path = msg.split("Failed to open verification fd for file ", 1)[1].strip()
            return AntiTamperingEvent(
                ts=ts,
                tag=tag,
                severity=severity,
                message=msg,
                path=file_path,
                raw=line.rstrip("\n"),
            )
        except Exception:
            pass

    return AntiTamperingEvent(
        ts=ts, tag=tag, severity=severity, message=msg, raw=line.rstrip("\n")
    )


def tail_file(path: Path, bus: EventBus, stop_evt: threading.Event) -> None:
    """
    Tail a file, handling truncation/rotation in a simple way:
    - If file shrinks, seek back to 0
    - If file disappears, keep retrying
    """
    pos = 0
    last_inode = None

    while not stop_evt.is_set():
        try:
            st = path.stat()
            inode = (st.st_dev, st.st_ino)
            if last_inode != inode:
                # Rotation or new file: reset
                last_inode = inode
                pos = 0

            # If truncated, reset
            if st.st_size < pos:
                pos = 0

            with path.open("r", encoding="utf-8", errors="replace") as f:
                f.seek(pos)
                while not stop_evt.is_set():
                    line = f.readline()
                    if not line:
                        pos = f.tell()
                        break
                    ev = parse_line(line)
                    if ev:
                        bus.publish(json.dumps(asdict(ev), ensure_ascii=False))
        except FileNotFoundError:
            last_inode = None
            pos = 0
        except Exception:
            # Don't crash the tailer on unexpected parse/IO errors
            pass

        time.sleep(0.25)


class Handler(BaseHTTPRequestHandler):
    server_version = "AntiTamperingDashboard/0.1"

    def _send(self, code: int, content_type: str, body: bytes) -> None:
        self.send_response(code)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(body)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self) -> None:  # noqa: N802
        if self.path == "/" or self.path.startswith("/index.html"):
            html = (Path(__file__).parent / "index.html").read_bytes()
            self._send(HTTPStatus.OK, "text/html; charset=utf-8", html)
            return

        if self.path.startswith("/events"):
            self.send_response(HTTPStatus.OK)
            self.send_header("Content-Type", "text/event-stream; charset=utf-8")
            self.send_header("Cache-Control", "no-store")
            self.send_header("Connection", "keep-alive")
            self.end_headers()

            q = self.server.bus.subscribe()  # type: ignore[attr-defined]
            try:
                # Initial comment to open the stream quickly
                self.wfile.write(b": ok\n\n")
                self.wfile.flush()

                while True:
                    try:
                        payload = q.get(timeout=15)
                        data = f"event: warn\ndata: {payload}\n\n".encode("utf-8")
                        self.wfile.write(data)
                        self.wfile.flush()
                    except queue.Empty:
                        # keep-alive ping
                        self.wfile.write(b": ping\n\n")
                        self.wfile.flush()
            except (BrokenPipeError, ConnectionResetError):
                pass
            finally:
                self.server.bus.unsubscribe(q)  # type: ignore[attr-defined]
            return

        if self.path.startswith("/health"):
            self._send(HTTPStatus.OK, "application/json", b'{"ok":true}')
            return

        self._send(HTTPStatus.NOT_FOUND, "text/plain; charset=utf-8", b"Not found")

    def log_message(self, fmt: str, *args) -> None:
        # Quiet by default (this tool is meant to watch *your* logs)
        return


def main() -> int:
    default_log = (
        Path(__file__).resolve().parents[2] / "logs" / "warn.log"
    )  # repo_root/logs/warn.log

    p = argparse.ArgumentParser()
    p.add_argument("--log", default=str(default_log), help="Path to log file to tail")
    p.add_argument("--host", default="127.0.0.1", help="Bind host")
    p.add_argument("--port", type=int, default=8008, help="Bind port")
    args = p.parse_args()

    log_path = Path(args.log).expanduser().resolve()
    if not log_path.exists():
        print(f"[warn] log file does not exist yet: {log_path}")
        print("       (the dashboard will keep retrying)")

    bus = EventBus()
    stop_evt = threading.Event()
    t = threading.Thread(target=tail_file, args=(log_path, bus, stop_evt), daemon=True)
    t.start()

    httpd = ThreadingHTTPServer((args.host, args.port), Handler)
    httpd.bus = bus  # type: ignore[attr-defined]

    print(f"Anti-tampering dashboard: http://{args.host}:{args.port}")
    print(f"Tailing: {log_path}")

    try:
        httpd.serve_forever(poll_interval=0.5)
    except KeyboardInterrupt:
        pass
    finally:
        stop_evt.set()
        httpd.server_close()

    return 0


if __name__ == "__main__":
    raise SystemExit(main())

