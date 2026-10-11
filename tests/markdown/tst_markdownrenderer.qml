import QtQuick
import QtQuick.Window
import QtTest

TestCase {
    id: testCase

    name: "MarkdownRenderer"
    when: windowShown
    property int streamTickCount: 0
    property int heartbeatCount: 0
    width: 800
    height: 600

    ListModel {
        id: conversationModel
    }

    QtObject {
        id: fakeChat
        property var conversation: conversationModel
        property bool running: false
    }

    QtObject {
        id: fakeImages
        property int requests: 0
        property string lastSource: ""

        function cachedImageUrl(source) { return "" }
        function cachedImageSize(source) { return Qt.size(0, 0) }
        function requestImage(source) { requests += 1; lastSource = source; return "" }
        function releaseImage(source) {}

        signal imageReady(string source, string url, bool animated, size dimensions)
        signal imageFailed(string source, string reason)
    }

    QtObject {
        id: fakeParser

        function parse(source) {
            return [{kind: "paragraph", children: [{kind: "text", text: source}]}]
        }
    }

    Timer {
        id: streamTimer
        interval: 2
        repeat: true

        onTriggered: {
            if (testCase.streamTickCount >= 50) {
                stop()
                return
            }

            testCase.streamTickCount += 1
            conversationModel.setProperty(0, "text", conversationModel.get(0).text + " token")
        }
    }

    Timer {
        id: heartbeatTimer
        interval: 5
        repeat: true
        onTriggered: testCase.heartbeatCount += 1
    }

    Window {
        id: rendererWindow
        width: 640
        height: 360
        visible: true
    }

    Loader {
        id: codeLoader
        Component.onCompleted: setSource(Qt.resolvedUrl("../../src/qml/views/CodeBlock.qml"), {
            width: 600,
            code: "int main() {\n    return 0;\n}",
            language: "cpp"
        })
    }

    Loader {
        id: markdownNodeLoader
        Component.onCompleted: setSource(Qt.resolvedUrl("../../src/qml/views/MarkdownNode.qml"), {
            width: 600,
            node: ({
                kind: "paragraph",
                children: [
                    {kind: "text", text: "A "},
                    {kind: "strong", children: [{kind: "text", text: "formatted"}]},
                    {kind: "text", text: " paragraph"}
                ]
            })
        })
    }

    Loader {
        id: tableLoader
        Component.onCompleted: setSource(Qt.resolvedUrl("../../src/qml/views/MarkdownNode.qml"), {
            width: 600,
            node: ({
                kind: "table",
                children: [
                    {kind: "tableHead", children: [{kind: "tableRow", children: [
                        {kind: "tableHeaderCell", alignment: 0, children: [{kind: "text", text: "A"}]},
                        {kind: "tableHeaderCell", alignment: 0, children: [{kind: "text", text: "B"}]}
                    ]}]},
                    {kind: "tableBody", children: [{kind: "tableRow", children: [
                        {kind: "tableCell", alignment: 0, children: [{kind: "text", text: "1"}]},
                        {kind: "tableCell", alignment: 0, children: [{kind: "text", text: "2"}]}
                    ]}]}
                ]
            })
        })
    }

    Loader {
        id: imageParagraphLoader
        Component.onCompleted: setSource(Qt.resolvedUrl("../../src/qml/views/MarkdownNode.qml"), {
            width: 600,
            node: ({kind: "paragraph", children: [
                {kind: "text", text: "before"},
                {kind: "image", destination: "https://example.invalid/image.png", text: "example image", title: ""},
                {kind: "text", text: "after"}
            ]}),
            imageLoader: fakeImages
        })
    }

    function test_toolCallsHideArgumentsAndRenderJsonResultsAsCode() {
        const component = Qt.createComponent(Qt.resolvedUrl("../../src/qml/views/ConversationBlock.qml"))
        compare(component.status, Component.Ready)

        const arguments = '{"query":"private arguments should not be shown"}'
        const call = component.createObject(rendererWindow, {
            index: 0,
            kind: 3,
            text: "graphql",
            payload: arguments
        })
        verify(call)
        const callText = findChild(call, "conversationPlainText")
        verify(callText)
        compare(callText.text, "Used tool: graphql")
        verify(!callText.text.includes("private arguments"))

        const directJson = '{"status":"ok","output":"value"}'
        const result = component.createObject(rendererWindow, {
            index: 1,
            kind: 4,
            text: "graphql",
            payload: directJson
        })
        verify(result)
        verify(result.hasJsonToolResult)
        const resultCode = findChild(result, "markdownCodeText")
        verify(resultCode)
        verify(resultCode.text.startsWith("{"))
        verify(resultCode.text.includes('"status": "ok"'))

        const escapedResult = component.createObject(rendererWindow, {
            index: 2,
            kind: 4,
            text: "graphql",
            payload: JSON.stringify(directJson)
        })
        verify(escapedResult)
        verify(escapedResult.hasJsonToolResult)
        const escapedCode = findChild(escapedResult, "markdownCodeText")
        verify(escapedCode)
        verify(escapedCode.text.includes('"output": "value"'))

        const largeResult = component.createObject(rendererWindow, {
            index: 3,
            kind: 4,
            text: "graphql",
            payload: JSON.stringify({data: Array(100).fill("a long tool result value")})
        })
        verify(largeResult)
        const truncatedCode = findChild(largeResult, "markdownCodeText")
        verify(truncatedCode)
        verify(truncatedCode.text.endsWith("…"))
        verify(truncatedCode.text.length <= largeResult.toolResultCodeLimit + 2)

        const plainResult = component.createObject(rendererWindow, {
            index: 4,
            kind: 4,
            text: "graphql",
            payload: "service unavailable"
        })
        verify(plainResult)
        verify(!plainResult.hasJsonToolResult)
        const plainText = findChild(plainResult, "conversationPlainText")
        verify(plainText)
        compare(plainText.text, "Result: service unavailable")

        call.destroy()
        result.destroy()
        escapedResult.destroy()
        largeResult.destroy()
        plainResult.destroy()
    }

    function test_qmlComponentsLoad() {
        var codeComponent = Qt.createComponent(Qt.resolvedUrl("../../src/qml/views/CodeBlock.qml"))
        if (codeComponent.status !== Component.Ready) {
            console.log(codeComponent.errorString())
        }
        compare(codeComponent.status, Component.Ready)
        var markdownComponent = Qt.createComponent(Qt.resolvedUrl("../../src/qml/views/MarkdownNode.qml"))
        if (markdownComponent.status !== Component.Ready) {
            console.log(markdownComponent.errorString())
        }
        compare(markdownComponent.status, Component.Ready)
    }

    function test_codeBlockHasContentHeight() {
        tryVerify(function() { return codeLoader.item && codeLoader.item.implicitHeight > 30 })
        compare(codeLoader.item.language, "cpp")
        const codeText = findChild(codeLoader.item, "markdownCodeText")
        verify(codeText)
        compare(codeText.text, "int main() {\n    return 0;\n}")
    }

    function test_codeWrappingCanToggleAndCopyControlExists() {
        const wrapButton = findChild(codeLoader.item, "codeWrapButton")
        const copyButton = findChild(codeLoader.item, "codeCopyButton")
        verify(wrapButton)
        verify(copyButton)
        compare(codeLoader.item.wordWrap, true)
        wrapButton.activate()
        compare(codeLoader.item.wordWrap, false)
        wrapButton.activate()
        compare(codeLoader.item.wordWrap, true)
    }

    function test_structuredInlineContentHasContentHeight() {
        tryVerify(function() { return markdownNodeLoader.item && markdownNodeLoader.item.implicitHeight > 0 })
    }

    function test_tableCellsHaveContentHeight() {
        tryVerify(function() { return tableLoader.item && tableLoader.item.implicitHeight > 0 })
        const cell = findChild(tableLoader.item, "markdownRichText")
        verify(cell)
        verify(cell.text.includes("A"))
    }

    function test_inlineImageIsRepresentedAsASeparateRenderer() {
        tryVerify(function() { return imageParagraphLoader.item && imageParagraphLoader.item.implicitHeight > 0 })
        tryVerify(function() { return fakeImages.requests > 0 })
        compare(fakeImages.lastSource, "https://example.invalid/image.png")
    }

    function createView() {
        const component = Qt.createComponent(Qt.resolvedUrl("../../src/qml/views/ConversationView.qml"))
        compare(component.status, Component.Ready)

        return component.createObject(rendererWindow, {
            x: 0,
            y: 0,
            width: rendererWindow.width,
            height: rendererWindow.height,
            columnWidth: 600,
            chatController: fakeChat,
            markdownParser: fakeParser,
            stickToBottom: true
        })
    }

    function populate(count) {
        conversationModel.clear()
        for (let i = 0; i < count; ++i) {
            conversationModel.append({kind: 0, text: "message " + i + " " + ("content ".repeat(12)), payload: ""})
        }
    }

    function test_repeaterFollowsTheTailWhenEnabled() {
        const view = createView()
        verify(view)
        populate(20)
        view.snapToBottom()
        wait(50)
        tryVerify(function() { return view.contentHeight > view.viewportHeight })
        tryVerify(function() { return view.atBottom() })
        conversationModel.append({kind: 0, text: "new tail message", payload: ""})
        tryVerify(function() { return view.atBottom() })
        view.destroy()
    }

    function test_scrollingAreaCanScrollAndFollowTheTail() {
        const view = createView()
        verify(view)
        populate(20)
        tryVerify(function() { return view.contentHeight > view.viewportHeight })
        view.snapToBottom()
        tryVerify(function() { return view.atBottom() })

        const previousY = view.contentY
        view.scrollBy(-80)
        verify(view.contentY < previousY)
        conversationModel.append({kind: 0, text: "new tail message", payload: ""})
        tryVerify(function() { return view.atBottom() })
        view.destroy()
    }

    function test_expandControlLivesInTheConversationBar() {
        const view = createView()
        verify(view)
        const expand = findChild(view, "conversationExpandButton")
        verify(expand)
        compare(expand.visible, true)
        compare(expand.enabled, true)

        let requested = 0
        view.expandRequested.connect(function() { requested += 1 })
        expand.activate()
        compare(requested, 1)

        view.expandVisible = false
        tryVerify(function() { return !expand.visible })
        view.destroy()
    }

    function test_reasoningVisibilityToggle() {
        const view = createView()
        const toggle = findChild(view, "reasoningButton")
        verify(toggle)
        compare(view.reasoningVisible, true)
        toggle.activate()
        compare(view.reasoningVisible, false)
        toggle.activate()
        compare(view.reasoningVisible, true)
        view.destroy()
    }

    function test_streamedDeltasKeepTheEventLoopResponsive() {
        const view = createView()
        verify(view)
        populate(0)
        conversationModel.append({kind: 1, text: "stream start", payload: ""})
        fakeChat.running = true
        testCase.streamTickCount = 0
        testCase.heartbeatCount = 0
        heartbeatTimer.start()
        streamTimer.start()

        tryVerify(function() { return testCase.streamTickCount === 50 }, 5000)
        heartbeatTimer.stop()
        verify(testCase.heartbeatCount > 0)
        fakeChat.running = false
        wait(100)
        tryVerify(function() { return view.atBottom() })
        view.destroy()
    }


}
