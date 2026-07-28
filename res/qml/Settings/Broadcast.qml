import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import Mixxx 1.0 as Mixxx
import ".." as Skin
import "../Theme"

// M11 rough sketch: first-pass QML settings page for the Broadcast/Shoutcast
// profile that the legacy DlgPrefBroadcast dialog has always edited. Wraps
// the real BroadcastSettings/BroadcastProfile C++ model via
// Mixxx.Broadcast (QmlBroadcastProxy) rather than reinventing profile
// storage. Only edits the first/default profile -- multi-profile
// add/rename/remove management (which the legacy dialog supports via
// BroadcastSettingsModel) is out of scope for this pass. Enable/disable and
// live connection status are already exposed elsewhere via the "[Shoutcast]"
// ControlProxy group (see MasterPanel.qml's BCAST button).
Category {
    id: root

    property bool committing: false
    property bool hasChanges: false

    label: "Broadcast"

    // M11 audit fix: server type and format must round-trip through the
    // engine's exact internal identifiers (BROADCAST_SERVER_ICECAST2 =
    // "Icecast2", BROADCAST_SERVER_SHOUTCAST = "Shoutcast",
    // BROADCAST_SERVER_ICECAST1 = "Icecast1"; ENCODING_MP3 = "MP3",
    // ENCODING_OGG = "OGG" -- see src/broadcast/defs_broadcast.h,
    // src/recording/defs_recording.h), not a friendly display string --
    // this previously stored "Icecast 2" (extra space) and "OGG Vorbis"/
    // "AAC" (AAC isn't even a supported broadcast format), which would
    // have silently saved an invalid profile. valueRole/currentValue keeps
    // the display label and stored identifier separate and correct.
    function defaultLoginForServerType(serverType) {
        return serverType === "Shoutcast" ? "admin" : "source";
    }
    function load() {
        const p = Mixxx.Broadcast;
        hostField.text = p.getHost();
        portField.text = String(p.getPort());
        mountField.text = p.getMountpoint();
        loginField.text = p.getLogin() || root.defaultLoginForServerType(p.getServerType());
        passwordField.text = p.getPassword();
        serverTypeField.currentIndex = Math.max(0, serverTypeField.indexOfValue(p.getServerType()));
        formatField.currentIndex = Math.max(0, formatField.indexOfValue(p.getFormat()));
        bitrateField.text = String(p.getBitrate());
        streamNameField.text = p.getStreamName();
        streamDescField.text = p.getStreamDesc();
        streamGenreField.text = p.getStreamGenre();
        streamPublicField.checked = p.getStreamPublic();
        enableReconnectField.checked = p.getEnableReconnect();
        reconnectPeriodField.text = String(p.getReconnectPeriod());
        limitReconnectsField.checked = p.getLimitReconnects();
        maximumRetriesField.text = String(p.getMaximumRetries());
        enableMetadataField.checked = p.getEnableMetadata();
        customArtistField.text = p.getCustomArtist();
        customTitleField.text = p.getCustomTitle();
        metadataFormatField.text = p.getMetadataFormat();
        root.hasChanges = false;
    }
    function save() {
        const p = Mixxx.Broadcast;
        // A pasted full URL (e.g. "http://stream.example.com") is a
        // documented common mistake -- strip rather than fail at connect.
        p.setHost(hostField.text.replace(/^https?:\/\//i, ""));
        p.setPort(parseInt(portField.text) || 0);
        p.setMountpoint(mountField.text);
        p.setLogin(loginField.text);
        p.setPassword(passwordField.text);
        p.setServerType(serverTypeField.currentValue);
        p.setFormat(formatField.currentValue);
        p.setBitrate(parseInt(bitrateField.text) || 0);
        p.setStreamName(streamNameField.text);
        p.setStreamDesc(streamDescField.text);
        p.setStreamGenre(streamGenreField.text);
        p.setStreamPublic(streamPublicField.checked);
        p.setEnableReconnect(enableReconnectField.checked);
        p.setReconnectPeriod(parseFloat(reconnectPeriodField.text) || 0);
        p.setLimitReconnects(limitReconnectsField.checked);
        p.setMaximumRetries(parseInt(maximumRetriesField.text) || 0);
        p.setEnableMetadata(enableMetadataField.checked);
        p.setCustomArtist(customArtistField.text);
        p.setCustomTitle(customTitleField.text);
        p.setMetadataFormat(metadataFormatField.text);
        root.committing = true;
        p.commit();
    }

    Component.onCompleted: {
        load();
    }

    Connections {
        target: Mixxx.Broadcast

        function onCommitted(error) {
            root.committing = false;
            if (!error) {
                root.hasChanges = false;
            } else {
                errorMessage.text = error;
            }
        }
    }

    Mixxx.SettingParameter {
        label: "Server type"

        Skin.ComboBox {
            id: serverTypeField

            model: [
                {
                    text: "Icecast 2",
                    value: "Icecast2"
                },
                {
                    text: "Shoutcast 1",
                    value: "Shoutcast"
                },
                {
                    text: "Icecast 1",
                    value: "Icecast1"
                }
            ]
            textRole: "text"
            valueRole: "value"
            width: 160

            onActivated: {
                root.hasChanges = true;
                // Only auto-fill the login suggestion if it still matches
                // the OTHER type's default -- don't clobber something the
                // user typed themselves.
                const otherDefault = currentValue === "Shoutcast" ? "source" : "admin";
                if (loginField.text === "" || loginField.text === otherDefault) {
                    loginField.text = root.defaultLoginForServerType(currentValue);
                }
            }
        }
    }
    Mixxx.SettingParameter {
        label: "Host"

        Skin.TextField {
            id: hostField

            placeholderText: "stream.example.com (no http://)"
            width: 220

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Port"

        Skin.TextField {
            id: portField

            validator: IntValidator {
                bottom: 1
                top: 65535
            }
            width: 80

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        // Required for Icecast, unused for Shoutcast.
        label: "Mount point"
        visible: serverTypeField.currentValue !== "Shoutcast"

        Skin.TextField {
            id: mountField

            width: 160

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Login"

        Skin.TextField {
            id: loginField

            width: 160

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Password"

        Skin.TextField {
            id: passwordField

            echoMode: TextInput.Password
            width: 160

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Format"

        ColumnLayout {
            spacing: 2

            Skin.ComboBox {
                id: formatField

                model: [
                    {
                        text: "MP3",
                        value: "MP3"
                    },
                    {
                        text: "Ogg Vorbis",
                        value: "OGG"
                    }
                ]
                textRole: "text"
                valueRole: "value"
                width: 140

                onActivated: root.hasChanges = true
            }
            Text {
                color: "#B4453F"
                font.pixelSize: 10
                text: "Ogg streams don't support live artist/title metadata to listeners"
                visible: formatField.currentValue === "OGG"
            }
        }
    }
    Mixxx.SettingParameter {
        label: "Bitrate (kbps)"

        Skin.TextField {
            id: bitrateField

            validator: IntValidator {
                bottom: 8
                top: 320
            }
            width: 80

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Stream name"

        Skin.TextField {
            id: streamNameField

            width: 220

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Stream description"

        Skin.TextField {
            id: streamDescField

            width: 220

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Stream genre"

        Skin.TextField {
            id: streamGenreField

            width: 160

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "List publicly"

        CheckBox {
            id: streamPublicField

            onToggled: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Custom metadata format"

        Skin.TextField {
            id: metadataFormatField

            placeholderText: "%artist% - %title%"
            width: 220

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Custom artist override"

        Skin.TextField {
            id: customArtistField

            width: 160

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Custom title override"

        Skin.TextField {
            id: customTitleField

            width: 160

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Enable metadata"

        CheckBox {
            id: enableMetadataField

            onToggled: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Auto-reconnect"

        CheckBox {
            id: enableReconnectField

            onToggled: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Reconnect period (s)"

        Skin.TextField {
            id: reconnectPeriodField

            validator: DoubleValidator {
                bottom: 0
            }
            width: 80

            onTextEdited: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Limit reconnect attempts"

        CheckBox {
            id: limitReconnectsField

            onToggled: root.hasChanges = true
        }
    }
    Mixxx.SettingParameter {
        label: "Maximum retries"

        Skin.TextField {
            id: maximumRetriesField

            validator: IntValidator {
                bottom: 0
            }
            width: 80

            onTextEdited: root.hasChanges = true
        }
    }

    RowLayout {
        Layout.topMargin: 4

        Skin.FormButton {
            activeColor: "#999999"
            backgroundColor: "#7D3B3B"
            enabled: !root.committing
            opacity: enabled ? 1.0 : 0.5
            text: "Cancel"
            visible: root.hasChanges

            onPressed: {
                root.load();
            }
        }
        Item {
            Layout.fillWidth: true
        }
        Text {
            id: errorMessage

            Layout.alignment: Qt.AlignVCenter
            Layout.rightMargin: 16
            color: "#7D3B3B"
            text: ""
        }
        Skin.FormButton {
            activeColor: "#999999"
            backgroundColor: root.hasChanges ? "#3a60be" : Theme.darkGray3
            enabled: root.hasChanges && !root.committing
            opacity: enabled ? 1.0 : 0.5
            text: "Save"

            onPressed: {
                errorMessage.text = "";
                root.save();
            }
        }
    }
}
