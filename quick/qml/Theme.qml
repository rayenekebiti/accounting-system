pragma Singleton
import QtQuick
QtObject {
    id: theme
    property bool dark: false
    property bool reduceMotion: false
    property bool rtl: false
    readonly property QtObject color: QtObject {
        readonly property color canvas: "#F8FAFC"
        readonly property color surface: "#FFFFFF"
        readonly property color surfaceMuted: "#F1F5F9"
        readonly property color border: "#E2E8F0"
        readonly property color borderStrong: "#CBD5E1"
        readonly property color textPrimary: "#0F172A"
        readonly property color textSecondary: "#64748B"
        readonly property color textOnBrand: "#FFFFFF"
        readonly property color brand: "#4F46E5"
        readonly property color brandHover: "#4338CA"
        readonly property color brandSubtle: "#EEF2FF"
        readonly property color accent: "#14B8A6"
        readonly property color income: "#059669"
        readonly property color incomeSubtle: "#ECFDF5"
        readonly property color expense: "#E11D48"
        readonly property color expenseSubtle: "#FFF1F2"
        readonly property color pending: "#D97706"
        readonly property color pendingSubtle: "#FFFBEB"
        readonly property color info: "#0EA5E9"
        readonly property color infoSubtle: "#E0F2FE"
        readonly property color focusRing: "#6366F1"
    }
    readonly property QtObject space: QtObject {
        readonly property int xxs: 2; readonly property int xs: 4; readonly property int sm: 8
        readonly property int md: 12; readonly property int lg: 16; readonly property int xl: 24
        readonly property int xxl: 32; readonly property int xxxl: 48
    }
    readonly property QtObject radius: QtObject {
        readonly property int sm: 8; readonly property int md: 10; readonly property int lg: 12
        readonly property int card: 16; readonly property int pill: 999
    }
    readonly property QtObject font: QtObject {
        readonly property string sans: "Inter"
        readonly property string arabic: "IBM Plex Sans Arabic"
        // QML's font value type only supports a SINGLE family string (no `families`).
        // These are locale-aware single families; fallback chains are provided in C++
        // via QFont::insertSubstitutions (see main_quick.cpp), so a missing primary
        // (e.g. Inter not yet bundled) resolves to Segoe UI / Tahoma automatically.
        readonly property string uiFamily: theme.rtl ? "IBM Plex Sans Arabic" : "Inter"
        readonly property string numericFamily: "Inter"
        readonly property real lineHeightArabic: 1.35
        readonly property real lineHeightLatin: 1.20
        readonly property int xs: 11; readonly property int sm: 12; readonly property int base: 13
        readonly property int md: 14; readonly property int lg: 16; readonly property int xl: 20
        readonly property int xxl: 24; readonly property int xxxl: 30
        readonly property int weightRegular: 400; readonly property int weightMedium: 500
        readonly property int weightSemibold: 600; readonly property int weightBold: 700
    }
    readonly property QtObject elevation: QtObject {
        readonly property color shadowColor: "#1E293B"
        readonly property real e1Opacity: 0.06; readonly property int e1Blur: 12; readonly property int e1Y: 2
        readonly property real e2Opacity: 0.10; readonly property int e2Blur: 24; readonly property int e2Y: 6
    }
    readonly property QtObject motion: QtObject {
        readonly property int fast: 120; readonly property int base: 180; readonly property int slow: 260
    }
}
