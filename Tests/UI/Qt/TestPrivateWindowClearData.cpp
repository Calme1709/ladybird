/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <LibURL/InternalURLs.h>
#include <LibURL/Parser.h>
#include <UI/Qt/Application.h>
#include <UI/Qt/ChromeStyle.h>
#include <UI/Qt/StringUtils.h>
#include <UI/Qt/Tab.h>

#include <QAccessible>
#include <QLabel>
#include <QtCore/qtestsupport_core.h>

static QAccessibleInterface* enabled_clear_history_button(QAccessibleInterface& node)
{
    if (node.role() == QAccessible::Button && node.text(QAccessible::Name) == QStringLiteral("Clear history…") && !node.state().disabled)
        return &node;
    for (int i = 0; i < node.childCount(); ++i) {
        if (auto* button = enabled_clear_history_button(*node.child(i)))
            return button;
    }
    return nullptr;
}

static bool shows_private_session_confirmation(Ladybird::Tab const& tab)
{
    auto* title = tab.findChild<QLabel*>("LadybirdMessagePopoverTitle");
    return title && title->isVisible() && title->text() == QStringLiteral("Start a fresh private session?");
}

static void check_browsing_data_estimates(Ladybird::Tab& tab, QString const& expected_result)
{
    constexpr auto script = R"(javascript:{
        const estimate = () => ladybird.request('estimateBrowsingDataSizes', {})
            .then(sizes => document.title = typeof sizes.totalCacheSize)
            .catch(error => document.title = error.name);
        if (window.ladybird) estimate();
        else document.addEventListener('WebUILoaded', estimate, { once: true });
    })"sv;
    auto check_page = [&](URL::URL const& page, QString const& title) {
        tab.navigate(page);
        EXPECT(QTest::qWaitFor([&] { return tab.title() == title && !tab.is_loading(); }));
        tab.view().load(URL::Parser::basic_parse(script).value());
        EXPECT(QTest::qWaitFor([&] { return tab.title() != title; }));
        EXPECT_EQ(tab.title(), expected_result);
    };
    check_page(URL::about_history(), QStringLiteral("History"));
    check_page(URL::about_settings(), QStringLiteral("Settings"));
}

TEST_CASE(clearing_private_browsing_data_shows_the_private_session_confirmation)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    char executable[] = "Ladybird";
    char test_mode[] = "--test-mode";
    char* argv[] { executable, test_mode, nullptr };
    StringView arguments[] { { executable, sizeof(executable) - 1 }, { test_mode, sizeof(test_mode) - 1 } };

    OwnPtr<Ladybird::Application> app = TRY_OR_FAIL(Ladybird::Application::create({
        .argc = 2,
        .argv = argv,
        .strings = arguments,
    }));
    Ladybird::ChromeStyle::install_application_style();

    auto& window = app->new_window({ URL::about_blank() }, app->configuration_for_new_window(), Ladybird::BrowserWindow::IsPopupWindow::No, WebView::IsPrivate::Yes);
    auto& regular_window = app->new_window({ URL::about_blank() }, app->configuration_for_new_window());
    Ladybird::HubConnection::the().async_ui_did_start();

    check_browsing_data_estimates(*regular_window.current_tab(), QStringLiteral("number"));
    check_browsing_data_estimates(*window.current_tab(), QStringLiteral("OperationError"));

    auto& history_tab = *window.current_tab();
    window.activateWindow();
    history_tab.navigate(URL::about_history());
    EXPECT(QTest::qWaitFor([&] { return history_tab.title() == QStringLiteral("History") && !history_tab.is_loading(); }));
    history_tab.view().request_accessibility_tree();
    auto* document = QAccessible::queryAccessibleInterface(&history_tab.view());
    VERIFY(document);
    QAccessibleInterface* button = nullptr;
    EXPECT(QTest::qWaitFor([&] {
        button = enabled_clear_history_button(*document);
        return button;
    }));
    if (button)
        button->actionInterface()->doAction(QAccessibleActionInterface::pressAction());
    EXPECT(QTest::qWaitFor([&] { return shows_private_session_confirmation(history_tab); }));
    EXPECT_EQ(history_tab.url(), URL::about_history());
    history_tab.close_popovers();

    auto settings_url = URL::about_settings();
    settings_url.set_fragment("clearBrowsingData"sv);
    auto& settings_tab = window.new_tab_from_url(settings_url, Web::HTML::ActivateTab::No, Ladybird::BrowserWindow::TabLocation::end());
    auto show_confirmation = move(settings_tab.view().on_request_private_session_reset_confirmation);
    bool confirmation_requested = false;
    settings_tab.view().on_request_private_session_reset_confirmation = [&] {
        confirmation_requested = true;
        show_confirmation();
    };
    settings_url.set_fragment("privacy"sv);
    EXPECT(QTest::qWaitFor([&] { return confirmation_requested && settings_tab.url() == settings_url; }));
    EXPECT(window.current_tab() == &history_tab);
    EXPECT(!shows_private_session_confirmation(settings_tab));
    window.activate_tab(window.tab_index(&settings_tab));
    EXPECT(QTest::qWaitFor([&] { return shows_private_session_confirmation(settings_tab); }));
    EXPECT_EQ(regular_window.tab_count(), 1);

    window.activate_tab(window.tab_index(&history_tab));
    settings_tab.view().on_request_private_session_reset_confirmation();
    settings_tab.view().on_web_content_crashed(WebView::ViewImplementation::WebContentCrashReason::ProcessCrash);
    window.activate_tab(window.tab_index(&settings_tab));
    EXPECT(!shows_private_session_confirmation(settings_tab));

    regular_window.hide();
    window.hide();
    app->restart();
    auto restart = app->take_restart_command();
    app = nullptr;

    VERIFY(restart.has_value() && restart->hub.has_value());
    EXPECT_EQ(TRY_OR_FAIL(restart->hub->wait_for_termination()), 0);
}
