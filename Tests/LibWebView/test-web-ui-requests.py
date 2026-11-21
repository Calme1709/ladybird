#!/usr/bin/env python3
#
# Copyright (c) 2026-present, the Ladybird developers.
#
# SPDX-License-Identifier: BSD-2-Clause

import argparse
import importlib
import os

webdriver = importlib.import_module("test-webdriver-session-history")
helpers = importlib.import_module("test-webdriver-delete-session")


def run_test(webdriver_binary):
    process, port = helpers.launch_webdriver(webdriver_binary, env={**os.environ, "QT_QPA_PLATFORM": "offscreen"})
    try:
        session = webdriver.request(
            port, "POST", "/session", {"capabilities": {"alwaysMatch": {"ladybird:headless": False}}}
        )["value"]["sessionId"]
        path = f"/session/{session}"
        webdriver.request(port, "POST", f"{path}/timeouts", {"script": 5000})
        webdriver.request(port, "POST", f"{path}/url", {"url": "about:history"})

        results = webdriver.execute_script(
            port,
            session,
            """
            return (async () => {
                if (!window.ladybird)
                    await new Promise(resolve => document.addEventListener('WebUILoaded', resolve, {once: true}));

                const cyclic = {};
                cyclic.self = cyclic;
                const results = await Promise.allSettled([
                    ladybird.request('unknownHistoryRequest'),
                    ladybird.request('unknownHistoryRequest', cyclic),
                ]);
                return results.map(result => result.reason?.name);
            })();
        """,
        )
        assert results == ["OperationError", "DataCloneError"], results

        # Observe the old document's promise from a window that survives the navigation.
        history_window = webdriver.request(port, "GET", f"{path}/window")["value"]
        webdriver.execute_script(port, session, "window.open()")
        (observer,) = set(webdriver.request(port, "GET", f"{path}/window/handles")["value"]) - {history_window}
        webdriver.request(port, "POST", f"{path}/window", {"handle": observer})
        outcome = webdriver.execute_script(
            port,
            session,
            """
            const historyWindow = window.opener;
            return new Promise(resolve => {
                historyWindow.addEventListener('pagehide', () => {
                    historyWindow.ladybird.request('unknownHistoryRequest', {}).then(
                        () => resolve('fulfilled'), error => resolve(error.name));
                }, { once: true });
                historyWindow.location.replace('about:blank');
            });
        """,
        )
        assert outcome == "AbortError", outcome
        webdriver.request(port, "POST", f"{path}/window", {"handle": history_window})
        assert webdriver.current_url(port, session) == "about:blank"
        print("PASS: WebUI requests reject errors and abort on navigation")
    finally:
        helpers.stop_webdriver(process)


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("webdriver_binary")
    run_test(parser.parse_args().webdriver_binary)
