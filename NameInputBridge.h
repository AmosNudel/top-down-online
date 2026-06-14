#ifndef NAME_INPUT_BRIDGE_H
#define NAME_INPUT_BRIDGE_H

#if defined(PLATFORM_WEB)
#include <emscripten.h>

// Opens the device keyboard via a hidden HTML input (mobile-friendly).
EM_JS(void, OpenPlayerNameKeyboard, (const char *current), {
    var input = document.getElementById("player-name-input");
    if (!input) return;
    input.value = UTF8ToString(current).toUpperCase().replace(/[^A-Z]/g, "").slice(0, 3);
    input.style.display = "block";
    input.focus({ preventScroll: true });
    if (input.setSelectionRange) {
        var len = input.value.length;
        input.setSelectionRange(len, len);
    }
});

EM_JS(void, ClosePlayerNameKeyboard, (), {
    var input = document.getElementById("player-name-input");
    if (!input) return;
    input.blur();
    input.style.display = "none";
});

EM_JS(void, ReadPlayerNameIntoBuffer, (char *buf, int maxLen), {
    var input = document.getElementById("player-name-input");
    var value = "";
    if (input) {
        value = (input.value || "").toUpperCase().replace(/[^A-Z]/g, "").slice(0, 3);
        input.value = value;
    }
    stringToUTF8(value, buf, maxLen);
});

EM_JS(int, IsPlayerNameKeyboardFocused, (), {
    var input = document.getElementById("player-name-input");
    return (input && input.style.display !== "none" && document.activeElement === input) ? 1 : 0;
});
#else
inline void OpenPlayerNameKeyboard(const char *) {}
inline void ClosePlayerNameKeyboard() {}
inline void ReadPlayerNameIntoBuffer(char *, int) {}
inline int IsPlayerNameKeyboardFocused() { return 0; }
#endif

#endif
