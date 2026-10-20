#include "NativeDevTools.h"

#include <cstdint>

namespace ndt {

using namespace sym;

void NativeDevTools::onAttachToAgent(void* session, void* agent, char forceIo) {
    char* s = static_cast<char*>(session);
    if (rd(s + kRecvEcOff))
        return; // already attached
    if (!agent)
        return; // renderer frame not ready
    buildRendererChannel(s, agent, forceIo);
}

void NativeDevTools::buildRendererChannel(char* session, void* agent, char forceIo) {
    session[kUseIoOff]  = static_cast<char>(forceIo & 1);
    const auto nameHost = reinterpret_cast<const void*>(mNameSessionHost);
    const auto nameSess = reinterpret_cast<const void*>(mNameSession);
    const auto cbInfo   = reinterpret_cast<void*>(mCbInfo);
    const auto cbName   = reinterpret_cast<void*>(mCbName);

    void *hHostPass = nullptr, *hHostBind = nullptr;
    mSiehCtor(&hHostPass);
    mSiehCtor(&hHostBind);
    mCreatePair(&hHostPass, &hHostBind);
    void* vHost = mNew(8);
    mPassThroughCtor(vHost);            // RequestValidator(PassThroughFilter)
    std::uint64_t spanHost[2] = {0, 0}; // sync-method ordinals span (none)
    mHostBind(session + kRecvEcOff,
              &hHostBind,
              session + kRecvStubOff,
              &vHost,
              spanHost,
              nullptr,
              0u,
              nameHost,
              cbInfo,
              cbName);
    struct {
        void*         handle;
        std::uint32_t version;
        std::uint32_t pad;
    } hostPending{hHostPass, 0, 0};

    void *hSessPass = nullptr, *hSessBind = nullptr;
    mSiehCtor(&hSessPass);
    mSiehCtor(&hSessBind);
    mCreatePair(&hSessPass, &hSessBind);
    void* vSess = mNew(8);
    mPassThroughCtor(vSess);
    mSessBind(session + kSessEcOff, &hSessBind, 0u, &vSess, nullptr, nameSess, cbInfo, cbName);
    void* sessEc = rd(session + kSessEcOff);
    if (sessEc) {
        void* sessProxy = mNew(16);
        mProxyCtor(sessProxy, sessEc);
        *reinterpret_cast<void**>(session + kSessProxyOff) = sessProxy;
    }

    std::uint64_t pipe0 = 0, pipe1 = 0;
    if (mMojoCreatePipe(nullptr, &pipe0, &pipe1) != 0)
        return;
    std::uint64_t ioRemoteState[2] = {pipe0, 0};
    mIoBind(session + kIoBaseOff, ioRemoteState, nullptr);
    std::uint64_t ioHolder[4] = {pipe1, 0, 0, 0};

    void*        sessArg       = hSessPass;
    void*        statePtr      = nullptr;
    std::uint8_t scriptStr[24] = {0};
    std::uint8_t sessIdStr[24] = {0};
    mAttachSend(agent, &hostPending, &sessArg, ioHolder, &statePtr, scriptStr, 0, 1, sessIdStr, 0);

    session[kSuspendFlagOff] = 0; // suspended_sending_messages_to_agent_ = false
    logDebug("[channel] renderer channel attached: session={} agent={} force_io={}",
             static_cast<const void*>(session),
             static_cast<const void*>(agent),
             forceIo & 1);
}

} // namespace ndt
