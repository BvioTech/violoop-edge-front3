pragma Singleton
import QtQuick

// Thin wrapper around AppShell's single StackView so any page can navigate
// without AppShell having to pre-declare a Component{} for every route
// (there are ~30 of them). AppShell.qml sets `Nav.view = stackView` once at
// startup; every other page just calls `Nav.push("SomePage.qml")` /
// `Nav.pop()`. `push` takes a plain file name (all pages live flat in
// qml/, see app/docs/progress.md for why), passed straight through to
// StackView.push() - NOT wrapped in Qt.resolvedUrl(). That wrapping was
// tried first and silently broke StackView's initialItem the same way
// (depth stayed 0, nothing pushed, no error of any kind) until it was
// changed to a bare string - see AppShell.qml's rotatedRoot comment for
// the sibling bug this was found alongside. Not fully root-caused (likely
// a QUrl-vs-string handling quirk in this Qt build's StackView), but the
// empirical fix is consistent: pass plain strings, let StackView resolve
// them itself.
QtObject {
    property var view

    function push(fileName, props) {
        if (!view)
            return
        view.push(fileName, props || {})
    }

    function pop() {
        if (view && view.depth > 1)
            view.pop()
    }

    function popToHome() {
        if (view)
            view.pop(null)
    }
}
