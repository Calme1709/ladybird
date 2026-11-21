/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <LibWebView/WebUI.h>

namespace WebView {

class ClearBrowsingDataUI {
public:
    static ReadonlySpan<WebUI::SharedInterface> interfaces();
    static NonnullRefPtr<Core::Promise<JsonValue>> estimate_browsing_data_sizes(JsonValue const&);

private:
    static void clear_browsing_data(WebUI&, WebContentClient&, JsonValue const&);
};

}
