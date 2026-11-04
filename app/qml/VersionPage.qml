import QtQuick
import QtQuick.Controls

// Equivalent of edge-front2's lib/pages/version/version_page.dart, backed
// by core/UpgradeStore.h/.cpp (ported from lib/stores/upgrade_store.dart -
// see that port's header comment for the business rules). Real
// /upgrade/status and /upgrade/versions shapes confirmed on dev1 (see
// app/docs/progress.md). Progress card first, "the only card anyone is
// looking at during an OTA" - same ordering as upstream.
Item {
    id: root

    Component.onCompleted: upgradeStore.start()
    Component.onDestruction: upgradeStore.stop()

    Flickable {
        anchors.fill: parent
        anchors.margins: 16
        contentHeight: col.implicitHeight
        clip: true

        Column {
            id: col
            width: parent.width
            spacing: 12

            Row {
                spacing: 16
                Button { text: "< 返回"; onClicked: nav.pop() }
                Text { text: "版本和升级"; color: "white"; font.pixelSize: 26; anchors.verticalCenter: parent.verticalCenter }
                Button { text: "刷新"; onClicked: upgradeStore.refreshAll() }
                Button {
                    text: upgradeStore.triggering ? "触发中..." : "立即升级"
                    enabled: !upgradeStore.triggering && !upgradeStore.upgrading
                    onClicked: upgradeStore.trigger()
                }
            }
            Text {
                visible: upgradeStore.triggerError.length > 0
                color: "#e57373"
                font.pixelSize: 13
                text: "触发失败: " + upgradeStore.triggerError
            }

            ActionCard {
                width: parent.width
                title: "升级进度"
                Text {
                    color: upgradeStore.unreachable ? "#ffb300" : "white"
                    font.pixelSize: 15
                    text: upgradeStore.unreachable ? "升级中，设备暂时不可达（正在重连）..." : (upgradeStore.upgrading ? "升级进行中" : "空闲")
                }
                Text {
                    visible: upgradeStore.stalled
                    color: "#e57373"
                    font.pixelSize: 14
                    text: "进度已停滞 " + upgradeStore.progressAgeSeconds + " 秒"
                }
                Text {
                    color: "#cccccc"
                    font.pixelSize: 14
                    text: {
                        const p = upgradeStore.progress
                        if (!p || Object.keys(p).length === 0) return "(无进度记录)"
                        let s = "阶段: " + (p.phase ?? "-") + "  进度: " + (p.percent ?? "-") + "%"
                        if (p.attempt && p.attempt > 1) s += "  (第 " + p.attempt + " 次尝试)"
                        if (p.currentPackage) s += "\n当前包: " + p.currentPackage
                        if (p.detail) s += "\n" + p.detail
                        if (p.error) s += "\n错误: " + p.error
                        return s
                    }
                }
                Repeater {
                    model: {
                        const p = upgradeStore.progress
                        return (p && p.plan) ? p.plan : []
                    }
                    delegate: Text {
                        color: "#9e9e9e"
                        font.pixelSize: 13
                        text: modelData.name + ": " + modelData.from + " → " + modelData.to
                    }
                }
            }

            ActionCard {
                width: parent.width
                title: "服务版本"
                Text {
                    color: "white"
                    font.pixelSize: 14
                    text: {
                        const v = upgradeStore.versions
                        const s = (v && v.service) ? v.service : {}
                        return "名称: " + (s.name ?? "-") + "  版本: " + (s.version ?? "-") + "  核心版本: " + (s.coreVersion ?? "-")
                    }
                }
            }

            ActionCard {
                width: parent.width
                title: "系统信息"
                Text {
                    color: "white"
                    font.pixelSize: 14
                    text: {
                        const v = upgradeStore.versions
                        const os = (v && v.os) ? v.os : {}
                        const mcu = (v && v.mcu) ? v.mcu : {}
                        return "OS: " + (os.prettyName ?? "-") + "\n构建: " + (os.buildInfo ?? "-") + "\nMCU 固件: " + (mcu.version ?? "-")
                    }
                }
            }

            ActionCard {
                width: parent.width
                title: "安装包"
                Column {
                    spacing: 2
                    Repeater {
                        model: {
                            const v = upgradeStore.versions
                            const pkgs = (v && v.packages) ? v.packages : {}
                            return Object.keys(pkgs).map((name) => ({name: name, info: pkgs[name]}))
                        }
                        delegate: Text {
                            color: modelData.info.installed ? "#cccccc" : "#666666"
                            font.pixelSize: 13
                            text: modelData.name + ": " + (modelData.info.installed ? modelData.info.version : "未安装")
                        }
                    }
                }
            }

            ActionCard {
                width: parent.width
                title: "升级单元状态"
                Text {
                    color: "white"
                    font.pixelSize: 14
                    text: {
                        const s = upgradeStore.status
                        if (!s || Object.keys(s).length === 0) return "(无数据)"
                        return "unit: " + (s.unit ?? "-") + "  activeState: " + (s.activeState ?? "-")
                            + "  subState: " + (s.subState ?? "-") + "\nresult: " + (s.result ?? "-")
                            + "  exitCode: " + (s.exitCode ?? "-") + "  stalled: " + (s.stalled ?? false)
                    }
                }
                Text {
                    visible: upgradeStore.statusError.length > 0
                    color: "#e57373"
                    font.pixelSize: 13
                    text: "状态读取失败: " + upgradeStore.statusError
                }
            }
        }
    }
}
