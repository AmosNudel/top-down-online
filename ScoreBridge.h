#ifndef SCORE_BRIDGE_H
#define SCORE_BRIDGE_H

#if defined(PLATFORM_WEB)
#include <emscripten.h>

// Sends final score to the parent Next.js page (HeroGui listens for this).
EM_JS(void, SendScoreToParent, (const char *name, int score), {
    if (typeof parent !== "undefined" && parent !== window) {
        parent.postMessage({
            type: "top-down-survive:score",
            name: UTF8ToString(name),
            score: score
        }, "*");
    }
});
#else
inline void SendScoreToParent(const char *, int) {}
#endif

#endif
