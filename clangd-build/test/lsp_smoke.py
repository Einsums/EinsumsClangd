#!/usr/bin/env python3
"""LSP smoke runner for the custom clangd build.

Drives clangd over stdin/stdout, opens a single C++ file, waits for the
``textDocument/publishDiagnostics`` notification(s), and asserts that at
least one diagnostic's message contains a given substring. Exits 0 on
match, non-zero otherwise.

Run from ctest as::

    lsp_smoke.py --clangd <path-to-clangd> \\
                 --file   <abs-path-to-source.cpp> \\
                 --expect 'redundant permute' \\
                 --timeout 30

We don't depend on a third-party LSP client (none ship with the conda env),
so this is a hand-rolled JSON-RPC dance over Content-Length-framed
messages. Enough to drive ``initialize``, ``didOpen``, and read back
``publishDiagnostics`` — that's the entire surface we need.
"""
from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import threading
import time


def _frame(obj: dict) -> bytes:
    body = json.dumps(obj).encode("utf-8")
    return f"Content-Length: {len(body)}\r\n\r\n".encode("ascii") + body


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--clangd", required=True, help="Path to the clangd binary to test")
    p.add_argument("--file", required=True, help="Source file to open in clangd")
    p.add_argument("--expect", required=True, help="Substring expected in at least one diagnostic message")
    p.add_argument("--timeout", type=float, default=30.0, help="Seconds to wait for diagnostics")
    args = p.parse_args()

    file_path = os.path.abspath(args.file)
    if not os.path.exists(file_path):
        print(f"FAIL: source file not found: {file_path}", file=sys.stderr)
        return 2
    if not os.access(args.clangd, os.X_OK):
        print(f"FAIL: clangd not executable: {args.clangd}", file=sys.stderr)
        return 2

    with open(file_path, encoding="utf-8") as f:
        text = f.read()
    uri = "file://" + file_path

    proc = subprocess.Popen(
        [args.clangd, "--enable-config", "--log=error"],
        stdin=subprocess.PIPE,
        stdout=subprocess.PIPE,
        stderr=subprocess.PIPE,
    )

    matched: list[str] = []
    all_messages: list[str] = []
    done = threading.Event()

    def reader() -> None:
        # Parse Content-Length framed JSON-RPC messages from clangd's stdout.
        while not done.is_set():
            line = proc.stdout.readline()
            if not line:
                return
            if not line.startswith(b"Content-Length:"):
                continue
            length = int(line.split(b":", 1)[1].strip())
            proc.stdout.readline()  # consume blank separator line
            body = proc.stdout.read(length)
            try:
                m = json.loads(body)
            except json.JSONDecodeError:
                continue
            if m.get("method") != "textDocument/publishDiagnostics":
                continue
            for d in m["params"].get("diagnostics", []):
                msg = d.get("message", "")
                all_messages.append(msg)
                if args.expect in msg:
                    matched.append(msg)
                    done.set()
                    return

    t = threading.Thread(target=reader, daemon=True)
    t.start()

    # Standard initialize → initialized → didOpen handshake.
    proc.stdin.write(_frame({
        "jsonrpc": "2.0", "id": 1, "method": "initialize",
        "params": {
            "processId": os.getpid(),
            "rootUri": "file://" + os.getcwd(),
            "capabilities": {},
        },
    }))
    proc.stdin.write(_frame({"jsonrpc": "2.0", "method": "initialized", "params": {}}))
    proc.stdin.write(_frame({
        "jsonrpc": "2.0", "method": "textDocument/didOpen",
        "params": {"textDocument": {"uri": uri, "languageId": "cpp", "version": 1, "text": text}},
    }))
    proc.stdin.flush()

    # Wait for the reader to catch a matching diagnostic, or time out.
    done.wait(timeout=args.timeout)

    # Polite shutdown so clangd doesn't dump background-index state on kill.
    try:
        proc.stdin.write(_frame({"jsonrpc": "2.0", "id": 2, "method": "shutdown", "params": None}))
        proc.stdin.write(_frame({"jsonrpc": "2.0", "method": "exit"}))
        proc.stdin.flush()
        proc.wait(timeout=5)
    except Exception:
        proc.kill()

    if matched:
        print(f"OK: matched diagnostic — {matched[0][:200]}")
        return 0

    print(f"FAIL: no diagnostic containing '{args.expect}' (saw {len(all_messages)} diagnostic(s))",
          file=sys.stderr)
    for msg in all_messages[:20]:
        print(f"  - {msg[:200]}", file=sys.stderr)
    err_tail = proc.stderr.read().decode("utf-8", errors="replace")[-2000:]
    if err_tail:
        print("--- clangd stderr (tail) ---", file=sys.stderr)
        print(err_tail, file=sys.stderr)
    return 1


if __name__ == "__main__":
    sys.exit(main())
