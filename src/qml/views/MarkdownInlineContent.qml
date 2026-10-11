import QtQuick
import Lib.Theme

Column {
    id: control

    required property var nodes
    property var imageLoader: null
    property var customBlockRenderers: ({})
    property var scrollController: null
    property int imageRevision: 0
    property color contentColor: Theme.color.text
    property color mutedColor: Theme.color.textMuted
    property bool renderEnabled: true
    property var segments: []

    spacing: 2

    function copyWithChildren(node, children) {
        const copy = {}
        for (const key in node) {
            copy[key] = node[key]
        }
        copy.children = children
        return copy
    }

    function expandImageNodes(node) {
        if (node.kind === "image") {
            return [node]
        }

        const children = node.children || []
        if (!control.containsImage(children)) {
            return [node]
        }

        const expandedChildren = []
        for (let i = 0; i < children.length; ++i) {
            const expanded = control.expandImageNodes(children[i])
            for (let j = 0; j < expanded.length; ++j) {
                expandedChildren.push(expanded[j])
            }
        }

        if (node.kind !== "emphasis" && node.kind !== "strong" && node.kind !== "strikethrough" && node.kind !== "link") {
            return expandedChildren
        }

        const result = []
        let group = []
        for (let i = 0; i < expandedChildren.length; ++i) {
            const child = expandedChildren[i]
            if (child.kind === "image") {
                if (group.length > 0) {
                    result.push(control.copyWithChildren(node, group))
                    group = []
                }
                result.push(child)
            } else {
                group.push(child)
            }
        }
        if (group.length > 0) {
            result.push(control.copyWithChildren(node, group))
        }
        return result
    }

    function containsImage(nodes) {
        for (let i = 0; i < nodes.length; ++i) {
            if (nodes[i].kind === "image" || control.containsImage(nodes[i].children || [])) {
                return true
            }
        }
        return false
    }

    function splitImages(sourceNodes) {
        const result = []
        let textNodes = []
        for (let i = 0; i < sourceNodes.length; ++i) {
            const expanded = control.expandImageNodes(sourceNodes[i])
            for (let j = 0; j < expanded.length; ++j) {
                const item = expanded[j]
                if (item.kind === "image") {
                    if (textNodes.length > 0) {
                        result.push({kind: "paragraph", children: textNodes})
                        textNodes = []
                    }
                    result.push(item)
                } else {
                    textNodes.push(item)
                }
            }
        }
        if (textNodes.length > 0) {
            result.push({kind: "paragraph", children: textNodes})
        }
        return result
    }

    function rebuild() {
        control.segments = control.splitImages(control.nodes || [])
    }

    Component.onCompleted: rebuild()
    onNodesChanged: rebuild()

    Repeater {
        model: control.segments

        delegate: Loader {
            id: segmentLoader
            property var segment: modelData
            property int segmentIndex: index
            width: control.width
            sourceComponent: segmentLoader.segment.kind === "image" ? imageSegment : textSegment

            Component {
                id: imageSegment

                MarkdownImage {
                    width: Math.min(control.width, implicitWidth)
                    sourceUrl: segmentLoader.segment.destination
                    altText: segmentLoader.segment.text
                    title: segmentLoader.segment.title
                    maximumWidth: control.width
                    imageLoader: control.imageLoader
                    renderEnabled: control.renderEnabled
                }
            }

            Component {
                id: textSegment

                MarkdownNodeDelegate {
                    width: control.width
                    modelData: segmentLoader.segment
                    index: segmentLoader.segmentIndex
                    imageLoader: control.imageLoader
                    customBlockRenderers: control.customBlockRenderers
                    scrollController: control.scrollController
                    imageRevision: control.imageRevision
                    parentKind: "paragraph"
                    contentColor: control.contentColor
                    mutedColor: control.mutedColor
                }
            }
        }
    }
}
