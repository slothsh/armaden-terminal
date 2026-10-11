#include <md4c.h>

export module markdown:parser;

import std;
import regex;
import :model;

namespace {

auto copyText(MD_ATTRIBUTE const* attribute) -> std::string {
    if (attribute == nullptr || attribute->text == nullptr) {
        return {};
    }

    return std::string(attribute->text, attribute->size);
}

auto copyText(char const* text, MD_SIZE size) -> std::string {
    if (text == nullptr) {
        return {};
    }

    return std::string(text, size);
}

auto toBlockKind(MD_BLOCKTYPE type) -> mts::markdown::NodeKind {
    using mts::markdown::NodeKind;

    switch (type) {
        case MD_BLOCK_DOC: return NodeKind::Document;
        case MD_BLOCK_QUOTE: return NodeKind::BlockQuote;
        case MD_BLOCK_UL: return NodeKind::UnorderedList;
        case MD_BLOCK_OL: return NodeKind::OrderedList;
        case MD_BLOCK_LI: return NodeKind::ListItem;
        case MD_BLOCK_HR: return NodeKind::HorizontalRule;
        case MD_BLOCK_H: return NodeKind::Heading;
        case MD_BLOCK_CODE: return NodeKind::CodeBlock;
        case MD_BLOCK_HTML: return NodeKind::RawHtml;
        case MD_BLOCK_P: return NodeKind::Paragraph;
        case MD_BLOCK_TABLE: return NodeKind::Table;
        case MD_BLOCK_THEAD: return NodeKind::TableHead;
        case MD_BLOCK_TBODY: return NodeKind::TableBody;
        case MD_BLOCK_TR: return NodeKind::TableRow;
        case MD_BLOCK_TH: return NodeKind::TableHeaderCell;
        case MD_BLOCK_TD: return NodeKind::TableCell;
    }

    return NodeKind::Text;
}

auto toSpanKind(MD_SPANTYPE type) -> mts::markdown::NodeKind {
    using mts::markdown::NodeKind;

    switch (type) {
        case MD_SPAN_EM: return NodeKind::Emphasis;
        case MD_SPAN_STRONG: return NodeKind::Strong;
        case MD_SPAN_A: return NodeKind::Link;
        case MD_SPAN_IMG: return NodeKind::Image;
        case MD_SPAN_CODE: return NodeKind::InlineCode;
        case MD_SPAN_DEL: return NodeKind::Strikethrough;
        case MD_SPAN_LATEXMATH:
        case MD_SPAN_LATEXMATH_DISPLAY: return NodeKind::InlineCode;
        case MD_SPAN_WIKILINK: return NodeKind::Link;
        case MD_SPAN_U: return NodeKind::Emphasis;
    }

    return NodeKind::Text;
}

struct ParseState {
    mts::markdown::Node root;
    std::vector<mts::markdown::Node*> stack;
    bool valid = true;

    ParseState() {
        root.kind = mts::markdown::NodeKind::Document;
        stack.push_back(&root);
    }

    auto push(mts::markdown::Node node) -> mts::markdown::Node* {
        if (stack.empty()) {
            valid = false;
            return nullptr;
        }

        stack.back()->children.push_back(std::move(node));
        mts::markdown::Node* const child = &stack.back()->children.back();
        stack.push_back(child);

        return child;
    }

    auto pop() -> void {
        if (stack.size() <= 1) {
            valid = false;
            return;
        }

        stack.pop_back();
    }
};

auto enterBlock(MD_BLOCKTYPE type, void* detail, void* userdata) -> int {
    ParseState& state = *static_cast<ParseState*>(userdata);
    using mts::markdown::Node;
    using mts::markdown::NodeKind;

    if (type == MD_BLOCK_DOC) {
        return 0;
    }

    Node node;
    node.kind = toBlockKind(type);
    if (type == MD_BLOCK_H && detail != nullptr) {
        node.level = static_cast<int>(static_cast<MD_BLOCK_H_DETAIL const*>(detail)->level);
    } else if (type == MD_BLOCK_CODE && detail != nullptr) {
        auto const* code = static_cast<MD_BLOCK_CODE_DETAIL const*>(detail);
        node.info = copyText(&code->info);
        node.language = copyText(&code->lang);
        node.languageIdentifierValid = mts::regex::fullMatch(mts::regex::Pattern::CodeFenceLanguage, node.language);
        node.marker = static_cast<char>(code->fence_char);
    } else if (type == MD_BLOCK_UL && detail != nullptr) {
        auto const* list = static_cast<MD_BLOCK_UL_DETAIL const*>(detail);
        node.tight = list->is_tight != 0;
        node.marker = static_cast<char>(list->mark);
    } else if (type == MD_BLOCK_OL && detail != nullptr) {
        auto const* list = static_cast<MD_BLOCK_OL_DETAIL const*>(detail);
        node.start = static_cast<int>(std::min(list->start, static_cast<unsigned>(std::numeric_limits<int>::max())));
        node.marker = static_cast<char>(list->mark_delimiter);
        node.tight = list->is_tight != 0;
    } else if (type == MD_BLOCK_LI && detail != nullptr) {
        auto const* item = static_cast<MD_BLOCK_LI_DETAIL const*>(detail);
        node.task = item->is_task != 0;
        node.checked = item->is_task != 0 && item->task_mark != ' ';
    } else if ((type == MD_BLOCK_TH || type == MD_BLOCK_TD) && detail != nullptr) {
        node.alignment = static_cast<int>(static_cast<MD_BLOCK_TD_DETAIL const*>(detail)->align);
    }

    if (state.push(std::move(node)) == nullptr) {
        return 1;
    }

    return 0;
}

auto leaveBlock(MD_BLOCKTYPE type, void*, void* userdata) -> int {
    ParseState& state = *static_cast<ParseState*>(userdata);
    if (type != MD_BLOCK_DOC) {
        state.pop();
    }

    return state.valid ? 0 : 1;
}

auto enterSpan(MD_SPANTYPE type, void* detail, void* userdata) -> int {
    ParseState& state = *static_cast<ParseState*>(userdata);
    using mts::markdown::Node;

    Node node;
    node.kind = toSpanKind(type);
    if (type == MD_SPAN_A && detail != nullptr) {
        auto const* link = static_cast<MD_SPAN_A_DETAIL const*>(detail);
        node.destination = copyText(&link->href);
        node.title = copyText(&link->title);
    } else if (type == MD_SPAN_IMG && detail != nullptr) {
        auto const* image = static_cast<MD_SPAN_IMG_DETAIL const*>(detail);
        node.destination = copyText(&image->src);
        node.title = copyText(&image->title);
    } else if (type == MD_SPAN_WIKILINK && detail != nullptr) {
        node.destination = copyText(&static_cast<MD_SPAN_WIKILINK_DETAIL const*>(detail)->target);
    }

    if (state.push(std::move(node)) == nullptr) {
        return 1;
    }

    return 0;
}

auto plainText(mts::markdown::Node const& node) -> std::string {
    using mts::markdown::NodeKind;

    if (node.kind == NodeKind::Text || node.kind == NodeKind::Entity || node.kind == NodeKind::InlineCode) {
        return node.text;
    }
    if (node.kind == NodeKind::HardBreak || node.kind == NodeKind::SoftBreak) {
        return " ";
    }

    std::string output;
    for (mts::markdown::Node const& child : node.children) {
        output += plainText(child);
    }

    return output;
}

auto leaveSpan(MD_SPANTYPE type, void*, void* userdata) -> int {
    ParseState& state = *static_cast<ParseState*>(userdata);
    if (state.stack.size() <= 1) {
        state.valid = false;
        return 1;
    }

    if (type == MD_SPAN_IMG) {
        state.stack.back()->text = plainText(*state.stack.back());
    }
    state.pop();

    return state.valid ? 0 : 1;
}

auto appendText(ParseState& state, MD_TEXTTYPE type, char const* text, MD_SIZE size) -> void {
    using mts::markdown::Node;
    using mts::markdown::NodeKind;

    if (state.stack.empty()) {
        state.valid = false;
        return;
    }

    Node* const current = state.stack.back();
    if (current->kind == NodeKind::CodeBlock || current->kind == NodeKind::RawHtml || current->kind == NodeKind::InlineCode) {
        current->text += copyText(text, size);
        return;
    }

    Node node;
    switch (type) {
        case MD_TEXT_NORMAL:
            node.kind = NodeKind::Text;
            break;
        case MD_TEXT_ENTITY:
            node.kind = NodeKind::Entity;
            break;
        case MD_TEXT_NULLCHAR:
            node.kind = NodeKind::Text;
            node.text = "\xEF\xBF\xBD";
            current->children.push_back(std::move(node));
            return;
        case MD_TEXT_BR:
            node.kind = NodeKind::HardBreak;
            current->children.push_back(std::move(node));
            return;
        case MD_TEXT_SOFTBR:
            node.kind = NodeKind::SoftBreak;
            current->children.push_back(std::move(node));
            return;
        case MD_TEXT_CODE:
        case MD_TEXT_LATEXMATH:
            node.kind = NodeKind::Text;
            break;
        case MD_TEXT_HTML:
            node.kind = NodeKind::RawHtml;
            break;
    }

    node.text = copyText(text, size);
    if (!current->children.empty() && current->children.back().kind == node.kind) {
        current->children.back().text += node.text;
    } else {
        current->children.push_back(std::move(node));
    }
}

auto text(MD_TEXTTYPE type, char const* value, MD_SIZE size, void* userdata) -> int {
    ParseState& state = *static_cast<ParseState*>(userdata);
    appendText(state, type, value, size);

    return state.valid ? 0 : 1;
}

} // END OF NAMESPACE anonymous

auto mts::markdown::parse(std::string_view source) -> Document {
    if (source.size() > std::numeric_limits<MD_SIZE>::max()) {
        Document document;
        document.valid = false;

        return document;
    }

    ParseState state;
    MD_PARSER parser{};
    parser.abi_version = 0;
    parser.flags = MD_DIALECT_GITHUB | MD_FLAG_NOHTML;
    parser.enter_block = enterBlock;
    parser.leave_block = leaveBlock;
    parser.enter_span = enterSpan;
    parser.leave_span = leaveSpan;
    parser.text = text;

    int const result = md_parse(source.data(), static_cast<MD_SIZE>(source.size()), &parser, &state);

    Document document;
    document.root = std::move(state.root);
    document.valid = result == 0 && state.valid && state.stack.size() == 1;

    return document;
}
