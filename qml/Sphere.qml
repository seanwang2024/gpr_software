import QtQuick
import QtQuick3D

View3D {
    anchors.fill: parent
    environment: SceneEnvironment {
        clearColor: "#1a1a2e"
        backgroundMode: SceneEnvironment.Color
    }

    PerspectiveCamera {
        id: camera
        position: Qt.vector3d(0, 0, 300)
    }

    DirectionalLight {
        eulerRotation: Qt.vector3d(-30, -70, 0)
        brightness: 1.5
    }

    DirectionalLight {
        eulerRotation: Qt.vector3d(30, 70, 0)
        brightness: 0.3
        color: "#4488ff"
    }

    Model {
        id: sphere
        source: "#Sphere"
        y: floatY
        materials: PrincipledMaterial {
            baseColor: "#4488ff"
            metalness: 0.8
            roughness: 0.2
        }

        property real floatY: 0
        SequentialAnimation on floatY {
            loops: Animation.Infinite
            NumberAnimation { from: -30; to: 30; duration: 2000; easing.type: Easing.InOutQuad }
            NumberAnimation { from: 30; to: -30; duration: 2000; easing.type: Easing.InOutQuad }
        }

        SequentialAnimation on eulerRotation.y {
            loops: Animation.Infinite
            NumberAnimation { from: 0; to: 360; duration: 5000 }
        }
    }
}
