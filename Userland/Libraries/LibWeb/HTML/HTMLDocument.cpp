/*
 * Copyright (c) 2023, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/HTML/HTMLDocument.h>

namespace Web::HTML {

JS_DEFINE_ALLOCATOR(HTMLDocument);

HTMLDocument::HTMLDocument(JS::Realm& realm, URL const& url)
    : Document(realm, url)
{
}

HTMLDocument::~HTMLDocument() = default;

WebIDL::ExceptionOr<JS::NonnullGCPtr<HTMLDocument>> HTMLDocument::construct_impl(JS::Realm& realm)
{
    return HTMLDocument::create(realm);
}

JS::NonnullGCPtr<HTMLDocument> HTMLDocument::create(JS::Realm& realm, URL const& url)
{
    return realm.heap().allocate<HTMLDocument>(realm, realm, url);
}

void HTMLDocument::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    set_prototype(&Bindings::ensure_web_prototype<Bindings::HTMLDocumentPrototype>(realm, "HTMLDocument"_fly_string));
}

}
