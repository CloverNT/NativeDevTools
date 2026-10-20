#include "NativeDevTools.h"
#include "Platform.h"

#include <filesystem>
#include <string>
#include <system_error>
#include <vector>

#include <CloverNT/API/Plugin/PluginManager.hpp>
#include <CloverNT/API/Plugin/RegisterHelper.hpp>

namespace ndt {

Memory::Address NativeDevTools::scanText(const Memory::SignatureView sig, const std::string_view what) {
    const auto r = Memory::findFirstInSection(sym::kModule, sym::kTextSection, sig);
    if (!r) {
        logError("signature not found: {}", what);
        mResolveFailed = true;
        return 0;
    }
    return r.get();
}

Memory::Address NativeDevTools::scanRel(const sym::RelRef& ref, const std::string_view what) {
    const auto r = Memory::findFirstInSection(sym::kModule, sym::kTextSection, ref.sig);
    if (!r) {
        logError("anchor not found: {}", what);
        mResolveFailed = true;
        return 0;
    }
    const auto target = r.rel(ref.rel);
    if (!target) {
        logError("rel32 decode failed: {}", what);
        mResolveFailed = true;
        return 0;
    }
    return *target;
}

Memory::Address NativeDevTools::scanAsciiz(const std::string_view text, const std::string_view what) {
    static constexpr char kHex[] = "0123456789ABCDEF";
    std::string           pattern;
    pattern.reserve(text.size() * 3 + 2);
    for (const char c: text) {
        const auto b = static_cast<unsigned char>(c);
        pattern.push_back(kHex[b >> 4]);
        pattern.push_back(kHex[b & 0xF]);
        pattern.push_back(' ');
    }
    pattern += "00";
    const auto sig = Memory::parseSignature(pattern);
    if (!sig) {
        logError("bad asciiz pattern: {}", what);
        mResolveFailed = true;
        return 0;
    }
    const auto r = Memory::findFirstInSection(sym::kModule, sym::kRdataSection, *sig);
    if (!r) {
        logError("string not found: {}", what);
        mResolveFailed = true;
        return 0;
    }
    return r.get();
}

bool NativeDevTools::resolveSessionSymbols() {
    mResolveFailed = false;
    mNew           = scanFn<sym::NewFn>(sym::kOperatorNew, "operator new");
    mCtor          = scanFn<sym::CtorFn>(sym::kSessionCtor, "DevToolsSession ctor");
    mAttachSession = scanFn<sym::AttachSessionFn>(sym::kAttachSession, "AttachSession");
    mBrpAcquire    = scanFn<sym::BrpAcquireFn>(sym::kBrpAcquire, "raw_ptr acquire");
    mDtor          = scanFn<sym::DtorFn>(sym::kSessionDtor, "session dtor");
    mDelete        = scanFn<sym::DeleteFn>(sym::kOperatorDelete, "operator delete");
    mBrpPoolBase   = scanRel(sym::kBrpPoolBaseRef, "BRP pool base");
    return !mResolveFailed;
}

bool NativeDevTools::resolveChannelSymbols() {
    mResolveFailed   = false;
    mUpdateRenderer  = scanFn<sym::UpdateRendererFn>(sym::kUpdateRenderer, "UpdateRendererChannel");
    mSiehCtor        = scanFn<sym::SiehCtorFn>(sym::kSiehCtor, "SIEH ctor");
    mCreatePair      = scanFn<sym::CreatePairFn>(sym::kCreatePair, "CreatePair");
    mHostBind        = scanFn<sym::HostBindFn>(sym::kHostBind, "HostBind");
    mSessBind        = scanFn<sym::SessBindFn>(sym::kSessBind, "SessBind");
    mIoBind          = scanFn<sym::IoBindFn>(sym::kIoBind, "IoBind");
    mMojoCreatePipe  = scanFn<sym::MojoCreatePipeFn>(sym::kMojoCreatePipe, "MojoCreateMessagePipe");
    mPassThroughCtor = scanFn<sym::PassThroughCtorFn>(sym::kPassThroughCtor, "PassThroughFilter ctor");
    mProxyCtor       = scanFn<sym::ProxyCtorFn>(sym::kProxyCtor, "DevToolsSession::Proxy ctor");
    mAttachSend      = scanFn<sym::AttachSendFn>(sym::kAttachSend, "AttachDevToolsSession");
    mNameSessionHost = scanAsciiz(sym::kNameDevToolsSessionHost, "name DevToolsSessionHost");
    mNameSession     = scanAsciiz(sym::kNameDevToolsSession, "name DevToolsSession");
    mCbInfo          = mBase + sym::kCbMethodInfoRva; // residual RVA (no scannable anchor)
    mCbName          = mBase + sym::kCbMethodNameRva;
    return !mResolveFailed;
}

bool NativeDevTools::resolveAppHandlers() {
    const bool saved = mResolveFailed;
    mStorageCtor     = scanRelFn<sym::StorageCtorFn>(sym::kStorageCtorCall, "StorageHandler ctor");
    mSvcWorkerCtor   = scanRelFn<sym::SvcWorkerCtorFn>(sym::kSvcWorkerCtorCall, "ServiceWorkerHandler ctor");
    mInspectorCtor   = scanRelFn<sym::InspectorCtorFn>(sym::kInspectorCtorCall, "InspectorHandler ctor");
    mAddHandler      = scanFn<sym::AddHandlerFn>(sym::kAddHandler, "AddHandler");
    const bool ok    = mStorageCtor && mSvcWorkerCtor && mInspectorCtor && mAddHandler;
    mResolveFailed   = saved;
    return ok;
}

bool NativeDevTools::load() {
    logInfo("load from {}", directory().string());

    const auto mod = Memory::findModule(sym::kModule);
    if (!mod) {
        logInfo("{} not present — safe exit (not the browser process)", sym::kModule);
        return true;
    }
    mBase = mod->base();
    logInfo("{} @ {:#x} ({} bytes)", sym::kModule, mBase, mod->size());

    platform::pinSelfModule();

    if (!resolveSessionSymbols()) {
        logWarn("core signatures not found — installing no hooks, safe exit");
        return true;
    }

    {
        const bool saved   = mResolveFailed;
        mAttachSessionFull = scanFn<sym::AttachSessionFn>(sym::kAttachSessionFull, "full AttachSession builder");
        mResolveFailed     = saved;
    }
    mAppHandlers = resolveAppHandlers();

    mRendererChannel = resolveChannelSymbols();
    if (mRendererChannel) {
        installAttachToAgentHook();
        mRendererChannel = static_cast<bool>(mAttachHook);
    }
    if (!mRendererChannel)
        logWarn("renderer channel unavailable — session bookkeeping only");

    installDrainHook();             // unlink injected sessions before the close storm
    installDetachHook();            // rebuild the gutted DetachClient cleanup
    installAutoAttachWiringFix();   // auto_attacher fix (setAutoAttach)
    installReadyToCommitHook();     // rebuild DevToolsFrontendHost::Create
    installCloseDevToolsHook();     // close funnel, CDP teardown before native teardown
    installBundledDataSourceHook(); // devtools:// bundled resource supply
    installF12Hook();               // F12 toggle devtools
    installAttachClientHook();
    return true;
}

void NativeDevTools::installAttachClientHook() {
    const Memory::Address entry = scanText(sym::kAttachClient, "AttachClient");
    if (!entry)
        return;
    const auto owner = Plugin::PluginManager::getPlugin(name());
    auto       hook  = Memory::Hook::Inline::create<sym::AttachClientFn>(
            entry,
            [this](auto& /*original*/, void* agentHost, void* client) -> std::uint64_t {
                return restoreAttach(agentHost, client); // rebuild the gutted session bookkeeping
            },
            Memory::Hook::Options{.owner = owner});
    if (!hook) {
        logError("hook AttachClient failed: {}", hook.error().message);
        return;
    }
    mHook = std::make_unique<decltype(hook)::value_type>(std::move(hook.value()));
    logInfo("hooked AttachClient @ {:#x}", entry);
}

void NativeDevTools::installDrainHook() {
    const Memory::Address entry = scanText(sym::kForceDetachAll, "ForceDetachAllSessions");
    if (!entry) {
        logWarn("ForceDetachAllSessions not found — drain fix disabled");
        return;
    }
    const auto owner = Plugin::PluginManager::getPlugin(name());
    auto       hook  = Memory::Hook::Inline::create<sym::ForceDetachAllFn>(
            entry,
            [this](auto& original, void* ah, void* out) -> void* {
                void*     collected[8];
                const int n = unlinkInjectedOn(static_cast<char*>(ah), collected, 8);
                for (int i = 0; i < n; ++i)
                    destroySession(collected[i]);
                return original(ah, out);
            },
            Memory::Hook::Options{.owner = owner});
    if (!hook) {
        logError("hook ForceDetachAllSessions failed: {}", hook.error().message);
        return;
    }
    mDrainHook = std::make_unique<decltype(hook)::value_type>(std::move(hook.value()));
    logInfo("hooked ForceDetachAllSessions @ {:#x}", entry);
}

void NativeDevTools::installAttachToAgentHook() {
    const Memory::Address entry = scanRel(sym::kAttachToAgentCall, "AttachToAgent");
    if (!entry)
        return;
    const auto owner = Plugin::PluginManager::getPlugin(name());
    auto       hook  = Memory::Hook::Inline::create<sym::AttachToAgentFn>(
            entry,
            [this](auto& original, void* session, void* agent, char forceIo) -> void {
                if (isInjectedSession(session))
                    onAttachToAgent(session, agent, forceIo);
                original(session, agent, forceIo);
            },
            Memory::Hook::Options{.owner = owner});
    if (!hook) {
        logError("hook AttachToAgent failed: {}", hook.error().message);
        return;
    }
    mAttachHook = std::make_unique<decltype(hook)::value_type>(std::move(hook.value()));
    logInfo("hooked AttachToAgent @ {:#x}", entry);
}

void NativeDevTools::installDetachHook() {
    const Memory::Address entry = scanText(sym::kDebuggerDetach, "Debugger::Detach");
    if (!entry) {
        logWarn("Debugger::Detach not found — DetachClient restoration disabled");
        return;
    }
    const auto owner = Plugin::PluginManager::getPlugin(name());
    auto       hook  = Memory::Hook::Inline::create<sym::DebuggerDetachFn>(
            entry,
            [this](auto& original, void* dbg) -> std::uint64_t {
                if (dbg) {
                    void* ah = rd(static_cast<char*>(dbg) + sym::kDbgAgentHostOff);
                    if (ah)
                        detachClientCleanup(ah, static_cast<char*>(dbg) + sym::kDbgClientOff);
                }
                return original(dbg);
            },
            Memory::Hook::Options{.owner = owner});
    if (!hook) {
        logError("hook Debugger::Detach failed: {}", hook.error().message);
        return;
    }
    mDetachHook = std::make_unique<decltype(hook)::value_type>(std::move(hook.value()));
    logInfo("hooked Debugger::Detach @ {:#x}", entry);
}

void NativeDevTools::installAutoAttachWiringFix() {
    const Memory::Address site = scanText(sym::kAutoAttachWiring, "AttachSession auto_attacher wiring");
    if (!site)
        return;
    const Memory::Address disp = site + sym::kAutoAttachDispOff;
    const std::uint8_t    cur  = rd<std::uint8_t>(reinterpret_cast<void*>(disp));
    if (cur != sym::kAutoAttachBugByte) {
        logWarn("[autoattach] unexpected displacement byte {:#x} @ {:#x} — skip (already fixed?)", cur, site);
        return;
    }
    const auto owner = Plugin::PluginManager::getPlugin(name());
    auto       patch = Memory::createPatch(
            disp, std::vector<std::uint8_t>{sym::kAutoAttachFixByte}, Memory::ResourceOptions{.owner = owner});
    if (!patch) {
        logError("[autoattach] patch failed: {}", patch.error().message);
        return;
    }
    mAutoAttachPatch = std::move(patch.value());
    logInfo("[autoattach] auto_attacher wiring fixed @ {:#x}", site);
}

void NativeDevTools::installCloseDevToolsHook() {
    const Memory::Address entry = scanText(sym::kIswcCloseDevToolsOrch, "ISWC::CloseDevTools(orch)");
    if (!entry) {
        logWarn("ISWC::CloseDevTools(orch) not found — close-funnel teardown disabled");
        return;
    }
    const auto owner = Plugin::PluginManager::getPlugin(name());
    auto       hook  = Memory::Hook::Inline::create<sym::IswcCloseDevToolsOrchFn>(
            entry,
            [this](auto& original, void* iswc) -> void {
                teardownCdpClient(iswc);
                original(iswc);
            },
            Memory::Hook::Options{.owner = owner});
    if (!hook) {
        logError("hook ISWC::CloseDevTools failed: {}", hook.error().message);
        return;
    }
    mCloseDevToolsHook = std::make_unique<decltype(hook)::value_type>(std::move(hook.value()));
    logInfo("hooked ISWC::CloseDevTools(orch) @ {:#x}", entry);
}

} // namespace ndt

using ndt::NativeDevTools;
CloverNT_REGISTER_PLUGIN(NativeDevTools)
