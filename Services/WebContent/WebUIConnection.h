/*
 * Copyright (c) 2025, Tim Flynn <trflynn89@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#pragma once

#include <AK/HashMap.h>
#include <AK/JsonValue.h>
#include <AK/NonnullRefPtr.h>
#include <AK/Utf16String.h>
#include <LibGC/Ptr.h>
#include <LibIPC/ConnectionFromClient.h>
#include <LibIPC/Transport.h>
#include <LibIPC/TransportHandle.h>
#include <LibJS/Heap/Cell.h>
#include <LibWeb/Forward.h>
#include <WebContent/WebUIClientEndpoint.h>
#include <WebContent/WebUIServerEndpoint.h>

namespace WebContent {

class WebUIConnection final : public IPC::ConnectionFromClient<WebUIClientEndpoint, WebUIServerEndpoint> {
public:
    static ErrorOr<NonnullRefPtr<WebUIConnection>> connect(IPC::TransportHandle, Web::DOM::Document&);

    void visit_edges(JS::Cell::Visitor&);

    void received_message_from_web_ui(Utf16String const& name, JS::Value data);
    Web::WebIDL::ExceptionOr<void> received_request_from_web_ui(Utf16String const& name, JS::Value data, GC::Ref<Web::WebIDL::Promise>);

    Web::DOM::Document const& document() const { return m_document; }

private:
    WebUIConnection(NonnullOwnPtr<IPC::Transport>, Web::DOM::Document&);

    virtual void die() override;
    virtual void send_message(String name, JsonValue data) override;
    virtual void request_result(u64 request_id, JsonValue data, Optional<String> error) override;

    GC::Ref<Web::DOM::Document> m_document;
    u64 m_next_request_id { 1 };
    HashMap<u64, GC::Ref<Web::WebIDL::Promise>> m_pending_requests;
};

}
