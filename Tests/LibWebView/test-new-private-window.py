#!/usr/bin/env python3
#
# Copyright (c) 2026-present, the Ladybird developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import http.server
import json
import os
import queue
import runpy
import subprocess
import tempfile
import threading

from pathlib import Path

stop_browser = runpy.run_path(str(Path(__file__).with_name("test-devtools-client-follows-the-tab.py")))["stop"]

COOKIE = "private_session=1"
requests = queue.Queue()


class Handler(http.server.BaseHTTPRequestHandler):
    def do_GET(self):
        cookie = self.headers.get("Cookie")
        if self.path == "/cold-launch":
            self.send_response(302)
            self.send_header("Set-Cookie", f"{COOKIE}; Path=/")
            self.send_header("Location", "/cold-ready")
        elif self.path == "/saved-session":
            self.send_response(200)
            self.send_header("Content-Type", "text/html")
        elif self.path in ("/saved-session-ready", "/cold-ready", "/new-tab"):
            self.send_response(204)
        else:
            self.send_error(404)
            return
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        if self.path == "/saved-session":
            self.wfile.write(b'<img src="/saved-session-ready">')
        elif self.path != "/cold-launch":
            # The redirected request confirms the cookie was applied before the next launch.
            requests.put((self.path, cookie))

    def log_message(self, format, *args):
        pass


def expect_requests(*expected):
    actual = {requests.get(timeout=15) for _ in expected}
    assert actual == set(expected), (actual, expected)


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("ladybird_binary")
    binary = parser.parse_args().ladybird_binary
    with (
        tempfile.TemporaryDirectory() as directory,
        http.server.ThreadingHTTPServer(("127.0.0.1", 0), Handler) as server,
    ):
        profile = Path(directory) / "profile"
        config = profile / "config"
        config.mkdir(parents=True)
        base_url = f"http://127.0.0.1:{server.server_port}"
        settings = {
            "firstRunComplete": True,
            "restorePreviousSession": True,
            "backgroundNetworking": {"enabled": False},
            "newTabPageURL": f"{base_url}/new-tab",
        }
        (config / "Settings.json").write_text(json.dumps(settings))
        session_db = profile / "data" / "Sessions.db"
        threading.Thread(target=server.serve_forever, daemon=True).start()
        os.environ["QT_QPA_PLATFORM"] = "offscreen"
        regular_command = [binary, f"--profile-path={profile}"]
        seed = subprocess.Popen(regular_command + [f"{base_url}/saved-session"])
        try:
            expect_requests(("/saved-session-ready", None))
        finally:
            stop_browser(seed)
        assert seed.returncode == 0, "Could not save the regular session"

        saved_session_db = session_db.read_bytes()
        command = regular_command + ["--new-private-window"]
        private_only = subprocess.Popen(command)
        try:
            expect_requests(("/new-tab", None))
        finally:
            stop_browser(private_only)
        assert private_only.returncode == 0, "Private-only launch did not quit cleanly on SIGTERM"

        browser = subprocess.Popen(command + [f"{base_url}/cold-launch"])
        try:
            expect_requests(("/cold-ready", COOKIE))

            subprocess.run(command, check=True, timeout=15)
            expect_requests(("/new-tab", COOKIE))

            assert session_db.read_bytes() == saved_session_db, "Private launch changed the regular session"
            assert not session_db.with_suffix(".db-wal").exists(), "Private launch opened the regular session store"

            subprocess.run(regular_command + ["--new-window"], check=True, timeout=15)
            expect_requests(("/saved-session-ready", None), ("/new-tab", None))
        finally:
            stop_browser(browser)
            server.shutdown()

        headless = subprocess.run(
            [
                binary,
                f"--profile-path={Path(directory) / 'headless'}",
                "--headless=text",
                "--new-private-window",
                "data:text/html,<p>Private launch flag is ignored in headless mode</p>",
            ],
            check=True,
            capture_output=True,
            text=True,
            timeout=15,
        )
        assert "Private launch flag is ignored in headless mode" in headless.stdout
        print("PASS: Private launches shut down cleanly, isolate cookies, and defer regular session restoration")


if __name__ == "__main__":
    main()
