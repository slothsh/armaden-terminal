#include <catch2/catch_test_macros.hpp>

import std;
import markdown;

using namespace mts::markdown;

namespace {

auto findNode(Node const& root, NodeKind kind) -> Node const* {
    if (root.kind == kind) {
        return &root;
    }

    for (Node const& child : root.children) {
        if (Node const* const found = findNode(child, kind); found != nullptr) {
            return found;
        }
    }

    return nullptr;
}

auto collectNodes(Node const& root, NodeKind kind) -> std::vector<Node const*> {
    std::vector<Node const*> result;
    if (root.kind == kind) {
        result.push_back(&root);
    }

    for (Node const& child : root.children) {
        std::vector<Node const*> const descendants = collectNodes(child, kind);
        result.insert(result.end(), descendants.begin(), descendants.end());
    }

    return result;
}

auto collectText(Node const& root) -> std::string {
    std::string result = root.text;
    for (Node const& child : root.children) {
        result += collectText(child);
    }

    return result;
}

}

TEST_CASE("Markdown parser preserves block and inline structure", "[markdown]") {
    Document const document = parse("# Heading\n\nA **strong** and *emphasized* [link](https://example.org).\n");

    REQUIRE(document.valid);
    REQUIRE(document.root.kind == NodeKind::Document);
    REQUIRE(findNode(document.root, NodeKind::Heading) != nullptr);
    REQUIRE(findNode(document.root, NodeKind::Strong) != nullptr);
    REQUIRE(findNode(document.root, NodeKind::Emphasis) != nullptr);

    Node const* const link = findNode(document.root, NodeKind::Link);
    REQUIRE(link != nullptr);
    REQUIRE(link->destination == "https://example.org");
}

TEST_CASE("Markdown parser extracts fenced code language and exact content", "[markdown]") {
    std::string const source = "```cpp\nint main() {\n    return 0;\n}\n```\n";
    Document const document = parse(source);

    REQUIRE(document.valid);
    Node const* const code = findNode(document.root, NodeKind::CodeBlock);
    REQUIRE(code != nullptr);
    REQUIRE(code->language == "cpp");
    REQUIRE(code->languageIdentifierValid);
    REQUIRE(code->info == "cpp");
    REQUIRE(code->marker == '`');
    REQUIRE(code->text == "int main() {\n    return 0;\n}\n");
}

TEST_CASE("Markdown parser captures GFM tables and task list state", "[markdown]") {
    Document const document = parse(
        "| Name | Count |\n"
        "|:-----|------:|\n"
        "| map | 3 |\n\n"
        "- [x] done\n"
        "- [ ] pending\n"
    );

    REQUIRE(document.valid);
    REQUIRE(findNode(document.root, NodeKind::Table) != nullptr);

    std::vector<Node const*> const headers = collectNodes(document.root, NodeKind::TableHeaderCell);
    REQUIRE(headers.size() == 2);
    REQUIRE(headers[0]->alignment == 1);
    REQUIRE(headers[1]->alignment == 3);

    std::vector<Node const*> const items = collectNodes(document.root, NodeKind::ListItem);
    REQUIRE(items.size() == 2);
    REQUIRE(items[0]->task);
    REQUIRE(items[0]->checked);
    REQUIRE(items[1]->task);
    REQUIRE_FALSE(items[1]->checked);
}

TEST_CASE("Markdown parser extracts image source title and alt text", "[markdown]") {
    Document const document = parse("![A *map* image](https://example.org/map.png \"Map\")");

    REQUIRE(document.valid);
    Node const* const image = findNode(document.root, NodeKind::Image);
    REQUIRE(image != nullptr);
    REQUIRE(image->destination == "https://example.org/map.png");
    REQUIRE(image->title == "Map");
    REQUIRE(image->text == "A map image");
}

TEST_CASE("Markdown parser treats incomplete syntax as valid literal content", "[markdown]") {
    Document const document = parse("Open **strong and `code and [link](");

    REQUIRE(document.valid);
    REQUIRE(collectText(document.root).find("Open") != std::string::npos);
}

TEST_CASE("Markdown parser emits no raw HTML nodes", "[markdown]") {
    Document const document = parse("Before <script>alert(1)</script> after");

    REQUIRE(document.valid);
    REQUIRE(findNode(document.root, NodeKind::RawHtml) == nullptr);
}
