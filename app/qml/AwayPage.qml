import QtQuick
import QtQuick.Controls
import QtQuick3D
import QtQuick3D.AssetUtils

// Away/standby screen: continuously renders a glTF skeletal (skinned)
// animation via Quick3D's RuntimeLoader, the approach qt3d-spike spent
// several real-hardware rounds validating (see
// ../../qt3d-spike/docs/findings.md and ../../qt3d-spike/runtime_loader.qml,
// which this page's View3D/camera/light setup mirrors). Unlike edge-front2's
// AwayPage (lib/pages/away/away_page.dart), which composites a pre-baked
// MP4 over a poster image, this renders the animation live - that's the
// whole reason this module links Qt Quick3D (see ../../README.md's licensing
// note: GPL-3 because of it) instead of playing a video file.
//
// RuntimeLoader auto-plays whatever animation is baked into the glTF: its
// generated QtQuickTimeline TimelineAnimation defaults to `running: true`
// and `loops: Animation.Infinite` (confirmed by reading
// qtquick3d/src/assetutils/qssgqmlutilities.cpp), so no Timer/loop wiring
// is needed to keep it playing. What it does NOT do is close the loop:
// glTF keyframes rarely start at t=0, and the timeline fills that gap by
// blending in from the rest pose on every wrap-around (a twitch once per
// cycle). timelineLoopCloser (core/TimelineLoopCloser.h) patches the
// generated timeline once the model has loaded - see that header.
//
// Asset: assets/away/VIO_DesktopPet.glb, the designer's mascot model
// (tracked via git-lfs, see ../../.gitattributes, and listed in
// CMakeLists.txt's RESOURCES). Unlike the CesiumMan placeholder it
// replaced (one walk cycle), it carries 19 clips named by event
// ("idle.default", "welcome", "touch.left", ...). RuntimeLoader only ever
// shows the first one by itself; this page can step through all of them
// (timelineLoopCloser.playClip) so each can be looked at on the device.
// Which event plays which clip is not wired up - see
// ../docs/user-model-spec.md.
Item {
    id: root

    // Orbit camera: the point of this page is to show the animation is
    // rendered live from a rigged 3D model rather than played back from a
    // video (what edge-front2 does), so the viewer can drag to orbit the
    // camera around the character, pinch to zoom, and pause the animation
    // mid-stride and keep orbiting the frozen pose. The defaults below
    // reproduce the original fixed camera exactly (position (0, 1.2, 3.5),
    // pitched -10 degrees): that line of sight crosses the character's
    // axis (x = z = 0) at y = 1.2 - 3.5 * tan(10deg) ~= 0.58, which is the
    // orbit pivot, 3.5 / cos(10deg) ~= 3.55 away. Those two numbers were
    // tuned for CesiumMan and are now only what the camera uses until the
    // model has loaded; from then on they follow the model's own bounds
    // (modelMin/modelMax, set on load).
    readonly property real defaultYaw: 0
    readonly property real defaultPitch: -10
    property vector3d modelMin: Qt.vector3d(0, 0, 0)
    property vector3d modelMax: Qt.vector3d(0, 0, 0)
    readonly property vector3d modelSize: modelMax.minus(modelMin)
    readonly property bool modelMeasured: modelSize.x > 0 && modelSize.y > 0

    // Orbit around the bounds' centre.
    readonly property vector3d pivot: modelMeasured ? modelMin.plus(modelMax).times(0.5) : Qt.vector3d(0, 0.58, 0)

    // Far enough back that the bounding box takes up about 1/fill of the
    // view in its tighter direction. The slack is for what the bounds
    // don't cover - they are the rest pose's, and clips move the character
    // and bring out props beside and above it. A binding rather than a
    // one-off calculation because the model can finish loading before the
    // page has been given its size.
    readonly property real defaultDistance: {
        if (!modelMeasured || view.width <= 0 || view.height <= 0)
            return 3.55
        const fill = 1.7
        const tanHalfFov = Math.tan(camera.fieldOfView * Math.PI / 360) // vertical fov
        const footprint = Math.max(modelSize.x, modelSize.z)
        const forHeight = modelSize.y / 2 / tanHalfFov
        const forWidth = footprint / 2 / (tanHalfFov * view.width / view.height)
        return Math.max(forHeight, forWidth) * fill + footprint / 2
    }
    property real yaw: defaultYaw
    property real pitch: defaultPitch
    property real distance: defaultDistance
    property bool paused: false

    // The model to show. A property so that a different file can be put
    // in without editing the loader (the user-supplied-model feature in
    // ../docs/user-model-spec.md will need that).
    property url modelSource: Qt.resolvedUrl("../assets/away/VIO_DesktopPet.glb")
    property bool cullCollapsed: true

    // Frames left in which to try measuring the freshly loaded model.
    property int framesToMeasure: 0
    Connections {
        target: root.Window.window
        enabled: root.framesToMeasure > 0
        function onFrameSwapped() {
            root.framesToMeasure = root.measureModel() ? 0 : root.framesToMeasure - 1
        }
    }

    // Sets modelMin/modelMax to the union of every mesh's own bounds,
    // placed by its node's scene transform, and returns whether there was
    // anything to measure yet: a mesh only gets its bounds once it has
    // been through a frame, until then they read as empty (which is why
    // this runs from frameSwapped below rather than on load).
    //
    // Not loader.bounds: for this model that is (-0.03, 0, 0)..(0.86, 1.1,
    // 1.022) against a real extent of +-0.82 x 0..1.5 x +-0.80, before
    // and after the first frame alike - just the span of where the prop
    // nodes sit, without the character. Skinned meshes are measured in
    // their bind pose (a skinned node's own transform doesn't apply),
    // which is where the model stands when no clip moves it.
    function measureModel() {
        let lo = Qt.vector3d(Infinity, Infinity, Infinity)
        let hi = Qt.vector3d(-Infinity, -Infinity, -Infinity)
        function visit(node) {
            if (node instanceof Model) {
                const a = node.bounds.minimum
                const b = node.bounds.maximum
                if (!a.fuzzyEquals(b)) {
                    for (let i = 0; i < 8; ++i) {
                        const corner = Qt.vector3d(i & 1 ? a.x : b.x, i & 2 ? a.y : b.y, i & 4 ? a.z : b.z)
                        const p = node.skin ? corner : node.mapPositionToScene(corner)
                        lo = Qt.vector3d(Math.min(lo.x, p.x), Math.min(lo.y, p.y), Math.min(lo.z, p.z))
                        hi = Qt.vector3d(Math.max(hi.x, p.x), Math.max(hi.y, p.y), Math.max(hi.z, p.z))
                    }
                }
            }
            for (let i = 0; i < node.children.length; ++i)
                visit(node.children[i])
        }
        visit(loader)
        if (lo.x > hi.x)
            return false
        modelMin = lo
        modelMax = hi
        console.log("AwayPage: model bounds", modelMin, modelMax)
        return true
    }

    // Stops drawing nodes that the model has scaled down to nothing.
    // glTF can't animate visibility, so this model hides whatever a clip
    // doesn't use by parking it at scale 1e-5 - the props, and 17 of its
    // 18 lamp meshes at any one time, which is 229k of its 382k triangles.
    // Quick3D still pushes all of that through the GPU every frame (dev1,
    // default view, rendered offscreen and uncapped: 34 fps with them, 47
    // without). So `visible` follows the scale for every
    // node that has meshes under it, switching back on the moment a clip
    // scales the node up again.
    function hideCollapsedNodes() {
        function visit(node) {
            let hasMesh = node instanceof Model
            for (let i = 0; i < node.children.length; ++i)
                hasMesh = visit(node.children[i]) || hasMesh
            if (hasMesh && node !== loader) {
                const update = function() {
                    const s = node.scale
                    node.visible = Math.max(Math.abs(s.x), Math.abs(s.y), Math.abs(s.z)) > 0.001
                }
                node.scaleChanged.connect(update)
                update()
            }
            return hasMesh
        }
        visit(loader)
    }

    // Clips (glTF animations) in the loaded model, and the one playing.
    property var clipNames: []
    property string currentClip: ""

    function playClip(name) {
        if (!timelineLoopCloser.playClip(loader, name))
            return
        currentClip = name
        // The clip restarts running, whatever the previous one was doing.
        paused = false
    }
    function stepClip(delta) {
        if (clipNames.length === 0)
            return
        const i = clipNames.indexOf(currentClip)
        playClip(clipNames[(i + delta + clipNames.length) % clipNames.length])
    }

    // It's a standby screen: don't leave it parked at whatever odd angle
    // the last person dragged it to. A few seconds after the last touch,
    // ease back to the default view.
    function interactionStarted() {
        returnTimer.stop()
        returnAnimation.stop()
    }
    function interactionEnded() {
        returnTimer.restart()
    }

    Timer {
        id: returnTimer
        interval: 4000
        onTriggered: {
            // yaw accumulates freely across full turns while dragging;
            // return to the nearest equivalent of the default rather than
            // unwinding every turn.
            yawReturn.to = root.defaultYaw + Math.round((root.yaw - root.defaultYaw) / 360) * 360
            returnAnimation.start()
        }
    }

    ParallelAnimation {
        id: returnAnimation
        NumberAnimation { id: yawReturn; target: root; property: "yaw"; duration: 900; easing.type: Easing.InOutCubic }
        NumberAnimation { target: root; property: "pitch"; to: root.defaultPitch; duration: 900; easing.type: Easing.InOutCubic }
        NumberAnimation { target: root; property: "distance"; to: root.defaultDistance; duration: 900; easing.type: Easing.InOutCubic }
    }

    View3D {
        id: view
        anchors.fill: parent

        environment: SceneEnvironment {
            clearColor: "#121212"
            backgroundMode: SceneEnvironment.Color
            antialiasingMode: SceneEnvironment.MSAA
            antialiasingQuality: SceneEnvironment.High
        }

        // Yaw and pitch on separate nested nodes so the orbit doesn't
        // depend on Euler-angle composition order.
        Node {
            position: root.pivot
            eulerRotation.y: root.yaw

            Node {
                eulerRotation.x: root.pitch

                PerspectiveCamera {
                    id: camera
                    z: root.distance
                    // Default clipNear (10) clips away meter-scale glTF
                    // content at this camera distance (~3.5) - same fix
                    // runtime_loader.qml needed, see that file's comment.
                    clipNear: 0.1
                    clipFar: 100
                }
            }
        }

        // Both lights stay fixed in the world (not parented to the camera
        // rig) on purpose: the shading changing as the camera orbits is
        // part of what shows this is a lit 3D scene.
        DirectionalLight {
            eulerRotation: Qt.vector3d(-45, 45, 0)
            brightness: 1.2
        }

        // Weak fill from roughly the opposite side, so the character's
        // back isn't near-black once the camera orbits behind the key
        // light.
        DirectionalLight {
            eulerRotation: Qt.vector3d(-20, -135, 0)
            brightness: 0.4
        }

        RuntimeLoader {
            id: loader
            source: root.modelSource
            onStatusChanged: {
                root.modelMin = Qt.vector3d(0, 0, 0)
                root.modelMax = Qt.vector3d(0, 0, 0)
                if (status === RuntimeLoader.Success) {
                    timelineLoopCloser.closeLoops(loader)
                    timelineLoopCloser.restoreHolds(loader)
                    root.clipNames = timelineLoopCloser.clipNames(loader)
                    // Through playClip() even for the clip RuntimeLoader
                    // already started, so the other clips' animations
                    // stop ticking in the background.
                    if (root.clipNames.length > 0)
                        root.playClip(root.clipNames[0])
                    if (root.cullCollapsed)
                        root.hideCollapsedNodes()
                    root.framesToMeasure = 30
                } else if (status === RuntimeLoader.Error)
                    console.log("AwayPage: RuntimeLoader failed to load", source, "-", errorString)
            }
        }

        // One finger: orbit. target: null - the handler only reports the
        // drag, it must not move the View3D itself.
        //
        // Deltas come from centroid.position (in this View3D's own
        // coordinates), NOT activeTranslation: the latter is in scene
        // (window) coordinates, and AppShell.qml rotates the whole UI -90
        // degrees for the physical panel, so on the device a scene-space
        // drag is a quarter turn off from what the finger does on the
        // picture (found on dev1: swiping sideways pitched the camera).
        DragHandler {
            target: null
            property point last: Qt.point(0, 0)
            onActiveChanged: {
                last = centroid.position
                if (active)
                    root.interactionStarted()
                else
                    root.interactionEnded()
            }
            onCentroidChanged: {
                if (!active)
                    return
                // Dragging right/down pulls the character's surface along
                // with the finger, i.e. the camera orbits the other way.
                root.yaw -= (centroid.position.x - last.x) * 0.4
                root.pitch = Math.max(-60, Math.min(20, root.pitch - (centroid.position.y - last.y) * 0.4))
                last = centroid.position
            }
        }

        // Two fingers: zoom (camera distance). Rotation/translation of the
        // pinch are ignored.
        PinchHandler {
            target: null
            property real startDistance: root.defaultDistance
            onActiveChanged: {
                if (active) {
                    startDistance = root.distance
                    root.interactionStarted()
                } else {
                    root.interactionEnded()
                }
            }
            onActiveScaleChanged: {
                if (active)
                    root.distance = Math.max(root.defaultDistance * 0.4, Math.min(root.defaultDistance * 2.5, startDistance / activeScale))
            }
        }
    }

    Text {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.margins: 16
        color: "#9e9e9e"
        font.pixelSize: 32
        text: "待机"
    }

    Text {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.margins: 16
        color: "#616161"
        font.pixelSize: 24
        text: "滑动旋转 · 双指缩放"
    }

    // Freezes the generated TimelineAnimation (unreachable from QML, hence
    // the C++ helper - see core/TimelineLoopCloser.h). Orbiting still
    // works while paused.
    Button {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.margins: 16
        text: root.paused ? "播放" : "暂停"
        onClicked: {
            root.paused = !root.paused
            timelineLoopCloser.setPaused(loader, root.paused)
        }
    }

    // Steps through the model's clips, so every one of them can be checked
    // on the device. Stand-in for the real thing (events picking clips).
    Row {
        visible: root.clipNames.length > 1
        anchors.bottom: parent.bottom
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.margins: 16
        spacing: 12

        Button {
            text: "◀"
            onClicked: root.stepClip(-1)
        }
        Text {
            anchors.verticalCenter: parent.verticalCenter
            width: 330
            horizontalAlignment: Text.AlignHCenter
            color: "#9e9e9e"
            font.pixelSize: 28
            text: root.currentClip + "\n" + (root.clipNames.indexOf(root.currentClip) + 1) + " / " + root.clipNames.length
        }
        Button {
            text: "▶"
            onClicked: root.stepClip(1)
        }
    }

    // No hidden-gesture/idle-timeout entry or exit yet (matches this app's
    // existing explicit-button navigation everywhere else, see
    // HomePage.qml's comment on skipping edge-front2's 5-tap gesture) -
    // this page is reachable from DevMenuPage.qml for now.
    Button {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.margins: 16
        text: "< 返回"
        onClicked: nav.pop()
    }
}
