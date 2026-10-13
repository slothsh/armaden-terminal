import QtQuick
import QtQuick.Window
import QtTest
import Lib.Controls

TestCase {
    id: testCase

    name: "MarkdownRenderer"
    when: windowShown
    property int streamTickCount: 0
    property int heartbeatCount: 0
    property var promptInputUnderTest: null
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

    Component {
        id: promptInputComponent

        TextInput {
            width: 480
            height: 120
        }
    }

    SignalSpy {
        id: promptAcceptedSpy
        target: testCase.promptInputUnderTest
        signalName: "accepted"
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
        const conversationTextCursor = findChild(call, "conversationTextCursor")
        verify(callText)
        verify(conversationTextCursor)
        compare(conversationTextCursor.cursorShape, Qt.IBeamCursor)
        compare(callText.text, "Used tool: graphql")
        verify(!callText.text.includes("private arguments"))

        const returnedContent = JSON.stringify({ok: true, result: {data: {players: []}}})
        const messageEnvelope = JSON.stringify({
            content: returnedContent,
            content_blocks: [{text: returnedContent, type: "text"}],
            name: "command_graphql",
            role: "tool",
            tool_call_id: "call_LhEWjzKQBZksHl7xKjQWMMyO",
            type: "tool",
            usage_metadata: {}
        })
        const result = component.createObject(rendererWindow, {
            index: 1,
            kind: 4,
            text: "command_graphql",
            payload: messageEnvelope
        })
        verify(result)
        verify(result.hasJsonToolResult)
        const resultCode = findChild(result, "markdownCodeText")
        verify(resultCode)
        verify(resultCode.text.startsWith("{"))
        verify(resultCode.text.includes('"ok": true'))
        verify(resultCode.text.includes('"players": []'))
        verify(!resultCode.text.includes("tool_call_id"))
        verify(!resultCode.text.includes("content_blocks"))
        verify(!resultCode.text.includes("usage_metadata"))
        verify(!resultCode.text.includes("role"))

        const requestObject = JSON.stringify({method: "POST", variables: {message: "hello"}})
        const responseObject = JSON.stringify({data: {sent: true}, metadata: "hidden from preview"})
        const envelope = JSON.stringify({
            request: requestObject,
            response: responseObject,
            internals: {secret: "not for display"}
        })
        const escapedResult = component.createObject(rendererWindow, {
            index: 2,
            kind: 4,
            text: "graphql",
            payload: JSON.stringify(envelope)
        })
        verify(escapedResult)
        verify(escapedResult.hasJsonToolResult)
        const escapedCode = findChild(escapedResult, "markdownCodeText")
        verify(escapedCode)
        verify(escapedCode.text.includes('"method": "POST"'))
        verify(escapedCode.text.includes('"sent": true'))
        verify(!escapedCode.text.includes('"internals"'))

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
            payload: JSON.stringify({
                content: "service unavailable",
                name: "command_graphql",
                role: "tool",
                tool_call_id: "call_plain",
                type: "tool"
            })
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

    function test_structuredBlocksDoNotInstantiateRawText() {
        const component = Qt.createComponent(Qt.resolvedUrl("../../src/qml/views/ConversationBlock.qml"))
        compare(component.status, Component.Ready)

        const markdownBlock = component.createObject(rendererWindow, {
            index: 0,
            kind: 1,
            text: "# Heading\n\nBody copy",
            payload: ""
        })
        verify(markdownBlock)
        verify(markdownBlock.markdownContent)
        verify(!findChild(markdownBlock, "conversationPlainText"))

        const jsonBlock = component.createObject(rendererWindow, {
            index: 1,
            kind: 4,
            text: "graphql",
            payload: JSON.stringify({
                content: JSON.stringify({ok: true}),
                role: "tool"
            })
        })
        verify(jsonBlock)
        verify(jsonBlock.hasJsonToolResult)
        verify(!findChild(jsonBlock, "conversationPlainText"))

        const plainBlock = component.createObject(rendererWindow, {
            index: 2,
            kind: 0,
            text: "plain user text",
            payload: ""
        })
        verify(plainBlock)
        verify(plainBlock.plainContent)
        const plainText = findChild(plainBlock, "conversationPlainText")
        verify(plainText)
        compare(plainText.text, "plain user text")

        markdownBlock.destroy()
        jsonBlock.destroy()
        plainBlock.destroy()
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

    function test_ctrlEnterActivatesPromptAndPlainEnterIsUnhandled() {
        testCase.promptInputUnderTest = promptInputComponent.createObject(testCase)
        verify(testCase.promptInputUnderTest)

        compare(testCase.promptInputUnderTest.handleKeyPress(Qt.Key_Return, Qt.NoModifier), false)
        compare(promptAcceptedSpy.count, 0)
        compare(testCase.promptInputUnderTest.handleKeyPress(Qt.Key_Return, Qt.ControlModifier), true)
        compare(promptAcceptedSpy.count, 1)
        compare(testCase.promptInputUnderTest.handleKeyPress(Qt.Key_Enter, Qt.ControlModifier), true)
        compare(promptAcceptedSpy.count, 2)

        testCase.promptInputUnderTest.destroy()
        testCase.promptInputUnderTest = null
        promptAcceptedSpy.clear()
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
        compare(wrapButton.active, false)
        compare(copyButton.active, false)
        verify(!wrapButton.activeFocus)
        verify(!copyButton.activeFocus)

        const wrapCursor = findChild(wrapButton, "codeWrapPointerCursor")
        const copyCursor = findChild(copyButton, "codeCopyPointerCursor")
        const textCursor = findChild(codeLoader.item, "codeTextCursor")
        verify(wrapCursor)
        verify(copyCursor)
        verify(textCursor)
        compare(wrapCursor.cursorShape, Qt.PointingHandCursor)
        compare(copyCursor.cursorShape, Qt.PointingHandCursor)
        compare(textCursor.cursorShape, Qt.IBeamCursor)

        verify(!wrapButton.hovered)
        verify(!copyButton.hovered)
        verify(!wrapCursor.hovered)
        verify(!wrapButton.activeFocus)

        wrapButton.activate()
        compare(codeLoader.item.wordWrap, false)
        compare(wrapButton.active, false)
        wrapButton.activate()
        compare(codeLoader.item.wordWrap, true)
        compare(wrapButton.active, false)

        const component = Qt.createComponent(Qt.resolvedUrl("../../src/qml/views/CodeBlock.qml"))
        const longCodeBlock = component.createObject(rendererWindow, {
            width: 320,
            code: Array(100).fill("long code line").join("\n"),
            language: "json"
        })
        verify(longCodeBlock)
        const codeViewport = findChild(longCodeBlock, "codeViewport")
        verify(codeViewport)
        tryVerify(function() { return codeViewport.contentHeight > codeViewport.height })
        verify(longCodeBlock.codeOverflows)
        verify(!longCodeBlock.scrollEnabled)
        verify(!codeViewport.interactive)
        longCodeBlock.interactionScrollRequested(-1)
        compare(codeViewport.contentY, 0)
        longCodeBlock.destroy()
    }

    function test_codeBlockScrollsHorizontallyWhenWrapDisabled() {
        const component = Qt.createComponent(Qt.resolvedUrl("../../src/qml/views/CodeBlock.qml"))
        compare(component.status, Component.Ready)

        const block = component.createObject(rendererWindow, {
            width: 200,
            wordWrap: false,
            code: "const value = \"" + "a".repeat(400) + "\";",
            language: "js"
        })
        verify(block)
        const viewport = findChild(block, "codeViewport")
        verify(viewport)
        tryVerify(function() { return viewport.contentWidth > viewport.viewportWidth })
        verify(block.codeOverflows)

        block.wordWrap = true
        tryVerify(function() { return viewport.contentWidth <= viewport.viewportWidth + 1 })
        block.destroy()
    }

    function test_structuredInlineContentHasContentHeight() {
        tryVerify(function() { return markdownNodeLoader.item && markdownNodeLoader.item.implicitHeight > 0 })
        const textCursor = findChild(markdownNodeLoader.item, "markdownRichTextCursor")
        verify(textCursor)
        compare(textCursor.cursorShape, Qt.IBeamCursor)
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

    function test_messageTextWheelHandlerScrollsTheTranscript() {
        const view = createView()
        verify(view)
        populate(20)
        const scrollArea = findChild(view, "conversationScrollArea")
        const block = findChild(view, "conversationBlock")
        verify(scrollArea)
        verify(block)
        tryVerify(function() { return view.contentHeight > view.viewportHeight })
        view.stickToBottom = false
        scrollArea.contentY = 0
        const previousY = view.contentY
        const wheel = {
            pixelDelta: {y: 0},
            angleDelta: {y: -120},
            accepted: false
        }
        block.scrollTextWheel(wheel)
        verify(wheel.accepted)
        verify(view.contentY > previousY)
        view.destroy()
    }

    function test_scrollingAreaCanScrollAndFollowTheTail() {
        const view = createView()
        verify(view)
        const scrollArea = findChild(view, "conversationScrollArea")
        verify(scrollArea)
        populate(20)
        tryVerify(function() { return view.contentHeight > view.viewportHeight })
        view.stickToBottom = false
        scrollArea.contentY = 0
        const beforeWheelY = view.contentY
        const wheel = {
            pixelDelta: {y: 0},
            angleDelta: {y: -120},
            accepted: false
        }
        scrollArea.handleWheel(wheel)
        verify(wheel.accepted)
        verify(view.contentY > beforeWheelY)
        scrollArea.contentY = 0
        scrollArea.interactionScrollRequested(-1)
        verify(view.contentY > 0)
        view.stickToBottom = true
        view.snapToBottom()
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
