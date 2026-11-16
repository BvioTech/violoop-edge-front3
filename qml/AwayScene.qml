import QtQuick
import QtQuick3D
import QtQuick3D.AssetUtils
import EdgeFrontQt.Core

// The away page's 3D scene: the mascot model, loaded at run time from a
// glTF file by Quick3D's RuntimeLoader, with the designer's camera,
// lights, probes and glass shader around it. AwayPage.qml puts the
// gestures and the controls on top; this file is what stays the same
// wherever the model is shown.
//
// RuntimeLoader is the approach qt3d-spike spent several real-hardware
// rounds validating (see ../demos/qt3d-spike/docs/findings.md and
// ../demos/qt3d-spike/runtime_loader.qml, which this View3D/camera setup
// mirrors).
//
// RuntimeLoader auto-plays whatever animation is baked into the glTF: its
// generated QtQuickTimeline TimelineAnimation defaults to `running: true`
// and `loops: Animation.Infinite` (confirmed by reading
// qtquick3d/src/assetutils/qssgqmlutilities.cpp), so no Timer/loop wiring
// is needed to keep it playing. What it does NOT do is close the loop:
// glTF keyframes rarely start at t=0, and the timeline fills that gap by
// blending in from the rest pose on every wrap-around (a twitch once per
// cycle). ClipPlayer (src/ClipPlayer.h) patches the generated timelines
// once the model has loaded - see that header.
//
// Asset: assets/away/VIO_DesktopPet.glb, the designer's mascot model
// (tracked via git-lfs, see ../.gitattributes, and listed in
// CMakeLists.txt's RESOURCES). Unlike the CesiumMan placeholder it
// replaced (one walk cycle), it carries 20 clips named by event
// ("idle.default", "welcome", "touch.left", ...). RuntimeLoader only ever
// shows the first one by itself; `clips` switches between them. Which
// event plays which clip is not wired up - see
// ../docs/user-model-spec.md.
View3D {
    id: scene

    // Orbit camera: yaw, pitch and distance say where the camera is on a
    // sphere around `pivot`. Whoever uses the scene moves them (the away
    // page: by dragging and pinching).
    //
    // The defaults reproduce the camera the designer first set this
    // model up for (qt/VioPreview.qml in their 2026-11-13 package): 35
    // degrees of vertical field of view, at (0, 1.10, 5.10), pitched -5.7
    // degrees. That line of sight crosses the character's axis
    // (x = z = 0) at y = 1.10 - 5.10 * tan(5.7deg) ~= 0.59, which is the
    // orbit pivot, 5.10 / cos(5.7deg) ~= 5.125 away. The model itself
    // stands turned by modelYaw, as in their preview (the 2026-11-15
    // one). minDistance..maxDistance is the preview's zoom range.
    //
    // That 2026-11-15 preview also moved the camera in to 3.85. Not
    // followed: on this panel it puts the laptop and the mug of the
    // "working" clip off the bottom and right edges, and the character
    // covering that much more of the screen costs a quarter of the frame
    // rate (dev1: idle.default 42 fps against 56, working 41 against 56).
    readonly property real defaultYaw: 0
    readonly property real defaultPitch: -5.7
    readonly property vector3d pivot: Qt.vector3d(0, 0.59, 0)
    readonly property real defaultDistance: 5.125
    readonly property real minDistance: 2.75
    readonly property real maxDistance: 5.5
    readonly property real modelYaw: 14

    property real yaw: defaultYaw
    property real pitch: defaultPitch
    property real distance: defaultDistance

    // The model to show. A property so that a different file can be put
    // in without editing the loader (the user-supplied-model feature in
    // ../docs/user-model-spec.md will need that).
    property url modelSource: Qt.resolvedUrl("../assets/away/VIO_DesktopPet.glb")

    // "loading" until the model is on screen, then "ready"; "error" if the
    // file could not be loaded.
    //
    // RuntimeLoader parses the file on the GUI thread, inside the
    // assignment to its `source` (about 1.5s for this model on dev1, plus
    // 0.2s here patching its timelines and another 0.2s for the first
    // frame to upload the meshes). Nothing can be drawn during that, so
    // the scene first lets one frame reach the screen (loadStarted,
    // below) - in which whoever uses the scene can show a loading notice -
    // and only then hands the loader its source. Without that the tap
    // that opens the away page appears to do nothing for two seconds.
    property string loadState: "loading"
    property bool loadStarted: false
    onModelSourceChanged: loadState = "loading"

    // Frames left in which to wait for the freshly loaded model to have
    // been drawn.
    property int framesToWait: 0
    Connections {
        target: scene.Window.window
        enabled: !scene.loadStarted || scene.framesToWait > 0
        function onFrameSwapped() {
            if (!scene.loadStarted) {
                scene.loadStarted = true
                return
            }
            scene.framesToWait = scene.modelDrawn() ? 0 : scene.framesToWait - 1
            // Don't stay "loading" forever for a model that has no
            // meshes to draw.
            if (scene.framesToWait === 0 && scene.loadState === "loading")
                scene.loadState = "ready"
        }
    }

    // Whether the model has been through a frame, i.e. is on screen. A
    // mesh only gets its bounds once it has been drawn, until then they
    // read as empty - so the first mesh with bounds says so. (Not
    // loader.bounds: for this model that reads the same before and after
    // the first frame.)
    function modelDrawn() {
        function visit(node) {
            if (node instanceof Model && !node.bounds.minimum.fuzzyEquals(node.bounds.maximum))
                return true
            for (let i = 0; i < node.children.length; ++i) {
                if (visit(node.children[i]))
                    return true
            }
            return false
        }
        return visit(loader)
    }

    // Stops drawing nodes that the model has scaled down to nothing.
    // glTF can't animate visibility, so this model hides whatever a clip
    // doesn't use by parking it at scale 1e-5 - the props, and all but
    // one of its lamp meshes at any one time. Quick3D still pushes all of
    // that through the GPU every frame (measured on dev1 with the model's
    // first, heavier version, where that was 229k of 382k triangles:
    // default view, rendered offscreen and uncapped, 34 fps with them, 47
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

    // What the lights and the glass need from the loaded model (see the
    // lights below): the meshes the lights are scoped to, the visor
    // glass, and the node whose position the clips animate to say how
    // bright and how warm the model's own lamps currently are.
    //
    // bodyModel is the multi-material skinned shell ("VIO" in the file),
    // displayModel the inner face with the normal map ("Display"),
    // glassModel the transparent visor over it ("Window"),
    // notebookModel and accessoryModel the two silver props of the
    // "working" clip's desk (the laptop "Notebook"; the two monitors and
    // the mug, one mesh), told apart from everything else by their silver
    // material (metalness 0.52, roughness 0.32),
    // lightCue the empty node "VioLightCue", position (power, warmth, _).
    // A warmth of -1 is not a colour but the cue's way of saying
    // "offline" (the "offline" clip: grey lamp, grey eyes) - `offline`
    // below, with its own branch wherever the warmth is used.
    // Looked up by glTF node name, falling back on what the nodes look
    // like - which is what actually finds them: RuntimeLoader (Qt
    // 6.8.5-violoop3) does not carry node names over. For the cue that is
    // its z, which the model keeps at 12.125 as a marker. Any of them can
    // stay null: a light without a scope lights everything, and without
    // the cue the lights sit at the model's normal state.
    property Model bodyModel: null
    property Model glassModel: null
    property Model displayModel: null
    property Model notebookModel: null
    property Model accessoryModel: null
    property Node lightCue: null
    readonly property real lightPower: lightCue ? lightCue.x : 0.5
    readonly property real warmth: lightCue ? lightCue.y : 0
    readonly property bool offline: warmth < -0.5
    readonly property color spill: offline ? Qt.rgba(0.10, 0.11, 0.12, 1)
                                           : Qt.rgba(0.04 + 0.96 * warmth, 0.84 - 0.58 * warmth, 0.94 - 0.91 * warmth, 1)

    function findRig() {
        let body = null, display = null, cue = null
        let bodyGuess = null, displayGuess = null, cueGuess = null, glass = null
        let notebook = null, accessory = null
        function visit(node) {
            if (node.objectName === "VioLightCue")
                cue = node
            if (node instanceof Model) {
                if (node.objectName === "VIO")
                    body = node
                else if (node.objectName === "Display")
                    display = node
                if (node.skin) {
                    const n = node.materials.length
                    if (n > 1 && (!bodyGuess || n > bodyGuess.materials.length))
                        bodyGuess = node
                    else if (n === 1 && !displayGuess && node.materials[0].normalMap)
                        displayGuess = node
                    // roughness: the code windows are skinned, blended
                    // single-material meshes too, but matte.
                    else if (n === 1 && !glass && node.materials[0] instanceof PrincipledMaterial
                             && node.materials[0].alphaMode === PrincipledMaterial.Blend
                             && node.materials[0].roughness < 0.5)
                        glass = node
                } else {
                    for (let i = 0; i < node.materials.length; ++i) {
                        const m = node.materials[i]
                        if (!(m instanceof PrincipledMaterial) || Math.abs(m.metalness - 0.52) > 0.001
                                || Math.abs(m.roughness - 0.32) > 0.001)
                            continue
                        // The silver reflects the face's probe.
                        m.lightProbe = faceProbe
                        if (!notebook)
                            notebook = node
                        else if (notebook !== node)
                            accessory = node
                    }
                }
            } else if (!cueGuess && node.children.length === 0 && Math.abs(node.z - 12.125) < 0.001) {
                cueGuess = node
            }
            for (let i = 0; i < node.children.length; ++i)
                visit(node.children[i])
        }
        visit(loader)
        bodyModel = body || bodyGuess
        displayModel = display || displayGuess
        lightCue = cue || cueGuess
        glassModel = glass
        notebookModel = notebook
        accessoryModel = accessory

        // The designer's treatment of the face and the visor, as in their
        // qt/VioModel.qml. The face reflects its own small probe and its
        // glow turns with the lamps' warmth; the visor's PBR material is
        // replaced by the glass shader (the material the file carries is
        // the fallback for viewers without it).
        if (displayModel) {
            const face = displayModel.materials[0]
            face.lightProbe = faceProbe
            face.emissiveFactor = Qt.binding(function() {
                if (scene.offline)
                    return Qt.vector3d(0.24, 0.24, 0.24)
                return Qt.vector3d(1, 1 - 0.74 * scene.warmth, 1 - 0.84 * scene.warmth)
            })
        }
        if (glassModel)
            glassModel.materials = [glassMaterial]
        console.log("AwayScene: light rig - body", bodyModel, body ? "(by name)" : "(guessed)",
                    "display", displayModel, display ? "(by name)" : "(guessed)",
                    "cue", lightCue, cue ? "(by name)" : "(guessed)", "power", lightPower, "warmth", warmth,
                    "glass", glassModel, "notebook", notebookModel, "accessories", accessoryModel)
    }

    // The loaded model's animation clips: clips.clipNames,
    // clips.currentClip, clips.paused, clips.play(name, fadeMs) - see
    // src/ClipPlayer.h.
    readonly property ClipPlayer clips: ClipPlayer {}

    environment: SceneEnvironment {
        clearColor: "#000000"
        backgroundMode: SceneEnvironment.Color
        antialiasingMode: SceneEnvironment.MSAA
        antialiasingQuality: SceneEnvironment.High
        // What the dark metal shell, the glossy hands and the props
        // are lit by - see the lights below.
        lightProbe: Texture {
            source: Qt.resolvedUrl("../assets/away/visor-studio.hdr")
            mappingMode: Texture.LightProbe
        }
    }

    // The face's own probe, for its soft highlight (set on the face
    // material in findRig()).
    Texture {
        id: faceProbe
        source: Qt.resolvedUrl("../assets/away/display-studio.hdr")
        mappingMode: Texture.LightProbe
    }

    // The visor glass: one transparent draw over the face that
    // computes its reflection of a studio panorama per pixel
    // (assets/away/canopy.frag, the designer's shader, unchanged), with
    // the lamps' light picked up along its lower edge; it turns grey by
    // itself when signalWarmth says offline.
    CustomMaterial {
        id: glassMaterial
        property real signalPower: scene.lightPower
        property real signalWarmth: scene.warmth
        property TextureInput reflectionMap: TextureInput {
            texture: Texture {
                source: Qt.resolvedUrl("../assets/away/optical-studio.hdr")
                tilingModeHorizontal: Texture.Repeat
                tilingModeVertical: Texture.ClampToEdge
                generateMipmaps: false
            }
        }
        shadingMode: CustomMaterial.Shaded
        sourceBlend: CustomMaterial.SrcAlpha
        destinationBlend: CustomMaterial.OneMinusSrcAlpha
        cullMode: Material.BackFaceCulling
        depthDrawMode: Material.NeverDepthDraw
        fragmentShader: Qt.resolvedUrl("../assets/away/canopy.frag")
    }

    // Yaw and pitch on separate nested nodes so the orbit doesn't
    // depend on Euler-angle composition order.
    Node {
        position: scene.pivot
        eulerRotation.y: scene.yaw

        Node {
            eulerRotation.x: scene.pitch

            PerspectiveCamera {
                id: camera
                z: scene.distance
                fieldOfView: 35
                // Default clipNear (10) clips away meter-scale glTF
                // content at this camera distance - same fix
                // runtime_loader.qml needed, see that file's comment.
                clipNear: 0.01
                clipFar: 50
            }
        }
    }

    // The designer's light rig for this model, values as given in
    // "VIO灯光参数与设备接入说明" (2026-11-15): three directional and
    // three point lights, each scoped to either the shell or the face
    // (scene.bodyModel / scene.displayModel), positions in meters, plus
    // two directional lights for the desk of the "working" clip. Two of
    // the point lights follow the model's own lamps in brightness and
    // color (scene.lightPower / scene.spill).
    //
    // The rest of that rig is above: the environment's light probe,
    // the face's own probe and the glass shader. Everything outside
    // the scoped meshes (the other props, lamps) is lit by the
    // environment probe alone, and the dark metal shell and the hands
    // show little without it (on dev1: near black).
    //
    // All of them stay fixed in the world (not parented to the camera
    // rig) on purpose: the shading changing as the camera orbits is
    // part of what shows this is a lit 3D scene.

    // The laptop of the "working" clip. Off unless the model has one: a
    // light without a scope would light everything.
    DirectionalLight {
        scope: scene.notebookModel
        visible: scene.notebookModel !== null
        eulerRotation: Qt.vector3d(-24, -18, 0)
        color: "#e9edf5"
        brightness: 2.80
    }

    // That clip's monitors and mug.
    DirectionalLight {
        scope: scene.accessoryModel
        visible: scene.accessoryModel !== null
        eulerRotation: Qt.vector3d(-27, -25, 0)
        color: "#e9edf5"
        brightness: 1.10
    }

    // Face key.
    DirectionalLight {
        scope: scene.displayModel
        eulerRotation: Qt.vector3d(-26, 49, 0)
        color: "#e6e5f3"
        brightness: 2.75
        ambientColor: "#0a0c12"
    }

    // Rim on the shell and the round hands.
    DirectionalLight {
        scope: scene.bodyModel
        eulerRotation: Qt.vector3d(-36, -125, 0)
        color: "#b7cfea"
        brightness: 0.30
    }

    // Face fill from the front.
    DirectionalLight {
        scope: scene.displayModel
        eulerRotation: Qt.vector3d(-20, -12, 0)
        color: "#e8e9f1"
        brightness: 0.38
    }

    // Lamp light coming back off the floor.
    PointLight {
        scope: scene.bodyModel
        position: Qt.vector3d(-0.44, 0.09, 0.76)
        color: scene.spill
        brightness: 0.30 + 2.15 * scene.lightPower
        constantFade: 1
        linearFade: 0
        quadraticFade: 12
    }

    // Fill on the round hands and the shell.
    PointLight {
        scope: scene.bodyModel
        position: Qt.vector3d(0.67, 0.41, 1.17)
        color: "#80abc9"
        brightness: 0.87
        constantFade: 1
        linearFade: 0
        quadraticFade: 6.5
    }

    // Lamp light coming back inside the visor.
    PointLight {
        scope: scene.displayModel
        position: Qt.vector3d(-0.27, 0.68, 0.91)
        color: scene.spill
        brightness: 0.12 + 0.82 * scene.lightPower
        constantFade: 1
        quadraticFade: 10
    }

    RuntimeLoader {
        id: loader
        eulerRotation.y: scene.modelYaw
        source: scene.loadStarted ? scene.modelSource : ""
        onStatusChanged: {
            scene.bodyModel = null
            scene.displayModel = null
            scene.glassModel = null
            scene.notebookModel = null
            scene.accessoryModel = null
            scene.lightCue = null
            scene.clips.detach()
            if (status === RuntimeLoader.Success) {
                scene.findRig()
                scene.clips.attach(loader)
                scene.hideCollapsedNodes()
                scene.framesToWait = 600
            } else if (status === RuntimeLoader.Error) {
                scene.loadState = "error"
                console.error("AwayScene: RuntimeLoader failed to load", source, "-", errorString)
            }
        }
    }
}
