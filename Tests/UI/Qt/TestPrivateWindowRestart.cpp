/*
 * Copyright (c) 2026-present, the Ladybird developers.
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibTest/TestCase.h>
#include <LibURL/InternalURLs.h>
#include <UI/Qt/Application.h>
#include <UI/Qt/ChromeStyle.h>
#include <UI/Qt/SessionStore.h>
#include <UI/Qt/Tab.h>

#include <QAction>
#include <QApplication>
#include <QByteArray>
#include <QProcess>
#include <QTemporaryDir>
#include <QtCore/qtestsupport_core.h>

#include <algorithm>

TEST_CASE(private_window_launch_handoff_recovery_and_restart)
{
    qputenv("QT_QPA_PLATFORM", "offscreen");
    QCoreApplication::setAttribute(Qt::AA_DontCreateNativeWidgetSiblings);
    QTemporaryDir profile;
    VERIFY(profile.isValid());
    auto profile_path = profile.path().toUtf8();
    auto profile_option = "--profile-path=" + profile_path;
    QByteArray argument_storage[] {
        "Ladybird",
        "https://private.test/one",
        "--screenshot-path",
        "--new-private-window",
        "--new-private-window",
        "--new-window",
        "https://private.test/two",
        "--new-private-window",
        profile_option,
    };
    Vector<char*> argv;
    Vector<StringView> arguments;
    for (auto& argument : argument_storage) {
        argv.append(argument.data());
        arguments.append({ argument.constData(), static_cast<size_t>(argument.size()) });
    }
    argv.append(nullptr);

    OwnPtr<Ladybird::Application> app = TRY_OR_FAIL(Ladybird::Application::create({
        .argc = static_cast<int>(arguments.size()),
        .argv = argv.data(),
        .strings = arguments,
    }));
    Ladybird::ChromeStyle::install_application_style();

    {
        auto store = Ladybird::SessionStore::create_deferred(ByteString { (profile_path + "/data").constData() });
        TRY_OR_FAIL(store->start(Ladybird::SessionLaunchPolicy::None));
        auto tab = TRY_OR_FAIL(store->tab_opened({ .window_id = {}, .initial_url = URL::about_history(), .insertion_index = {}, .is_active = Ladybird::SessionStore::IsActive::No }));
        TRY_OR_FAIL(store->tab_closed({ .tab_id = tab, .closed_at = UnixDateTime::now() }));
        store->mark_clean_exit();
    }

    auto expected_arguments = Vector<ByteString> { "--screenshot-path", "--new-private-window", "--new-window", profile_option.constData() };
    auto expected_options = TRY_OR_FAIL(String::join(' ', expected_arguments));
    EXPECT_EQ(app->command_line(), TRY_OR_FAIL(String::formatted("Ladybird {}", expected_options)));

    app->open_new_window(WebView::IsPrivate::Yes);
    auto* initial_window = app->active_window_if_any();
    Ladybird::HubConnection::the().async_ui_did_start();
    EXPECT_EQ(QProcess::execute(TEST_LADYBIRD_PATH, { profile_option, "--new-private-window", "about:history", "about:settings" }), 0);
    auto received_both_urls = QTest::qWaitFor([&] {
        auto& window = app->active_window();
        return window.tab_count() == 2 && window.tab_at(0)->url() == URL::about_history() && window.tab_at(1)->url() == URL::about_settings();
    });
    EXPECT(received_both_urls);
    auto windows = QApplication::topLevelWidgets();
    EXPECT_EQ(std::ranges::count_if(windows, [](auto* widget) { return is<Ladybird::BrowserWindow>(*widget); }), 2);
    EXPECT(windows.contains(initial_window));
    EXPECT(app->active_window_if_any() != initial_window);
    EXPECT_EQ(app->active_window().is_private(), WebView::IsPrivate::Yes);

    auto* reopen_tab = app->reopen_recently_closed_tab_action();
    EXPECT(!reopen_tab->isEnabled());
    auto& window = app->new_window({ URL::about_blank() }, app->configuration_for_new_window());
    EXPECT(window.session().has_closed_units());
    EXPECT(reopen_tab->isEnabled());

    app->restart();
    auto restart = app->take_restart_command();
    app = nullptr;

    VERIFY(restart.has_value() && restart->hub.has_value());
    EXPECT_EQ(restart->arguments, expected_arguments);
    EXPECT_EQ(TRY_OR_FAIL(restart->hub->wait_for_termination()), 0);
}
