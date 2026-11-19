/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/JsonObject.h>
#include <LibCore/Promise.h>
#include <LibWebView/Application.h>
#include <LibWebView/WebContentClient.h>
#include <LibWebView/WebUI/ClearBrowsingDataUI.h>

namespace WebView {

ReadonlySpan<WebUI::SharedInterface> ClearBrowsingDataUI::interfaces()
{
    static constexpr WebUI::SharedInterface interfaces[] = {
        { "clearBrowsingData"sv, clear_browsing_data },
    };
    return interfaces;
}

NonnullRefPtr<Core::Promise<JsonValue>> ClearBrowsingDataUI::estimate_browsing_data_sizes(JsonValue const& options)
{
    if (!options.is_object())
        return Core::Promise<JsonValue>::rejected(Error::from_string_literal("Browsing data estimate options must be an object"));

    auto& application = Application::the();

    auto since = [&]() {
        if (auto since = options.as_object().get_integer<i64>("since"sv); since.has_value())
            return UnixDateTime::from_milliseconds_since_epoch(*since);
        return UnixDateTime::earliest();
    }();

    return application.estimate_browsing_data_size_accessed_since(since)
        ->map<JsonValue>([](Application::BrowsingDataSizes const& sizes) {
            JsonObject result;

            result.set("cacheSizeSinceRequestedTime"sv, sizes.cache_size_since_requested_time);
            result.set("totalCacheSize"sv, sizes.total_cache_size);

            result.set("siteDataSizeSinceRequestedTime"sv, sizes.site_data_size_since_requested_time);
            result.set("totalSiteDataSize"sv, sizes.total_site_data_size);

            return JsonValue { move(result) };
        });
}

void ClearBrowsingDataUI::clear_browsing_data(WebUI& web_ui, WebContentClient& client, JsonValue const& options)
{
    if (!options.is_object())
        return;

    if (client.is_private() == IsPrivate::Yes) {
        if (auto* page = client.page(web_ui.page_id()))
            Application::the().show_private_session_reset_confirmation(*page);
        return;
    }

    Application::ClearBrowsingDataOptions clear_browsing_data_options;

    if (auto since = options.as_object().get_integer<i64>("since"sv); since.has_value())
        clear_browsing_data_options.since = UnixDateTime::from_milliseconds_since_epoch(*since);

    clear_browsing_data_options.delete_cached_files = options.as_object().get_bool("cachedFiles"sv).value_or(false)
        ? Application::ClearBrowsingDataOptions::Delete::Yes
        : Application::ClearBrowsingDataOptions::Delete::No;

    clear_browsing_data_options.delete_history = options.as_object().get_bool("history"sv).value_or(false)
        ? Application::ClearBrowsingDataOptions::Delete::Yes
        : Application::ClearBrowsingDataOptions::Delete::No;

    clear_browsing_data_options.delete_download_history = options.as_object().get_bool("downloadHistory"sv).value_or(false)
        ? Application::ClearBrowsingDataOptions::Delete::Yes
        : Application::ClearBrowsingDataOptions::Delete::No;

    clear_browsing_data_options.delete_site_data = options.as_object().get_bool("siteData"sv).value_or(false)
        ? Application::ClearBrowsingDataOptions::Delete::Yes
        : Application::ClearBrowsingDataOptions::Delete::No;

    auto& application = Application::the();

    application.clear_browsing_data(clear_browsing_data_options)
        ->when_resolved([weak_ui = web_ui.make_weak_ptr<WebUI>()](Empty) {
            if (auto self = weak_ui.strong_ref())
                self->async_send_message("clearedBrowsingData"sv, {});
        })
        .when_rejected([](Error const& error) {
            dbgln("Failed to clear browser data: {}", error);
        });
}

}
