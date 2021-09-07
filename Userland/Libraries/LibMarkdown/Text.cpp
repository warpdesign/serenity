/*
 * Copyright (c) 2019-2020, Sergey Bugaev <bugaevc@serenityos.org>
 * Copyright (c) 2021, Peter Elliott <pelliott@serenityos.org>
 *
 * SPDX-License-Identifier: BSD-2-Clause
 */

#include <AK/Debug.h>
#include <AK/ScopeGuard.h>
#include <AK/StringBuilder.h>
#include <LibMarkdown/Text.h>
#include <ctype.h>
#include <string.h>

namespace Markdown {

void Text::EmphasisNode::render_to_html(StringBuilder& builder) const
{
    builder.append((strong) ? "<strong>" : "<em>");
    child->render_to_html(builder);
    builder.append((strong) ? "</strong>" : "</em>");
}

void Text::EmphasisNode::render_for_terminal(StringBuilder& builder) const
{
    if (strong) {
        builder.append("\e[1m");
        child->render_for_terminal(builder);
        builder.append("\e[22m");
    } else {
        builder.append("\e[3m");
        child->render_for_terminal(builder);
        builder.append("\e[23m");
    }
}

size_t Text::EmphasisNode::terminal_length() const
{
    return child->terminal_length();
}

void Text::CodeNode::render_to_html(StringBuilder& builder) const
{
    builder.append("<code>");
    code->render_to_html(builder);
    builder.append("</code>");
}

void Text::CodeNode::render_for_terminal(StringBuilder& builder) const
{
    builder.append("\e[1m");
    code->render_for_terminal(builder);
    builder.append("\e[22m");
}

size_t Text::CodeNode::terminal_length() const
{
    return code->terminal_length();
}

void Text::TextNode::render_to_html(StringBuilder& builder) const
{
    builder.append(escape_html_entities(text));
}

void Text::TextNode::render_for_terminal(StringBuilder& builder) const
{
    String text_copy = text;
    text_copy.replace("\n", " ");
    builder.append(text_copy);
}

size_t Text::TextNode::terminal_length() const
{
    return text.length();
}

void Text::LinkNode::render_to_html(StringBuilder& builder) const
{
    if (is_image) {
        builder.append("<img src=\"");
        href->render_to_html(builder);
        builder.append("\" alt=\"");
        text->render_to_html(builder);
        builder.append("\" >");
    } else {
        builder.append("<a href=\"");
        href->render_to_html(builder);
        builder.append("\">");
        text->render_to_html(builder);
        builder.append("</a>");
    }
}

void Text::LinkNode::render_for_terminal(StringBuilder& builder) const
{
    StringBuilder href_builder;
    href->render_for_terminal(href_builder);
    String href_string = href_builder.build();

    bool is_linked = href_string.contains("://");
    if (is_linked) {
        builder.append("\e]8;;");
        builder.append(href_string);
        builder.append("\e\\");
    }

    text->render_for_terminal(builder);

    if (is_linked) {
        builder.appendff(" <{}>", href_string);
        builder.append("\033]8;;\033\\");
    }
}

size_t Text::LinkNode::terminal_length() const
{
    return text->terminal_length();
}

void Text::MultiNode::render_to_html(StringBuilder& builder) const
{
    for (auto& child : children) {
        child.render_to_html(builder);
    }
}

void Text::MultiNode::render_for_terminal(StringBuilder& builder) const
{
    for (auto& child : children) {
        child.render_for_terminal(builder);
    }
}

size_t Text::MultiNode::terminal_length() const
{
    size_t length = 0;
    for (auto& child : children) {
        length += child.terminal_length();
    }
    return length;
}

size_t Text::terminal_length() const
{
    return m_node->terminal_length();
}

String Text::render_to_html() const
{
    StringBuilder builder;
    m_node->render_to_html(builder);
    return builder.build().trim(" \n\t");
}

String Text::render_for_terminal() const
{
    StringBuilder builder;
    m_node->render_for_terminal(builder);
    return builder.build().trim(" \n\t");
}

Text Text::parse(StringView const& str)
{
    Text text;
    auto const tokens = tokenize(str);
    auto iterator = tokens.begin();
    text.m_node = parse_sequence(iterator, false);
    return text;
}

Vector<Text::Token> Text::tokenize(StringView const& str)
{
    Vector<Token> tokens;
    StringBuilder current_token;

    auto flush_token = [&](bool left_flanking, bool right_flanking, bool is_run) {
        if (current_token.is_empty())
            return;

        tokens.append({
            current_token.build(),
            left_flanking,
            right_flanking,
            is_run,
        });
        current_token.clear();
    };

    for (size_t offset = 0; offset < str.length(); ++offset) {
        auto has = [&](StringView const& seq) {
            if (offset + seq.length() > str.length())
                return false;

            return str.substring_view(offset, seq.length()) == seq;
        };

        auto expect = [&](StringView const& seq) {
            VERIFY(has(seq));
            flush_token(false, false, false);
            current_token.append(seq);
            flush_token(false, false, false);
            offset += seq.length() - 1;
        };

        char ch = str[offset];

        if (ch == '\\' && offset + 1 < str.length()) {
            current_token.append(str[offset + 1]);
            ++offset;
        } else if (ch == '*' || ch == '_' || ch == '`') {
            flush_token(false, false, false);

            char delim = ch;
            size_t run_offset;
            for (run_offset = offset; run_offset < str.length() && str[run_offset] == delim; ++run_offset) {
                current_token.append(str[run_offset]);
            }

            bool left_flanking = run_offset < str.length() && !isspace(str[run_offset]);
            bool right_flanking = offset > 0 && !isspace(str[offset - 1]);
            flush_token(left_flanking, right_flanking, true);
            offset = run_offset - 1;

        } else if (ch == '\n') {
            flush_token(false, false, false);
            current_token.append(ch);
            flush_token(false, false, false);
        } else if (has("[")) {
            expect("[");
        } else if (has("![")) {
            expect("![");
        } else if (has("](")) {
            expect("](");
        } else if (has(")")) {
            expect(")");
        } else {
            current_token.append(ch);
        }
    }
    flush_token(false, false, false);
    return tokens;
}

NonnullOwnPtr<Text::MultiNode> Text::parse_sequence(Vector<Token>::ConstIterator& tokens, bool in_link)
{
    auto node = make<MultiNode>();

    for (; !tokens.is_end(); ++tokens) {
        if (tokens->is_run) {
            switch (tokens->run_char()) {
            case '*':
            case '_':
                node->children.append(parse_emph(tokens, in_link));
                break;
            case '`':
                node->children.append(parse_code(tokens));
                break;
            }
        } else if (!in_link && (*tokens == "[" || *tokens == "![")) {
            node->children.append(parse_link(tokens));
        } else if (in_link && *tokens == "](") {
            return node;
        } else {
            node->children.append(make<TextNode>(tokens->data));
        }

        if (in_link && !tokens.is_end() && *tokens == "](")
            return node;

        if (tokens.is_end())
            break;
    }
    return node;
}

bool Text::can_open(Token const& opening)
{
    return (opening.run_char() == '*' && opening.left_flanking) || (opening.run_char() == '_' && opening.left_flanking && !opening.right_flanking);
}

bool Text::can_close_for(Token const& opening, Text::Token const& closing)
{
    if (opening.run_char() != closing.run_char())
        return false;

    if (opening.run_length() != closing.run_length())
        return false;

    return (opening.run_char() == '*' && closing.right_flanking) || (opening.run_char() == '_' && !closing.left_flanking && closing.right_flanking);
}

NonnullOwnPtr<Text::Node> Text::parse_emph(Vector<Token>::ConstIterator& tokens, bool in_link)
{
    auto opening = *tokens;

    // Check that the opening delimiter run is properly flanking.
    if (!can_open(opening))
        return make<TextNode>(opening.data);

    auto child = make<MultiNode>();
    for (++tokens; !tokens.is_end(); ++tokens) {
        if (tokens->is_run) {
            if (can_close_for(opening, *tokens)) {
                return make<EmphasisNode>(opening.run_length() >= 2, move(child));
            }

            switch (tokens->run_char()) {
            case '*':
            case '_':
                child->children.append(parse_emph(tokens, in_link));
                break;
            case '`':
                child->children.append(parse_code(tokens));
                break;
            }
        } else if (*tokens == "[" || *tokens == "![") {
            child->children.append(parse_link(tokens));
        } else if (in_link && *tokens == "](") {
            child->children.prepend(make<TextNode>(opening.data));
            return child;
        } else {
            child->children.append(make<TextNode>(tokens->data));
        }

        if (in_link && !tokens.is_end() && *tokens == "](") {
            child->children.prepend(make<TextNode>(opening.data));
            return child;
        }

        if (tokens.is_end())
            break;
    }
    child->children.prepend(make<TextNode>(opening.data));
    return child;
}

NonnullOwnPtr<Text::Node> Text::parse_code(Vector<Token>::ConstIterator& tokens)
{
    auto opening = *tokens;

    auto is_closing = [&](Token const& token) {
        return token.is_run && token.run_char() == '`' && token.run_length() == opening.run_length();
    };

    bool is_all_whitespace = true;
    auto code = make<MultiNode>();
    for (auto iterator = tokens + 1; !iterator.is_end(); ++iterator) {
        if (is_closing(*iterator)) {
            tokens = iterator;

            // Strip first and last space, when appropriate.
            if (!is_all_whitespace) {
                auto& first = dynamic_cast<TextNode&>(code->children.first());
                auto& last = dynamic_cast<TextNode&>(code->children.last());
                if (first.text.starts_with(" ") && last.text.ends_with(" ")) {
                    first.text = first.text.substring(1);
                    last.text = last.text.substring(0, last.text.length() - 1);
                }
            }

            return make<CodeNode>(move(code));
        }

        is_all_whitespace = is_all_whitespace && iterator->data.is_whitespace();
        code->children.append(make<TextNode>((*iterator == "\n") ? " " : iterator->data));
    }

    return make<TextNode>(opening.data);
}

NonnullOwnPtr<Text::Node> Text::parse_link(Vector<Token>::ConstIterator& tokens)
{
    auto opening = *tokens++;
    bool is_image = opening == "![";

    auto link_text = parse_sequence(tokens, true);

    if (tokens.is_end() || *tokens != "](") {
        link_text->children.prepend(make<TextNode>(opening.data));
        return link_text;
    }
    auto seperator = *tokens;
    VERIFY(seperator == "](");

    auto address = make<MultiNode>();
    for (auto iterator = tokens + 1; !iterator.is_end(); ++iterator) {
        if (*iterator == ")") {
            tokens = iterator;
            return make<LinkNode>(is_image, move(link_text), move(address));
        }

        address->children.append(make<TextNode>(iterator->data));
    }

    link_text->children.prepend(make<TextNode>(opening.data));
    link_text->children.append(make<TextNode>(seperator.data));
    return link_text;
}
}
