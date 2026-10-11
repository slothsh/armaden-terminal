export module markdown:model;

import std;

export namespace mts::markdown {

enum class NodeKind {
    Document,
    Paragraph,
    Heading,
    BlockQuote,
    UnorderedList,
    OrderedList,
    ListItem,
    Table,
    TableHead,
    TableBody,
    TableRow,
    TableHeaderCell,
    TableCell,
    CodeBlock,
    HorizontalRule,
    RawHtml,
    CustomBlock,
    Text,
    Emphasis,
    Strong,
    Link,
    Image,
    InlineCode,
    Strikethrough,
    HardBreak,
    SoftBreak,
    Entity,
};

struct Node {
    NodeKind kind = NodeKind::Document;
    std::string text;
    std::string destination;
    std::string title;
    std::string info;
    std::string language;
    std::string renderer;
    int level = 0;
    int start = 1;
    int alignment = 0;
    char marker = '\0';
    bool tight = false;
    bool task = false;
    bool languageIdentifierValid = false;
    bool checked = false;
    std::vector<Node> children;
};

struct Document {
    Node root;
    bool valid = true;
};

[[nodiscard]] auto parse(std::string_view source) -> Document;

} // END OF NAMESPACE `mts::markdown`
