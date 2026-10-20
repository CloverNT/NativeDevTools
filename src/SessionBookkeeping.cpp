#include "NativeDevTools.h"

#include <cstring>

namespace ndt {

using namespace sym;

bool NativeDevTools::markSeenAh(void* ah) {
    for (int i = 0; i < mSeenAhN; ++i)
        if (mSeenAh[i] == ah)
            return false;
    if (mSeenAhN < static_cast<int>(mSeenAh.size()))
        mSeenAh[mSeenAhN++] = ah;
    return true;
}

std::uint64_t NativeDevTools::restoreAttach(void* agentHost, void* client) {
    char* ah = static_cast<char*>(agentHost);

    if (void* existing = sessionByClient(ah, client)) {
        logInfo("[restore] client {} already has session {} — noop", client, existing);
        return 0;
    }

    const std::uint32_t mode = *reinterpret_cast<std::uint32_t*>(*reinterpret_cast<std::uintptr_t*>(ah) - 4);

    void* session = mNew(kSessionSize);
    if (!session) {
        logError("operator new({}) failed", kSessionSize);
        return 0;
    }
    mCtor(session, client, mode);
    brpAssign(reinterpret_cast<void**>(static_cast<char*>(session) + kSessionAgentHostOff), agentHost);

    if (!mAttachSessionFull || !mAttachSessionFull(agentHost, session)) {
        if (!mAttachSession(agentHost, session)) {
            logWarn("AttachSession returned false — destroying session");
            mDtor(session, 1);
            return 0;
        }
        restoreAppHandlers(agentHost, session);
    }

    pushSession(ah, session);
    emplaceByClient(ah, client, session);
    if (mInjN < static_cast<int>(mInjected.size()))
        mInjected[mInjN++] = {ah, session};

    const bool firstAttach = markSeenAh(ah);
    if (mRendererChannel) {
        char* ch    = ah + kRendererChannelOff;
        void* proxy = rd(ch + kAgentProxyOff);
        if (!firstAttach && mUpdateRenderer) {
            logInfo("[restore] re-attach: UpdateRendererChannel(force) (session={})", session);
            mUpdateRenderer(ah, 1);
        } else if (proxy) {
            onAttachToAgent(session, proxy, 0);
        } else if (mUpdateRenderer) {
            mUpdateRenderer(ah, 1);
        }
    }

    logInfo("attached: session={} client={}", session, client);
    return 1;
}

void NativeDevTools::restoreAppHandlers(void* agentHost, void* session) {
    if (!mAppHandlers || !session)
        return;
    void* loop =
            rd(static_cast<char*>(session) + kSessionUvHandleOff + kUvHandleLoopOff); // uv_handle_get_loop(session+24)

    if (void* sh = mNew(kStorageHandlerSize)) {
        mStorageCtor(sh, agentHost, loop);
        void* slot = sh;
        mAddHandler(session, &slot);
        logInfo("[apph] + StorageHandler {} -> session {}", sh, session);
    }
    if (void* swh = mNew(kSvcWorkerHandlerSize)) {
        mSvcWorkerCtor(swh);
        void* slot = swh;
        mAddHandler(session, &slot);
        logInfo("[apph] + ServiceWorkerHandler {} -> session {}", swh, session);
    }
    if (void* ih = mNew(kInspectorHandlerSize)) {
        mInspectorCtor(ih);
        void* slot = ih;
        mAddHandler(session, &slot);
        logInfo("[apph] + InspectorHandler {} -> session {}", ih, session);
    }
}

void* NativeDevTools::sessionByClient(char* ah, void* client) {
    char*               v3  = *reinterpret_cast<char**>(ah + kByClientMapOff);
    char*               end = *reinterpret_cast<char**>(ah + kByClientMapOff + 8);
    const std::uint64_t key = reinterpret_cast<std::uint64_t>(client);
    std::uint64_t       v4  = static_cast<std::uint64_t>(end - v3);
    if (v4 != 0) {
        std::uint64_t v5 = v4 >> 4, v6 = v5;
        do {
            v6 >>= 1;
            const std::uint64_t v7 = 16 * v6;
            if (*reinterpret_cast<std::uint64_t*>(v3 + v7) < key) {
                v6 = v5 + ~v6;
                v3 += v7 + 16;
            }
            v5 = v6;
        } while (v6 != 0);
    }
    if (v3 != end && *reinterpret_cast<std::uint64_t*>(v3) == key && *reinterpret_cast<std::uint64_t*>(v3 + 8) != 0) {
        return *reinterpret_cast<void**>(v3 + 8);
    }
    return nullptr;
}

void NativeDevTools::brpAssign(void** dst, void* ptr) {
    const std::uint64_t pool = *reinterpret_cast<std::uint64_t*>(mBrpPoolBase);
    if ((reinterpret_cast<std::uint64_t>(ptr) & kBrpMask) == pool)
        mBrpAcquire(ptr);
    *dst = ptr;
}

void NativeDevTools::pushSession(char* ah, void* session) {
    char* begin = *reinterpret_cast<char**>(ah + kSessionsVecOff);
    char* end   = *reinterpret_cast<char**>(ah + kSessionsVecOff + 8);
    char* cap   = *reinterpret_cast<char**>(ah + kSessionsVecOff + 16);
    if (end >= cap) { // grow (QQ allocator; raw_ptr move == bit copy)
        const std::size_t oldBytes = static_cast<std::size_t>(end - begin);
        const std::size_t cnt      = oldBytes / 8;
        const std::size_t newCnt   = cnt ? cnt * 2 : 1;
        char*             nb       = static_cast<char*>(mNew(newCnt * 8));
        std::memcpy(nb, begin, oldBytes);
        if (begin)
            mDelete(begin, static_cast<std::size_t>(cap - begin));
        begin                                                = nb;
        end                                                  = nb + oldBytes;
        cap                                                  = nb + newCnt * 8;
        *reinterpret_cast<char**>(ah + kSessionsVecOff)      = begin;
        *reinterpret_cast<char**>(ah + kSessionsVecOff + 16) = cap;
    }
    brpAssign(reinterpret_cast<void**>(end), session);
    *reinterpret_cast<char**>(ah + kSessionsVecOff + 8) = end + 8;
}

void NativeDevTools::emplaceByClient(char* ah, void* client, void* session) {
    char*               v3    = *reinterpret_cast<char**>(ah + kByClientMapOff);
    char*               end   = *reinterpret_cast<char**>(ah + kByClientMapOff + 8);
    char*               cap   = *reinterpret_cast<char**>(ah + kByClientMapOff + 16);
    char*               begin = v3;
    const std::uint64_t key   = reinterpret_cast<std::uint64_t>(client);
    {
        std::uint64_t v4 = static_cast<std::uint64_t>(end - v3);
        if (v4 != 0) {
            std::uint64_t v5 = v4 >> 4, v6 = v5;
            do {
                v6 >>= 1;
                const std::uint64_t v7 = 16 * v6;
                if (*reinterpret_cast<std::uint64_t*>(v3 + v7) < key) {
                    v6 = v5 + ~v6;
                    v3 += v7 + 16;
                }
                v5 = v6;
            } while (v6 != 0);
        }
    }
    char* pos = v3;

    if (end < cap) {
        std::memmove(pos + 16, pos, static_cast<std::size_t>(end - pos));
        *reinterpret_cast<std::uint64_t*>(pos)              = key;
        *reinterpret_cast<std::uint64_t*>(pos + 8)          = reinterpret_cast<std::uint64_t>(session);
        *reinterpret_cast<char**>(ah + kByClientMapOff + 8) = end + 16;
    } else {
        const std::size_t oldBytes = static_cast<std::size_t>(end - begin);
        const std::size_t pre      = static_cast<std::size_t>(pos - begin);
        const std::size_t cnt      = oldBytes / 16;
        const std::size_t newCnt   = cnt ? cnt * 2 : 1;
        char*             nb       = static_cast<char*>(mNew(newCnt * 16));
        std::memcpy(nb, begin, pre);
        *reinterpret_cast<std::uint64_t*>(nb + pre)     = key;
        *reinterpret_cast<std::uint64_t*>(nb + pre + 8) = reinterpret_cast<std::uint64_t>(session);
        std::memcpy(nb + pre + 16, pos, oldBytes - pre);
        if (begin)
            mDelete(begin, static_cast<std::size_t>(cap - begin));
        *reinterpret_cast<char**>(ah + kByClientMapOff)      = nb;
        *reinterpret_cast<char**>(ah + kByClientMapOff + 8)  = nb + oldBytes + 16;
        *reinterpret_cast<char**>(ah + kByClientMapOff + 16) = nb + newCnt * 16;
    }
}

void NativeDevTools::eraseVec8(char* base, void* val) {
    char* begin = *reinterpret_cast<char**>(base);
    char* end   = *reinterpret_cast<char**>(base + 8);
    if (!begin || !end || end < begin || (end - begin) >= 0x4000)
        return;
    for (char* p = begin; p + 8 <= end; p += 8) {
        if (*reinterpret_cast<void**>(p) == val) {
            std::memmove(p, p + 8, static_cast<std::size_t>(end - (p + 8)));
            *reinterpret_cast<char**>(base + 8) = end - 8;
            return;
        }
    }
}

void NativeDevTools::eraseMap16(char* base, void* key) {
    char* begin = *reinterpret_cast<char**>(base);
    char* end   = *reinterpret_cast<char**>(base + 8);
    if (!begin || !end || end < begin || (end - begin) >= 0x8000)
        return;
    for (char* p = begin; p + 16 <= end; p += 16) {
        if (*reinterpret_cast<void**>(p) == key) {
            std::memmove(p, p + 16, static_cast<std::size_t>(end - (p + 16)));
            *reinterpret_cast<char**>(base + 8) = end - 16;
            return;
        }
    }
}

int NativeDevTools::unlinkInjectedOn(char* ah, void** outDestroy, int cap) {
    int n = 0;
    if (!ah)
        return 0;
    for (int i = 0; i < mInjN && n < cap; ++i) {
        if (mInjected[i].ah != ah || !mInjected[i].session)
            continue;
        void* session = mInjected[i].session;
        // Must still be in sessions_ (not already removed by QQ).
        char* sb      = *reinterpret_cast<char**>(ah + kSessionsVecOff);
        char* se      = *reinterpret_cast<char**>(ah + kSessionsVecOff + 8);
        bool  present = false;
        if (sb && se && se >= sb && (se - sb) < 0x4000) {
            for (char* p = sb; p + 8 <= se; p += 8)
                if (*reinterpret_cast<void**>(p) == session) {
                    present = true;
                    break;
                }
        }
        if (!present) {
            mInjected[i].session = nullptr;
            continue;
        }
        void* client = rd(static_cast<char*>(session) + 0x20); // client_ @ session+0x20
        eraseVec8(ah + kSessionsVecOff, session);
        if (client)
            eraseMap16(ah + kByClientMapOff, client);
        outDestroy[n++]      = session;
        mInjected[i].session = nullptr;
    }
    return n;
}

void NativeDevTools::destroySession(void* session) {
    if (!session)
        return;
    if (!platform::guardedDestroy(mDtor, session))
        logWarn("destroying session {} threw — leaking to stay safe", session);
}

bool NativeDevTools::isInjectedSession(void* session) const {
    if (!session)
        return false;
    for (int i = 0; i < mInjN; ++i)
        if (mInjected[i].session == session)
            return true;
    return false;
}

void NativeDevTools::detachClientCleanup(void* agentHost, void* client) {
    char* ah      = static_cast<char*>(agentHost);
    void* session = sessionByClient(ah, client);
    if (!session || !isInjectedSession(session))
        return;
    eraseVec8(ah + kSessionsVecOff, session);
    eraseMap16(ah + kByClientMapOff, client);
    for (int i = 0; i < mInjN; ++i)
        if (mInjected[i].session == session)
            mInjected[i].session = nullptr;
    destroySession(session);
    logInfo("[detach] DetachClient cleanup: session {} (ah={} client={})", session, agentHost, client);
}

} // namespace ndt
