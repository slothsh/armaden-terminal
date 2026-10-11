#include "markdown_parser.hpp"

#include <QVariantMap>

import std;
import markdown;

namespace {

auto kindName(mts::markdown::NodeKind kind) -> QString {
    using mts::markdown::NodeKind;

    switch (kind) {
        case NodeKind::Document: return QStringLiteral("document");
        case NodeKind::Paragraph: return QStringLiteral("paragraph");
        case NodeKind::Heading: return QStringLiteral("heading");
        case NodeKind::BlockQuote: return QStringLiteral("blockquote");
        case NodeKind::UnorderedList: return QStringLiteral("unorderedList");
        case NodeKind::OrderedList: return QStringLiteral("orderedList");
        case NodeKind::ListItem: return QStringLiteral("listItem");
        case NodeKind::Table: return QStringLiteral("table");
        case NodeKind::TableHead: return QStringLiteral("tableHead");
        case NodeKind::TableBody: return QStringLiteral("tableBody");
        case NodeKind::TableRow: return QStringLiteral("tableRow");
        case NodeKind::TableHeaderCell: return QStringLiteral("tableHeaderCell");
        case NodeKind::TableCell: return QStringLiteral("tableCell");
        case NodeKind::CodeBlock: return QStringLiteral("codeBlock");
        case NodeKind::HorizontalRule: return QStringLiteral("horizontalRule");
        case NodeKind::RawHtml: return QStringLiteral("rawHtml");
        case NodeKind::CustomBlock: return QStringLiteral("customBlock");
        case NodeKind::Text: return QStringLiteral("text");
        case NodeKind::Emphasis: return QStringLiteral("emphasis");
        case NodeKind::Strong: return QStringLiteral("strong");
        case NodeKind::Link: return QStringLiteral("link");
        case NodeKind::Image: return QStringLiteral("image");
        case NodeKind::InlineCode: return QStringLiteral("inlineCode");
        case NodeKind::Strikethrough: return QStringLiteral("strikethrough");
        case NodeKind::HardBreak: return QStringLiteral("hardBreak");
        case NodeKind::SoftBreak: return QStringLiteral("softBreak");
        case NodeKind::Entity: return QStringLiteral("entity");
    }

    return QStringLiteral("unknown");
}

auto toMap(mts::markdown::Node const& node) -> QVariantMap {
    QVariantMap result;
    result.insert(QStringLiteral("kind"), kindName(node.kind));
    result.insert(QStringLiteral("text"), QString::fromStdString(node.text));
    result.insert(QStringLiteral("destination"), QString::fromStdString(node.destination));
    result.insert(QStringLiteral("title"), QString::fromStdString(node.title));
    result.insert(QStringLiteral("info"), QString::fromStdString(node.info));
    result.insert(QStringLiteral("language"), QString::fromStdString(node.language));
    result.insert(QStringLiteral("renderer"), QString::fromStdString(node.renderer));
    result.insert(QStringLiteral("level"), node.level);
    result.insert(QStringLiteral("start"), node.start);
    result.insert(QStringLiteral("alignment"), node.alignment);
    result.insert(QStringLiteral("marker"), QChar::fromLatin1(node.marker));
    result.insert(QStringLiteral("tight"), node.tight);
    result.insert(QStringLiteral("task"), node.task);
    result.insert(QStringLiteral("checked"), node.checked);
    result.insert(QStringLiteral("languageIdentifierValid"), node.languageIdentifierValid);

    QVariantList children;
    children.reserve(static_cast<qsizetype>(node.children.size()));
    for (mts::markdown::Node const& child : node.children) {
        children.append(toMap(child));
    }
    result.insert(QStringLiteral("children"), children);

    return result;
}

}

MarkdownParser::MarkdownParser(QObject* parent)
    : QObject(parent) {}

auto MarkdownParser::parse(QString const& source) const -> QVariantList {
    mts::markdown::Document const document = mts::markdown::parse(source.toStdString());
    QVariantList result;
    result.reserve(static_cast<qsizetype>(document.root.children.size()));

    for (mts::markdown::Node const& node : document.root.children) {
        result.append(toMap(node));
    }

    return result;
}
