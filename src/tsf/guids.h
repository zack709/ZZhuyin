#pragma once

#include <windows.h>

namespace zzhuyin::tsf {

// {8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A01}
inline constexpr CLSID kClsidTextService = {
    0x8e4a7c31, 0x2b5d, 0x4f19, {0x9a, 0x63, 0x7d, 0x1e, 0x4b, 0x8c, 0x2a, 0x01}
};

// {8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A02}
inline constexpr GUID kGuidProfile = {
    0x8e4a7c31, 0x2b5d, 0x4f19, {0x9a, 0x63, 0x7d, 0x1e, 0x4b, 0x8c, 0x2a, 0x02}
};

// {8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A03}
inline constexpr GUID kGuidDisplayAttributeInput = {
    0x8e4a7c31, 0x2b5d, 0x4f19, {0x9a, 0x63, 0x7d, 0x1e, 0x4b, 0x8c, 0x2a, 0x03}
};

// {8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A04}
inline constexpr GUID kGuidLangBarItem = {
    0x8e4a7c31, 0x2b5d, 0x4f19, {0x9a, 0x63, 0x7d, 0x1e, 0x4b, 0x8c, 0x2a, 0x04}
};

// {8E4A7C31-2B5D-4F19-9A63-7D1E4B8C2A05}
inline constexpr GUID kGuidPreservedKeyToggleNative = {
    0x8e4a7c31, 0x2b5d, 0x4f19, {0x9a, 0x63, 0x7d, 0x1e, 0x4b, 0x8c, 0x2a, 0x05}
};

// Standard Windows 8/10/11 system tray IME input mode button GUID (GUID_LBI_INPUTMODE)
// {2C77A81E-41CC-4178-A3A7-5F8A987568E6}
inline constexpr GUID kGuidLbiInputMode = {
    0x2c77a81e, 0x41cc, 0x4178, {0xa3, 0xa7, 0x5f, 0x8a, 0x98, 0x75, 0x68, 0xe6}
};

inline constexpr LANGID kLangIdZhTw = MAKELANGID(LANG_CHINESE, SUBLANG_CHINESE_TRADITIONAL); // 0x0404
inline constexpr wchar_t kProfileDescription[] = L"Z注音";
inline constexpr wchar_t kClsidDescription[]   = L"Z注音";

} // namespace zzhuyin::tsf
