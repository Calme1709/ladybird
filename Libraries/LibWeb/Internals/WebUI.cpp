/*
 * Copyright (c) 2025, Tim Flynn <trflynn89@ladybird.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibGC/Heap.h>
#include <LibWeb/HTML/Window.h>
#include <LibWeb/Internals/WebUI.h>
#include <LibWeb/Page/Page.h>

namespace Web::Internals {

GC_DEFINE_ALLOCATOR(WebUI);

GC::Ref<WebUI> WebUI::create(HTML::Window& window)
{
    return GC::Heap::the().allocate<WebUI>(window);
}

WebUI::WebUI(HTML::Window& window)
    : InternalsBase(window)
{
}

WebUI::~WebUI() = default;

void WebUI::send_message(Utf16String const& name, JS::Value data)
{
    page().client().received_message_from_web_ui(name, data);
}

WebIDL::ExceptionOr<void> WebUI::request(Utf16String const& name, JS::Value data, GC::Ref<WebIDL::Promise> promise)
{
    return page().client().received_request_from_web_ui(window().associated_document(), name, data, promise);
}

}
