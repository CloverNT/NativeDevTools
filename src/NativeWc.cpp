#include "NativeDevTools.h"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace ndt {

using namespace sym;

void* NativeDevTools::createDevToolsWc(void* pageWc, void* pageIswc, const std::string& url) {
    if (!mNativeWcPrims || !pageWc)
        return nullptr;

    void* browserCtx = mGetBrowserContext(pageWc);
    if (!browserCtx) {
        logWarn("[wc] browser context null — abort");
        return nullptr;
    }

    alignas(16) std::uint8_t cp[0x400];
    alignas(16) std::uint8_t loc[0x20];
    std::memset(cp, 0, sizeof(cp));
    std::memset(loc, 0, sizeof(loc));
    mCreateParams(cp, browserCtx, loc);
    if (*reinterpret_cast<std::int32_t*>(cp + 0x14) != -2)
        logWarn("[wc] CreateParams cp[+0x14] != -2 (Create may not take the normal path)");

    void* newWc = nullptr;
    mWcCreate(&newWc, cp);
    if (!newWc) {
        logWarn("[wc] WebContents::Create returned null — abort");
        return nullptr;
    }

    if (pageIswc) {
        void** vtbl        = *reinterpret_cast<void***>(newWc);
        auto   setDelegate = reinterpret_cast<void (*)(void*, void*)>(vtbl[kWcSetDelegateVtOff / sizeof(void*)]);
        setDelegate(newWc, static_cast<char*>(pageIswc) + kIswcDelegateOff);
    }

    if (mFromOrCreate && mIsolateGetCurrent) {
        void*                     isolate = mIsolateGetCurrent();
        alignas(16) unsigned char hs[32]  = {};
        void*                     v8ctx   = nullptr;
        if (isolate && enterNodeContext(isolate, hs, v8ctx)) {
            struct CtxGuard {
                NativeDevTools* s;
                void*           c;
                void*           h;
                ~CtxGuard() {
                    s->exitNodeContext(c, h);
                }
            } guard{this, v8ctx, hs};
            alignas(16) std::uint8_t handle[16] = {};
            mFromOrCreate(handle, isolate, newWc);
            logInfo("[wc] FromOrCreate done (node main context entered) wc={}", newWc);
        } else {
            logWarn("[wc] could not enter node context — skipping FromOrCreate (devtools:// may blank)");
        }
    }

    alignas(16) std::uint8_t gurl[0x200];
    std::memset(gurl, 0, sizeof(gurl));
    struct Sv {
        const char* data;
        std::size_t len;
    } sv{url.data(), url.size()};
    mGurlCtor(gurl, &sv);

    alignas(16) std::uint8_t lp[0x400];
    std::memset(lp, 0, sizeof(lp));
    mLoadUrlParams(lp, gurl);

    void*                    navctrl = static_cast<char*>(newWc) + kNavControllerOff;
    alignas(16) std::uint8_t weak[0x20];
    std::memset(weak, 0, sizeof(weak));
    mLoadUrlWithParams(navctrl, weak, lp);
    logInfo("[wc] created + navigated: wc={} url={}", newWc, url);
    return newWc;
}

void NativeDevTools::createAndDockDevTools(int pageId) {
    if (!mNativeWcPrims || !mFromId || !mIswcShowDevTools || pageId < 0) {
        logWarn("[dock] primitives missing / bad pageId — skip");
        return;
    }
    if (mCtxEnterHook && mNodeEnv.load(std::memory_order_relaxed)) {
        (void) mCtxEnterHook->remove();
        mCtxEnterHook.reset();
        logInfo("[dock] Context::Enter capture hook removed (env captured)");
    }
    void* const pageApi = mFromId(reinterpret_cast<void*>(mWebContentsTable), pageId);
    if (!pageApi) {
        logWarn("[dock] FromID miss page={}", pageId);
        return;
    }
    void* pageIswc = rd(static_cast<char*>(pageApi) + kApiWcIswcOff);
    if (!pageIswc)
        return;
    void* pageView = rd(static_cast<char*>(pageIswc) + kIswcViewOff);
    if (!pageView)
        return;
    void* pageContentWc = rd(static_cast<char*>(pageIswc) + kIswcWebContentsOff);
    if (!pageContentWc)
        return;

    void* curManaged = rd(static_cast<char*>(pageIswc) + kIswcManagedDevtoolsOff);
    if (curManaged || hasCdpClientForPage(pageId)) {
        logWarn("[dock] page={} devtools already open — skip", pageId);
        return;
    }

    const std::string url =
            "devtools://devtools/bundled/devtools_app.html?can_dock=true&clovernt_target=" + std::to_string(pageId);
    void* newWc = createDevToolsWc(pageContentWc, pageIswc, url);
    if (!newWc) {
        logWarn("[dock] createDevToolsWc failed page={}", pageId);
        return;
    }

    *reinterpret_cast<void**>(static_cast<char*>(pageIswc) + kIswcManagedDevtoolsOff) = newWc;
    logInfo("[dock] created devtools wc + installed in managed slot: page={} newWc={}", pageId, newWc);
}

} // namespace ndt
