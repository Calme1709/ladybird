#!/usr/bin/env python3
#
# Copyright (c) 2026-present, the Ladybird developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import importlib
import os
import tempfile

from pathlib import Path

webdriver = importlib.import_module("test-webdriver-session-history")
helpers = importlib.import_module("test-webdriver-delete-session")


def run_test(webdriver_binary):
    with tempfile.TemporaryDirectory() as directory:
        page = Path(directory) / "visited.html"
        page.write_text("<title>Visited page</title>")
        process, port = helpers.launch_webdriver(webdriver_binary, env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
        try:
            session = webdriver.request(
                port, "POST", "/session", {"capabilities": {"alwaysMatch": {"ladybird:headless": False}}}
            )["value"]["sessionId"]
            path = f"/session/{session}"
            webdriver.request(port, "POST", f"{path}/timeouts", {"implicit": 5000})
            webdriver.request(port, "POST", f"{path}/url", {"url": page.as_uri()})
            webdriver.request(port, "POST", f"{path}/url", {"url": "about:history"})
            webdriver.find_element(port, session, f'.entry-title[href="{page.as_uri()}"]')

            assert webdriver.execute_script(
                port,
                session,
                """
                document.querySelector('#clear-history').click();
                return document.querySelector('.clear-browsing-data-dialog')?.open === true;
            """,
            ), "Clear history did not open the dialog on History"
            webdriver.execute_script(
                port,
                session,
                """
                for (const type of ['cached-files', 'download-history', 'site-data'])
                    document.querySelector(`#clear-browsing-data-${type}`).click();
                document.querySelector('#clear-browsing-data-remove-data').click();
            """,
            )

            webdriver.find_element(port, session, "#history-empty:not(.hidden)")
            assert webdriver.current_url(port, session) == "about:history"
            assert (
                webdriver.execute_script(port, session, "return document.querySelectorAll('.entry-title').length") == 0
            )
            print("PASS: Clearing browsing history removes the visited page from History")
        finally:
            helpers.stop_webdriver(process)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("webdriver_binary")
    run_test(parser.parse_args().webdriver_binary)
