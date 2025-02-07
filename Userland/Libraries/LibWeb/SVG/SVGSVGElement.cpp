/*
 * Copyright (c) 2020, Matthew Olsson <matthewcolsson@gmail.com>
 * Copyright (c) 2022, Andreas Kling <kling@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <LibWeb/CSS/Parser/Parser.h>
#include <LibWeb/CSS/StyleComputer.h>
#include <LibWeb/CSS/StyleValues/LengthStyleValue.h>
#include <LibWeb/CSS/StyleValues/PercentageStyleValue.h>
#include <LibWeb/DOM/Document.h>
#include <LibWeb/DOM/Event.h>
#include <LibWeb/HTML/Parser/HTMLParser.h>
#include <LibWeb/Layout/SVGSVGBox.h>
#include <LibWeb/SVG/AttributeNames.h>
#include <LibWeb/SVG/SVGAnimatedRect.h>
#include <LibWeb/SVG/SVGSVGElement.h>

namespace Web::SVG {

JS_DEFINE_ALLOCATOR(SVGSVGElement);

SVGSVGElement::SVGSVGElement(DOM::Document& document, DOM::QualifiedName qualified_name)
    : SVGGraphicsElement(document, qualified_name)
{
}

void SVGSVGElement::initialize(JS::Realm& realm)
{
    Base::initialize(realm);
    set_prototype(&Bindings::ensure_web_prototype<Bindings::SVGSVGElementPrototype>(realm, "SVGSVGElement"_fly_string));
    m_view_box_for_bindings = heap().allocate<SVGAnimatedRect>(realm, realm);
}

void SVGSVGElement::visit_edges(Visitor& visitor)
{
    Base::visit_edges(visitor);
    visitor.visit(m_view_box_for_bindings);
}

JS::GCPtr<Layout::Node> SVGSVGElement::create_layout_node(NonnullRefPtr<CSS::StyleProperties> style)
{
    return heap().allocate_without_realm<Layout::SVGSVGBox>(document(), *this, move(style));
}

void SVGSVGElement::apply_presentational_hints(CSS::StyleProperties& style) const
{
    Base::apply_presentational_hints(style);
    auto parsing_context = CSS::Parser::ParsingContext { document(), CSS::Parser::ParsingContext::Mode::SVGPresentationAttribute };

    auto x_attribute = attribute(SVG::AttributeNames::x);
    if (auto x_value = parse_css_value(parsing_context, x_attribute.value_or(String {}), CSS::PropertyID::X)) {
        style.set_property(CSS::PropertyID::X, x_value.release_nonnull());
    }

    auto y_attribute = attribute(SVG::AttributeNames::y);
    if (auto y_value = parse_css_value(parsing_context, y_attribute.value_or(String {}), CSS::PropertyID::Y)) {
        style.set_property(CSS::PropertyID::Y, y_value.release_nonnull());
    }

    auto width_attribute = attribute(SVG::AttributeNames::width);
    if (auto width_value = parse_css_value(parsing_context, width_attribute.value_or(String {}), CSS::PropertyID::Width)) {
        style.set_property(CSS::PropertyID::Width, width_value.release_nonnull());
    } else if (width_attribute == "") {
        // If the `width` attribute is an empty string, it defaults to 100%.
        // This matches WebKit and Blink, but not Firefox. The spec is unclear.
        // FIXME: Figure out what to do here.
        style.set_property(CSS::PropertyID::Width, CSS::PercentageStyleValue::create(CSS::Percentage { 100 }));
    }

    // Height defaults to 100%
    auto height_attribute = attribute(SVG::AttributeNames::height);
    if (auto height_value = parse_css_value(parsing_context, height_attribute.value_or(String {}), CSS::PropertyID::Height)) {
        style.set_property(CSS::PropertyID::Height, height_value.release_nonnull());
    } else if (height_attribute == "") {
        // If the `height` attribute is an empty string, it defaults to 100%.
        // This matches WebKit and Blink, but not Firefox. The spec is unclear.
        // FIXME: Figure out what to do here.
        style.set_property(CSS::PropertyID::Height, CSS::PercentageStyleValue::create(CSS::Percentage { 100 }));
    }
}

void SVGSVGElement::attribute_changed(FlyString const& name, Optional<String> const& value)
{
    SVGGraphicsElement::attribute_changed(name, value);

    if (name.equals_ignoring_ascii_case(SVG::AttributeNames::viewBox)) {
        if (!value.has_value()) {
            m_view_box_for_bindings->set_nulled(true);
        } else {
            m_view_box = try_parse_view_box(value.value_or(String {}));
            m_view_box_for_bindings->set_nulled(!m_view_box.has_value());
            if (m_view_box.has_value()) {
                m_view_box_for_bindings->set_base_val(Gfx::DoubleRect { m_view_box->min_x, m_view_box->min_y, m_view_box->width, m_view_box->height });
                m_view_box_for_bindings->set_anim_val(Gfx::DoubleRect { m_view_box->min_x, m_view_box->min_y, m_view_box->width, m_view_box->height });
            }
        }
    }
    if (name.equals_ignoring_ascii_case(SVG::AttributeNames::preserveAspectRatio))
        m_preserve_aspect_ratio = AttributeParser::parse_preserve_aspect_ratio(value.value_or(String {}));
    if (name.equals_ignoring_ascii_case(SVG::AttributeNames::width) || name.equals_ignoring_ascii_case(SVG::AttributeNames::height))
        update_fallback_view_box_for_svg_as_image();
}

void SVGSVGElement::update_fallback_view_box_for_svg_as_image()
{
    // AD-HOC: This creates a fallback viewBox for SVGs used as images.
    //         If the <svg> element has width and height, but no viewBox,
    //         we fall back to a synthetic viewBox="0 0 width height".

    Optional<double> width;
    Optional<double> height;

    auto width_attribute = get_attribute_value(SVG::AttributeNames::width);
    auto parsing_context = CSS::Parser::ParsingContext { document() };
    if (auto width_value = parse_css_value(parsing_context, width_attribute, CSS::PropertyID::Width)) {
        if (width_value->is_length() && width_value->as_length().length().is_absolute())
            width = width_value->as_length().length().absolute_length_to_px().to_double();
    }

    auto height_attribute = get_attribute_value(SVG::AttributeNames::height);
    if (auto height_value = parse_css_value(parsing_context, height_attribute, CSS::PropertyID::Height)) {
        if (height_value->is_length() && height_value->as_length().length().is_absolute())
            height = height_value->as_length().length().absolute_length_to_px().to_double();
    }

    if (width.has_value() && width.value() > 0 && height.has_value() && height.value() > 0) {
        m_fallback_view_box_for_svg_as_image = ViewBox { 0, 0, width.value(), height.value() };
    } else {
        m_fallback_view_box_for_svg_as_image = {};
    }
}

void SVGSVGElement::set_fallback_view_box_for_svg_as_image(Optional<ViewBox> view_box)
{
    m_fallback_view_box_for_svg_as_image = view_box;
}

Optional<ViewBox> SVGSVGElement::view_box() const
{
    if (m_view_box.has_value())
        return m_view_box;

    // NOTE: If the parent is a document, we're an <svg> element used as an image.
    if (parent() && parent()->is_document() && m_fallback_view_box_for_svg_as_image.has_value())
        return m_fallback_view_box_for_svg_as_image;

    return {};
}

}
