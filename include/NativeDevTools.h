#pragma once

#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <format>
#include <map>
#include <memory>
#include <string>
#include <string_view>
#include <utility>

#include <CloverNT/API/Logger.hpp>
#include <CloverNT/API/Memory/Hook.hpp>
#include <CloverNT/API/Memory/Memory.hpp>
#include <CloverNT/API/Plugin/PluginBase.hpp>

#include "Platform.h"
#include "Symbols.h"

namespace ndt {

using namespace CloverNT;

class NativeDevTools : public Plugin::PluginBase {
public:
    bool load();

    static void sHostImplDispatch(void* implThis, void* dictValue); // DispatchEmbedderMessage
    static void sHostImplDtor(void* implThis);                      // dtor

    static void
    sCdpDispatch(void* client, void* agentHost, void* span);        // DispatchProtocolMessage
    static void sCdpAgentHostClosed(void* client, void* agentHost); // AgentHostClosed

private:
    bool resolveSessionSymbols();
    bool resolveChannelSymbols();
    bool resolveAppHandlers();
    void installAttachClientHook();
    void installDrainHook();
    void installAttachToAgentHook();
    void installDetachHook();
    void installAutoAttachWiringFix();
    void installReadyToCommitHook();
    void installCloseDevToolsHook();
    void installBundledDataSourceHook();
    void installF12Hook();
    void resolveContextEntry();
    bool enterNodeContext(void* isolate, void* hsBuf, void*& ctxOut);
    void exitNodeContext(void* ctxLocalVal, void* hsBuf);

    bool                 loadBundledPak();
    const std::string*   decodeResource(const std::string& rel);
    std::string          committingUrl(void* navReq) const;
    static std::string   readLibcxxString(const void* strPtr);
    static std::uint32_t valueTag(const void* value);
    static const void*   dictFindValue(const void* dict, std::string_view key);
    const std::string&   compatScript();
    void                 onFrontendFrameCommitted(void* rfh, const std::string& url);
    void                 sendSetupDevToolsFrontend(void* frontendEc, const std::string& apiScript, void* hHostPass);
    void                 runFrontendJs(void* rfh, const std::string& asciiScript);
    void*                freshFrontendRfh(void* pageIswc);
    void                 sendCdpToPage(int pageId, const void* data, std::size_t len);
    void                 applyInspectedPageBounds(int pageId, int x, int y, int w, int h);
    void                 closeDock(int pageId);
    void                 clearHostChannelRfh(void* deadRfh);

    std::uint64_t restoreAttach(void* agentHost, void* client);
    void          restoreAppHandlers(void* agentHost, void* session);
    void*         sessionByClient(char* ah, void* client);
    void          brpAssign(void** dst, void* ptr);
    void          pushSession(char* ah, void* session);
    void          emplaceByClient(char* ah, void* client, void* session);
    void          eraseVec8(char* base, void* val);
    void          eraseMap16(char* base, void* key);
    int           unlinkInjectedOn(char* ah, void** outDestroy, int cap);
    void          destroySession(void* session);
    bool          isInjectedSession(void* session) const;
    void          detachClientCleanup(void* agentHost, void* client);

    void onAttachToAgent(void* session, void* agent, char forceIo);
    void buildRendererChannel(char* session, void* agent, char forceIo);

    void setupCdpClient(int pageId, void* pageIswc, void* frontendRfh);
    void teardownCdpClient(void* pageIswc);
    void releaseAgentHost(void* host);
    bool hasCdpClientForPage(int pageId) const;

    void* createDevToolsWc(void* pageWc, void* pageIswc, const std::string& url);
    void  createAndDockDevTools(int pageId);

    Memory::Address scanText(Memory::SignatureView sig, std::string_view what);
    template <class Fn>
    Fn* scanFn(const Memory::SignatureView sig, const std::string_view what) {
        return reinterpret_cast<Fn*>(scanText(sig, what));
    }
    Memory::Address scanRel(const sym::RelRef& ref, std::string_view what);
    template <class Fn>
    Fn* scanRelFn(const sym::RelRef& ref, const std::string_view what) {
        return reinterpret_cast<Fn*>(scanRel(ref, what));
    }
    Memory::Address scanAsciiz(std::string_view text, std::string_view what);

    template <class T = void*>
    static T rd(const void* p) {
        return *reinterpret_cast<const T*>(p);
    }

    template <class... Args>
    void logInfo(std::format_string<Args...> fmt, Args&&... args) const {
        logger().info(fmt, std::forward<Args>(args)...);
    }
    template <class... Args>
    void logWarn(std::format_string<Args...> fmt, Args&&... args) const {
        logger().warn(fmt, std::forward<Args>(args)...);
    }
    template <class... Args>
    void logError(std::format_string<Args...> fmt, Args&&... args) const {
        logger().error(fmt, std::forward<Args>(args)...);
    }
    template <class... Args>
    void logDebug([[maybe_unused]] std::format_string<Args...> fmt, [[maybe_unused]] Args&&... args) const {
#ifdef CloverNT_DEBUG
        logger().debug(fmt, std::forward<Args>(args)...);
#endif
    }

    Memory::Address mBase{};
    bool            mResolveFailed{};
    bool            mRendererChannel{};
    bool            mAppHandlers{};

    sym::NewFn*           mNew{};
    sym::CtorFn*          mCtor{};
    sym::AttachSessionFn* mAttachSession{};
    sym::AttachSessionFn* mAttachSessionFull{};
    sym::BrpAcquireFn*    mBrpAcquire{};
    sym::DtorFn*          mDtor{};
    sym::DeleteFn*        mDelete{};
    sym::StorageCtorFn*   mStorageCtor{};
    sym::SvcWorkerCtorFn* mSvcWorkerCtor{};
    sym::InspectorCtorFn* mInspectorCtor{};
    sym::AddHandlerFn*    mAddHandler{};

    sym::UpdateRendererFn*  mUpdateRenderer{};
    sym::SiehCtorFn*        mSiehCtor{};
    sym::CreatePairFn*      mCreatePair{};
    sym::HostBindFn*        mHostBind{};
    sym::SessBindFn*        mSessBind{};
    sym::IoBindFn*          mIoBind{};
    sym::MojoCreatePipeFn*  mMojoCreatePipe{};
    sym::PassThroughCtorFn* mPassThroughCtor{};
    sym::ProxyCtorFn*       mProxyCtor{};
    sym::AttachSendFn*      mAttachSend{};

    sym::GetRenderFrameHostFn* mGetRenderFrameHost{};
    sym::GetRemoteAssocFn*     mGetRemoteAssoc{};
    sym::GetAssocProxyFn*      mGetAssocProxy{};
    sym::MojoMsgCtorFn*        mMojoMsgCtor{};
    sym::MsgAllocFn*           mMsgAlloc{};
    sym::HdrInitFn*            mHdrInit24{};
    sym::SerStringFn*          mSerString{};
    sym::SerRemoteFn*          mSerRemote{};
    sym::SiehCopyFn*           mSiehCopy{};
    sym::SendMsgFn*            mSendMsg{};
    sym::MsgDtorFn*            mMsgDtor{};
    sym::DictDtorFn*           mDictDtor{};

    sym::FromIdFn*                mFromId{};
    sym::IswcShowDevToolsFn*      mIswcShowDevTools{};
    sym::IswcLayoutFn*            mIswcLayout{};
    sym::IswcCloseDevToolsFn*     mIswcCloseDevTools{};
    sym::IswcSetIsDockedFn*       mIswcSetIsDocked{};
    sym::IswcCloseDevToolsOrchFn* mIswcCloseDevToolsOrch{};

    sym::RefCountedMemCtorFn*                                        mRefCountedMemCtor{};
    sym::CallbackRunFn*                                              mCallbackRun{};
    sym::BrotliDecompressFn*                                         mBrotli{};
    std::string                                                      mPakData;
    std::map<std::uint16_t, std::pair<std::uint32_t, std::uint32_t>> mPakIndex;
    std::map<std::string, std::uint16_t>                             mPathToId;

    sym::FromOrCreateFn*      mFromOrCreate{};
    sym::IsolateGetCurrentFn* mIsolateGetCurrent{};

    sym::IsolateGetCurrentContextFn* mGetCurrentContext{};
    sym::HandleScopeCtorFn*          mHandleScopeCtor{};
    sym::HandleScopeDtorFn*          mHandleScopeDtor{};
    sym::ContextEnterFn*             mContextEnter{};
    sym::ContextExitFn*              mContextExit{};
    sym::GetCurrentEnvFn*            mGetCurrentEnv{};
    sym::GetMainContextFn*           mGetMainContext{};
    std::atomic<void*>               mNodeEnv{nullptr};

    sym::GetOrCreateForFn*    mGetOrCreateFor{};
    sym::DispatchProtoSendFn* mDispatchProtoSend{};
    sym::AttachClientFn*      mAttachClientCall{};
    bool                      mCdp{};

    sym::GetBrowserContextFn* mGetBrowserContext{};
    sym::CreateParamsFn*      mCreateParams{};
    sym::WcCreateFn*          mWcCreate{};
    sym::GurlCtorFn*          mGurlCtor{};
    sym::LoadUrlParamsFn*     mLoadUrlParams{};
    sym::LoadUrlWithParamsFn* mLoadUrlWithParams{};
    sym::ExecJsForTestsFn*    mExecJs{};
    bool                      mNativeWcPrims{};

    Memory::Address mWebContentsTable{};
    Memory::Address mBrpPoolBase{};
    Memory::Address mFhStubVtable{};
    Memory::Address mFhAcceptWrapper{};
    Memory::Address mCbInfo{};
    Memory::Address mCbName{};
    Memory::Address mNameSessionHost{};
    Memory::Address mNameSession{};
    Memory::Address mNameFrontend{};
    Memory::Address mNameFrontendHost{};

    std::string mCompatScript;
    bool        mCompatLoaded{};

    std::unique_ptr<Memory::Hook::InlineHandle<sym::AttachClientFn>>          mHook;
    std::unique_ptr<Memory::Hook::InlineHandle<sym::AttachToAgentFn>>         mAttachHook;
    std::unique_ptr<Memory::Hook::InlineHandle<sym::ForceDetachAllFn>>        mDrainHook;
    std::unique_ptr<Memory::Hook::InlineHandle<sym::DebuggerDetachFn>>        mDetachHook;
    Memory::PatchHandle                                                       mAutoAttachPatch;
    std::unique_ptr<Memory::Hook::InlineHandle<sym::ReadyToCommitFn>>         mReadyToCommitHook;
    std::unique_ptr<Memory::Hook::InlineHandle<sym::IswcCloseDevToolsOrchFn>> mCloseDevToolsHook;
    std::unique_ptr<Memory::Hook::InlineHandle<sym::BundledStartDataReqFn>>   mBundledHook;
    std::unique_ptr<Memory::Hook::InlineHandle<sym::PreHandleKeyboardFn>>     mF12Hook;
    std::unique_ptr<Memory::Hook::InlineHandle<sym::ContextEnterFn>>          mCtxEnterHook;


    struct Injected {
        char* ah;
        void* session;
    };
    std::array<Injected, 32> mInjected{};
    int                      mInjN{};

    std::array<void*, 32> mSeenAh{};
    int                   mSeenAhN{};
    bool                  markSeenAh(void* ah);

    std::array<void*, 16> mHostChannels{};
    int                   mHostChannelN{};

    std::array<void*, 16> mCdpClients{};
    int                   mCdpClientN{};
};

} // namespace ndt
