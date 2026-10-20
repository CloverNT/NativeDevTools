#include "NativeDevTools.h"

#include <cstddef>
#include <cstdint>
#include <string>

#include <nlohmann/json.hpp>

namespace ndt {

using namespace sym;

namespace {

    struct CdpClient {
        const void*     vtable; // DevToolsAgentHostClient vtable
        NativeDevTools* self;
        int             pageId;      // inspected page webContents id
        void*           agentHost;   // DevToolsAgentHost*
        void*           frontendRfh; // frontend RFH
        void*           pageIswc;    // inspected page ISWC
    };

    void* cdpDtor(void* thisp, unsigned int /*flags*/) {
        return thisp;
    } // lifetime is ours; never delete
    char cdpMayAttachUrl(void* /*t*/) {
        return 1;
    }
    char cdpMayAttachRfh(void* /*t*/) {
        return 1;
    }
    char cdpIsTrusted(void* /*t*/) {
        return 1;
    }
    char cdpMayReadLocal(void* /*t*/) {
        return 1;
    }
    char cdpMayWriteLocal(void* /*t*/) {
        return 1;
    }
    char cdpAllowUnsafe(void* /*t*/) {
        return 1;
    }
    char cdpUsesBinary(void* /*t*/) {
        return 0;
    } // JSON text CDP

    void* cdpNavInitiatorOrigin(void* retbuf, void* /*t*/) {
        if (retbuf)
            *static_cast<std::uint64_t*>(retbuf) = 0;
        return retbuf;
    }

    void* cdpTypeForMetrics(void* retbuf, void* /*t*/) {
        auto* r = static_cast<std::uint8_t*>(retbuf);
        for (int i = 0; i < 24; ++i)
            r[i] = 0;
        r[0] = static_cast<std::uint8_t>(5u << 1); // SSO, size = 5
        r[1] = 'O';
        r[2] = 't';
        r[3] = 'h';
        r[4] = 'e';
        r[5] = 'r';
        return retbuf;
    }

    const void* gCdpVtable[kCdpClientVtableSlots] = {
            reinterpret_cast<const void*>(&cdpDtor),                             // 0  dtor
            reinterpret_cast<const void*>(&NativeDevTools::sCdpDispatch),        // 1  DispatchProtocolMessage
            reinterpret_cast<const void*>(&NativeDevTools::sCdpAgentHostClosed), // 2  AgentHostClosed
            reinterpret_cast<const void*>(&cdpMayAttachUrl),                     // 3  MayAttachToURL
            reinterpret_cast<const void*>(&cdpMayAttachRfh),                     // 4  MayAttachToRenderFrameHost
            reinterpret_cast<const void*>(&cdpIsTrusted),                        // 5  IsTrusted
            reinterpret_cast<const void*>(&cdpMayReadLocal),                     // 6  MayReadLocalFiles
            reinterpret_cast<const void*>(&cdpMayWriteLocal),                    // 7  MayWriteLocalFiles
            reinterpret_cast<const void*>(&cdpAllowUnsafe),                      // 8  AllowUnsafeOperations
            reinterpret_cast<const void*>(&cdpNavInitiatorOrigin), // 9  GetNavigationInitiatorOrigin(sret)
            reinterpret_cast<const void*>(&cdpUsesBinary),         // 10 UsesBinaryProtocol
            reinterpret_cast<const void*>(&cdpTypeForMetrics),     // 11 GetTypeForMetrics(sret)
    };

} // namespace

void NativeDevTools::sCdpDispatch(void* client, void* /*agentHost*/, void* span) {
    auto* c = reinterpret_cast<CdpClient*>(client);
    if (!c || !c->self)
        return;
    const std::uint8_t* data = nullptr;
    std::uint64_t       size = 0;
    if (span) {
        data = rd<const std::uint8_t*>(span);
        size = rd<std::uint64_t>(static_cast<const char*>(span) + 8);
    }
    if (!data || !size)
        return;
    const std::string cdp(reinterpret_cast<const char*>(data), static_cast<std::size_t>(size));
    // Deliver to the current live frame (not a cached RFH, which reload can dangle).
    void* const fresh = c->self->freshFrontendRfh(c->pageIswc);
    if (fresh) {
        std::string script = "DevToolsAPI.dispatchMessage(";
        script += nlohmann::json(cdp).dump(-1, ' ', true);
        script += ")";
        c->self->runFrontendJs(fresh, script);
        static int n = 0;
        if (++n <= 3 || n % 500 == 0)
            c->self->logInfo("[cdp] -> frontend #{} ({}B)", n, size);
    } else {
        c->self->logInfo("[cdp] recv (page={} client={} {}B): {}",
                         c->pageId,
                         client,
                         size,
                         size < 400 ? cdp : "<" + std::to_string(size) + " bytes>");
    }
}

void NativeDevTools::sCdpAgentHostClosed(void* client, void* /*agentHost*/) {
    auto* c = reinterpret_cast<CdpClient*>(client);
    if (c && c->self)
        c->self->logInfo("[cdp] AgentHostClosed (page={} client={})", c->pageId, client);
}

void NativeDevTools::setupCdpClient(int pageId, void* pageIswc, void* frontendRfh) {
    if (!mCdp || pageId < 0 || !pageIswc)
        return;
    if (mCdpClientN >= static_cast<int>(mCdpClients.size())) {
        logWarn("[cdp] client slots full");
        return;
    }

    void* pageContentWc = rd(static_cast<char*>(pageIswc) + kIswcWebContentsOff);
    if (!pageContentWc) {
        logWarn("[cdp] page content wc null — skip");
        return;
    }
    for (int i = 0; i < mCdpClientN; ++i) {
        auto* ex = static_cast<CdpClient*>(mCdpClients[i]);
        if (ex && ex->pageId == pageId) {
            ex->frontendRfh = frontendRfh;
            logDebug("[cdp] page={} already attached — refreshed frontendRfh", pageId);
            return;
        }
    }

    void* agentHost = nullptr;
    mGetOrCreateFor(&agentHost, pageContentWc); // out = scoped_refptr<DevToolsAgentHost>
    if (!agentHost) {
        logWarn("[cdp] GetOrCreateFor returned null");
        return;
    }

    auto* c                    = new CdpClient{};
    c->vtable                  = gCdpVtable;
    c->self                    = this;
    c->pageId                  = pageId;
    c->agentHost               = agentHost;
    c->frontendRfh             = frontendRfh;
    c->pageIswc                = pageIswc;
    mCdpClients[mCdpClientN++] = c;

    mAttachClientCall(agentHost, c);
    logInfo("[cdp] client attached: page={} agentHost={} client={} rfh={}",
            pageId,
            agentHost,
            static_cast<void*>(c),
            frontendRfh);
}

void* NativeDevTools::freshFrontendRfh(void* pageIswc) {
    if (!pageIswc)
        return nullptr;
    void* devWc = rd(static_cast<char*>(pageIswc) + kIswcManagedDevtoolsOff);
    if (!devWc)
        return nullptr;
    return rd(static_cast<char*>(devWc) + kWcPrimaryMainFrameOff); // may be null during a load gap
}

void NativeDevTools::runFrontendJs(void* rfh, const std::string& asciiScript) {
    if (!mExecJs || !rfh)
        return;
    std::u16string u16;
    u16.reserve(asciiScript.size());
    for (const char ch: asciiScript)
        u16.push_back(static_cast<char16_t>(static_cast<unsigned char>(ch)));
    alignas(8) std::uint8_t cb[16] = {0}; // empty OnceCallback
    mExecJs(rfh, &u16, cb, 0);            // world 0 = main world
}

void NativeDevTools::sendCdpToPage(int pageId, const void* data, std::size_t len) {
    if (!mDispatchProtoSend || !data || !len)
        return;
    for (int i = 0; i < mCdpClientN; ++i) {
        auto* c = static_cast<CdpClient*>(mCdpClients[i]);
        if (c && c->pageId == pageId && c->agentHost) {
            struct Span {
                const void* data;
                std::size_t size;
            } span{data, len};
            mDispatchProtoSend(c->agentHost, c, &span);
            static int m = 0;
            if (++m <= 3 || m % 500 == 0)
                logInfo("[cdp] <- frontend #{} ({}B)", m, len);
            return;
        }
    }
    logWarn("[cdp] sendCdpToPage: page={} has no client", pageId);
}

bool NativeDevTools::hasCdpClientForPage(int pageId) const {
    if (pageId < 0)
        return false;
    for (int i = 0; i < mCdpClientN; ++i) {
        auto* c = static_cast<CdpClient*>(mCdpClients[i]);
        if (c && c->pageId == pageId)
            return true;
    }
    return false;
}

void NativeDevTools::teardownCdpClient(void* pageIswc) {
    if (!pageIswc)
        return;
    for (int i = 0; i < mCdpClientN; ++i) {
        auto* c = static_cast<CdpClient*>(mCdpClients[i]);
        if (!c || c->pageIswc != pageIswc)
            continue;
        const int   pid     = c->pageId;
        void* const deadRfh = c->frontendRfh;
        void* const host    = c->agentHost;
        if (host)
            detachClientCleanup(host, c);
        mCdpClients[i]           = mCdpClients[--mCdpClientN];
        mCdpClients[mCdpClientN] = nullptr;
        delete c;
        if (host)
            releaseAgentHost(host);
        if (deadRfh)
            clearHostChannelRfh(deadRfh);
        logInfo("[dock] teardownCdpClient page={} (devtools wc destroyed)", pid);
        return;
    }
}

void NativeDevTools::releaseAgentHost(void* host) {
    if (!host)
        return;
    auto* rc = reinterpret_cast<std::int32_t*>(static_cast<char*>(host) + kAgentHostRefCountOff);
    if (*rc <= 0) {
        logWarn("[cdp] agent host ref_count={} (<=0) — not releasing", *rc);
        return;
    }
    if (--(*rc) != 0)
        return;
    void** vt       = *reinterpret_cast<void***>(host);
    auto   destruct = reinterpret_cast<void (*)(void*, int)>(vt[kAgentHostDeleteDtorVtOff / sizeof(void*)]);
    destruct(host, 1);
    logInfo("[cdp] agent host last ref released -> deleted host={}", host);
}

} // namespace ndt
