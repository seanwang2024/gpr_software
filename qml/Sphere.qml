import QtQuick
import QtQuick3D

View3D {
    id: view3d
    anchors.fill: parent
    environment: SceneEnvironment {
        clearColor: "#e0e0e0"
        backgroundMode: SceneEnvironment.Color
    }

    property real camDistance: 3000
    property real camRotX: -20
    property real camRotY: 30
    property string textureSource: ""

    PerspectiveCamera {
        id: camera
        position: Qt.vector3d(
            camDistance * Math.sin(camRotY * Math.PI / 180) * Math.cos(camRotX * Math.PI / 180),
            camDistance * Math.sin(camRotX * Math.PI / 180),
            camDistance * Math.cos(camRotY * Math.PI / 180) * Math.cos(camRotX * Math.PI / 180)
        )
        eulerRotation: Qt.vector3d(-camRotX, -camRotY, 0)
    }

    DirectionalLight {
        eulerRotation: Qt.vector3d(-30, -70, 0)
        brightness: 2.0
    }

    DirectionalLight {
        eulerRotation: Qt.vector3d(30, 70, 0)
        brightness: 0.5
    }

    Texture {
        id: jpgTexture
        source: view3d.textureSource
        generateMipmaps: true
    }

    PrincipledMaterial {
        id: texMaterial
        baseColorMap: jpgTexture
        metalness: 0.0
        roughness: 0.5
    }

    PrincipledMaterial {
        id: solidMaterial
        baseColor: "#88aacc"
        opacity: 0.3
        metalness: 0.0
        roughness: 0.5
    }

    Model {
        id: cube
        source: "#Cube"
        materials: [
            texMaterial,
            texMaterial,
            texMaterial,
            texMaterial,
            texMaterial,
            texMaterial
        ]
        scale: Qt.vector3d(59.53, 5.12, 0.50)
    }

    MouseArea {
        anchors.fill: parent
        acceptedButtons: Qt.LeftButton | Qt.RightButton

        property real lastX: 0
        property real lastY: 0

        onPressed: (mouse) => {
            lastX = mouse.x
            lastY = mouse.y
        }

        onPositionChanged: (mouse) => {
            if (pressedButtons & Qt.LeftButton) {
                var dx = mouse.x - lastX
                var dy = mouse.y - lastY
                camRotY += dx * 0.15
                camRotX += dy * 0.15
                camRotX = Math.max(-89, Math.min(89, camRotX))
                lastX = mouse.x
                lastY = mouse.y
            }
        }

        onWheel: (wheel) => {
            camDistance -= wheel.angleDelta.y * 0.5
            camDistance = Math.max(100, Math.min(800, camDistance))
        }
    }
}
