#include "NativeDevTools.h"

#include <fstream>

#include <nlohmann/json.hpp>

#include <CloverNT/API/Plugin/PluginManager.hpp>

namespace ndt {

using namespace sym;

namespace {

    struct HostChannel {
        const void*     implVtable; // this; impl vtable = gHostImplVtable; also stub.sink_'s target
        NativeDevTools* self;
        void*           hostEc;      // InterfaceEndpointClient (written by BindImpl)
        std::string     targetKey;   // route key (devtools target webContents id, from the URL)
        const void*     stubVtable;  // QQ DevToolsFrontendHost stub vtable
        void*           stubSink;    // == this (the impl)
        void*           frontendRfh; // frontend frame RFH (prefs/getHostConfig ack target)
    };

    const void* gHostImplVtable[2] = {
            reinterpret_cast<const void*>(&NativeDevTools::sHostImplDtor),
            reinterpret_cast<const void*>(&NativeDevTools::sHostImplDispatch),
    };

    // "<key>=<value>" from a URL (up to & or #); empty if absent.
    std::string parseKey(const std::string& url, std::string_view key) {
        const auto p = url.find(key);
        if (p == std::string::npos)
            return {};
        const auto s = p + key.size();
        const auto e = url.find_first_of("&#", s);
        return url.substr(s, e == std::string::npos ? std::string::npos : e - s);
    }
    std::string parseTargetKey(const std::string& url) {
        return parseKey(url, "clovernt_target=");
    }

    int toInt(std::string_view v) {
        if (v.empty() || v.size() > 9)
            return -1;
        int n = 0;
        for (const char c: v) {
            if (c < '0' || c > '9')
                return -1;
            n = n * 10 + (c - '0');
        }
        return n;
    }
    int parseIntKey(const std::string& url, std::string_view key) {
        return toInt(parseKey(url, key));
    }

    nlohmann::json        gDevPrefs = nlohmann::json::object();
    std::filesystem::path gPrefsPath;
    void                  loadPrefs(const std::filesystem::path& path) {
        gPrefsPath = path;
        std::error_code ec;
        if (std::filesystem::exists(gPrefsPath, ec)) {
            std::ifstream f(gPrefsPath);
            if (f) {
                auto j = nlohmann::json::parse(f, nullptr, false);
                if (j.is_object())
                    gDevPrefs = std::move(j);
            }
        }
        if (gDevPrefs.empty()) { // theme/locale defaults (else the theme sticks light and locale reloads)
            gDevPrefs["language"]                = "\"zh\"";
            gDevPrefs["disable-locale-info-bar"] = "true";
            gDevPrefs["chrome-theme-colors"]     = "false";
        }
    }
    void savePrefs() {
        if (gPrefsPath.empty())
            return;
        std::ofstream f(gPrefsPath, std::ios::trunc);
        if (f)
            f << gDevPrefs.dump();
    }

} // namespace

std::string NativeDevTools::readLibcxxString(const void* strPtr) {
    if (!strPtr)
        return {};
    const std::uint8_t flag = rd<std::uint8_t>(strPtr);
    const char*        data = nullptr;
    std::uint64_t      size = 0;
    if (flag & 1) {
        size = rd<std::uint64_t>(static_cast<const char*>(strPtr) + 8);
        data = rd<const char*>(static_cast<const char*>(strPtr) + 16);
    } else {
        data = static_cast<const char*>(strPtr) + 1;
        size = static_cast<std::uint64_t>(flag) >> 1;
    }
    if (!data || size == 0 || size > 8192)
        return {};
    return std::string(data, static_cast<std::size_t>(size));
}

std::string NativeDevTools::committingUrl(void* navReq) const {
    void* gurl = rd(static_cast<char*>(navReq) + kNavReqUrlOff);
    if (!gurl)
        return {};
    return readLibcxxString(gurl);
}

const std::string& NativeDevTools::compatScript() {
    if (mCompatLoaded)
        return mCompatScript;
    mCompatLoaded = true;
    if (const std::string* js = decodeResource("devtools_compatibility.js"))
        mCompatScript = *js;
    if (mCompatScript.empty())
        logWarn("[frontend] compat script unavailable (window.DevToolsHost unavailable)");
    else
        logInfo("[frontend] compat script loaded: {} bytes", mCompatScript.size());
    return mCompatScript;
}

std::uint32_t NativeDevTools::valueTag(const void* value) {
    if (!value)
        return 0xFFFFFFFFu;
    return rd<std::uint32_t>(static_cast<const char*>(value) + 24);
}

const void* NativeDevTools::dictFindValue(const void* dict, std::string_view key) {
    void* begin = rd(dict);
    void* end   = rd(static_cast<const char*>(dict) + 8);
    if (!begin || !end || end < begin)
        return nullptr;
    const std::int64_t n = (static_cast<char*>(end) - static_cast<char*>(begin)) / kDictElemStride;
    if (n <= 0 || n > 128)
        return nullptr;
    for (std::int64_t i = 0; i < n; ++i) {
        const char* elem = static_cast<char*>(begin) + i * kDictElemStride;
        if (readLibcxxString(elem) == key)
            return rd(elem + 24);
    }
    return nullptr;
}

void NativeDevTools::sHostImplDtor(void* /*implThis*/) {}

void NativeDevTools::sHostImplDispatch(void* implThis, void* dictValue) {
    auto*           ch   = reinterpret_cast<HostChannel*>(implThis);
    NativeDevTools* self = ch ? ch->self : nullptr;
    if (!self || !dictValue)
        return;
    const void*       mv     = dictFindValue(dictValue, "method");
    const std::string method = (mv && valueTag(mv) == 4) ? readLibcxxString(mv) : std::string{};
    const void*       iv     = dictFindValue(dictValue, "id");
    std::int64_t      id     = -1;
    if (iv && valueTag(iv) == 2)
        id = rd<std::int32_t>(iv);

    // params[0]{x,y,width,height}
    if (method == "setInspectedPageBounds") {
        if (const void* pv = dictFindValue(dictValue, "params"); pv && valueTag(pv) == 7) {
            void* pb = rd(pv);
            void* pe = rd(static_cast<const char*>(pv) + 8);
            if (pb && pe > pb) {
                const void* rect = pb; // list[0] = inline base::Value (dict)
                if (valueTag(rect) == 6) {
                    const auto gi = [](const void* d, std::string_view k) -> int {
                        const void* v = dictFindValue(d, k);
                        return (v && valueTag(v) == 2) ? rd<std::int32_t>(v) : 0;
                    };
                    self->applyInspectedPageBounds(
                            toInt(ch->targetKey), gi(rect, "x"), gi(rect, "y"), gi(rect, "width"), gi(rect, "height"));
                }
            }
        }
        if (self->mDictDtor)
            self->mDictDtor(dictValue);
        return;
    }

    if (method == "closeWindow") {
        self->closeDock(toInt(ch->targetKey));
        if (self->mDictDtor)
            self->mDictDtor(dictValue);
        return;
    }
    if (method == "dispatchProtocolMessage") {
        if (const void* pv = dictFindValue(dictValue, "params"); pv && valueTag(pv) == 7) {
            void* pb = rd(pv);
            void* pe = rd(static_cast<const char*>(pv) + 8);
            if (pb && pe > pb && valueTag(pb) == 4) {
                const std::string cdp = readLibcxxString(pb); // params[0] = CDP JSON string
                self->sendCdpToPage(toInt(ch->targetKey), cdp.data(), cdp.size());
            }
        }
        if (self->mDictDtor)
            self->mDictDtor(dictValue);
        return;
    }
    nlohmann::json params = nlohmann::json::array();
    if (const void* pv = dictFindValue(dictValue, "params"); pv && valueTag(pv) == 7) {
        void* pb = rd(pv);
        void* pe = rd(static_cast<const char*>(pv) + 8);
        if (pb && pe > pb) {
            const std::int64_t pn = (static_cast<char*>(pe) - static_cast<char*>(pb)) / kDictElemStride;
            for (std::int64_t i = 0; i < pn && i < 32; ++i) {
                const char* ev = static_cast<char*>(pb) + i * kDictElemStride;
                switch (valueTag(ev)) {
                case 1:
                    params.push_back(rd<std::uint8_t>(ev) != 0);
                    break;
                case 2:
                    params.push_back(rd<std::int32_t>(ev));
                    break;
                case 4:
                    params.push_back(readLibcxxString(ev));
                    break;
                default:
                    params.push_back(nullptr);
                    break;
                }
            }
        }
    }
    {
        void* ackRfh = nullptr;
        if (self->mFromId) {
            void* pApi  = self->mFromId(reinterpret_cast<void*>(self->mWebContentsTable), toInt(ch->targetKey));
            void* pIswc = pApi ? rd(static_cast<char*>(pApi) + kApiWcIswcOff) : nullptr;
            if (pIswc)
                ackRfh = self->freshFrontendRfh(pIswc);
        }
        void* const rfh = ackRfh;
        const auto  ack = [&](const nlohmann::json& obj) {
            if (id >= 0 && rfh)
                self->runFrontendJs(rfh,
                                    "DevToolsAPI.embedderMessageAck(" + std::to_string(id) + ", " +
                                            obj.dump(-1, ' ', true) + ")");
        };
        const std::string a0 =
                (params.size() > 0 && params[0].is_string()) ? params[0].get<std::string>() : std::string{};
        if (method == "getPreferences")
            ack(gDevPrefs);
        else if (method == "getPreference")
            ack(gDevPrefs.contains(a0) ? gDevPrefs[a0] : nlohmann::json(nullptr));
        else if (method == "setPreference") {
            if (!a0.empty()) {
                gDevPrefs[a0] = (params.size() > 1 && params[1].is_string()) ? params[1] : nlohmann::json("");
                savePrefs();
            }
        } else if (method == "removePreference") {
            gDevPrefs.erase(a0);
            savePrefs();
        } else if (method == "clearPreferences") {
            gDevPrefs = nlohmann::json::object();
            savePrefs();
        } else if (method == "getSyncInformation")
            ack(nlohmann::json{{"isSyncActive", false}});
        else if (method == "registerPreference") {
        } else if (method == "setIsDocked") {
            const bool docked = (params.size() > 0 && params[0].is_boolean()) ? params[0].get<bool>() : true;
            if (self->mIswcSetIsDocked && self->mFromId) {
                void* pApi  = self->mFromId(reinterpret_cast<void*>(self->mWebContentsTable), toInt(ch->targetKey));
                void* pIswc = pApi ? rd(static_cast<char*>(pApi) + kApiWcIswcOff) : nullptr;
                void* pView = pIswc ? rd(static_cast<char*>(pIswc) + kIswcViewOff) : nullptr;
                if (pView) {
                    // dev_tools_bounds @ ISWC+80 sizes the float window; we never set it, so seed a default if empty.
                    if (!docked) {
                        auto* b = reinterpret_cast<std::int32_t*>(static_cast<char*>(pIswc) + 80);
                        if (b[2] <= 0 || b[3] <= 0) {
                            b[0] = 120;
                            b[1] = 80;
                            b[2] = 1100;
                            b[3] = 760;
                        }
                    }
                    self->mIswcSetIsDocked(pView, docked ? 1 : 0, 1);
                    self->logInfo("[dock] SetIsDocked(page={}, docked={})", ch->targetKey, docked);
                }
            }
            ack(nlohmann::json::object());
        } else
            ack(nlohmann::json::object()); // getHostConfig etc. -> {}
    }
    if (self->mDictDtor)
        self->mDictDtor(dictValue);
}

void NativeDevTools::clearHostChannelRfh(void* deadRfh) {
    if (!deadRfh)
        return;
    for (int i = 0; i < mHostChannelN; ++i) {
        auto* ch = static_cast<HostChannel*>(mHostChannels[i]);
        if (ch && ch->frontendRfh == deadRfh)
            ch->frontendRfh = nullptr;
    }
}

void NativeDevTools::onFrontendFrameCommitted(void* rfh, const std::string& url) {
    void* provider = mGetRemoteAssoc ? mGetRemoteAssoc(rfh) : nullptr;
    if (!provider) {
        logWarn("[frontend] null provider — skip Create");
        return;
    }
    if (!mGetAssocProxy || !mSiehCtor || !mCreatePair) {
        logWarn("[frontend] send primitives missing — skip");
        return;
    }
    void* proxy =
            rd(static_cast<char*>(provider) + kProviderProxyOff); // provider[2] = AssociatedInterfaceProvider_Proxy
    if (!proxy) {
        logWarn("[frontend] null provider proxy — skip");
        return;
    }

    void *hFrontendRemote = nullptr, *hFrontendReceiver = nullptr;
    mSiehCtor(&hFrontendRemote);
    mSiehCtor(&hFrontendReceiver);
    mCreatePair(&hFrontendRemote, &hFrontendReceiver);
    std::string ifaceName = kNameDevToolsFrontend;
    mGetAssocProxy(proxy, &ifaceName, &hFrontendReceiver);

    if (!mSessBind || !mNew || !mPassThroughCtor) {
        logWarn("[frontend] remote bind primitives missing");
        return;
    }
    void* frontendEc = nullptr;
    void* vFrontend  = mNew(8);
    mPassThroughCtor(vFrontend);
    mSessBind(&frontendEc,
              &hFrontendRemote,
              0u,
              &vFrontend,
              nullptr,
              reinterpret_cast<const void*>(mNameFrontend),
              reinterpret_cast<void*>(mCbInfo),
              reinterpret_cast<void*>(mCbName));
    if (!frontendEc) {
        logWarn("[frontend] null frontendEc — skip SetupDevToolsFrontend");
        return;
    }

    void *hHostPass = nullptr, *hHostBind = nullptr;
    mSiehCtor(&hHostPass);
    mSiehCtor(&hHostBind);
    mCreatePair(&hHostPass, &hHostBind);

    if (mHostBind && mNew && mPassThroughCtor) {
        void* const slot2 = rd(reinterpret_cast<const void*>(mFhStubVtable + 16));
        const bool  vtok  = slot2 == reinterpret_cast<void*>(mFhAcceptWrapper);
        if (!vtok) {
            logWarn("[frontend] stub vtable identity check failed — skip receiver bind");
        } else {
            const std::string newKey = parseTargetKey(url);
            for (int i = 0; i < mHostChannelN; ++i) {
                auto* old = static_cast<HostChannel*>(mHostChannels[i]);
                if (old && old->frontendRfh && old->targetKey == newKey)
                    old->frontendRfh = nullptr;
            }
            HostChannel* ch     = nullptr;
            bool         reused = false;
            for (int i = 0; i < mHostChannelN; ++i) {
                auto* ex = static_cast<HostChannel*>(mHostChannels[i]);
                if (ex && ex->frontendRfh == nullptr) {
                    ch     = ex;
                    reused = true;
                    break;
                }
            }
            if (!ch) {
                if (mHostChannelN >= static_cast<int>(mHostChannels.size()))
                    logWarn("[frontend] channel array full, no dead slot — skip");
                else {
                    ch                             = new HostChannel{};
                    mHostChannels[mHostChannelN++] = ch;
                }
            }
            if (ch) {
                ch->implVtable  = gHostImplVtable;
                ch->self        = this;
                ch->targetKey   = newKey;
                ch->stubVtable  = reinterpret_cast<const void*>(mFhStubVtable);
                ch->stubSink    = ch; // sink_ = impl = &channel
                ch->frontendRfh = rfh;
                void* vHost     = mNew(8);
                mPassThroughCtor(vHost);
                std::uint64_t span[2] = {0, 0}; // no sync methods
                mHostBind(&ch->hostEc,
                          &hHostBind,
                          &ch->stubVtable,
                          &vHost,
                          span,
                          nullptr,
                          0u,
                          reinterpret_cast<const void*>(mNameFrontendHost),
                          reinterpret_cast<void*>(mCbInfo),
                          reinterpret_cast<void*>(mCbName));
                logInfo("[frontend] DevToolsFrontendHost receiver bound ({}): target=\"{}\" channels={}",
                        reused ? "reused" : "new",
                        ch->targetKey,
                        mHostChannelN);
            }
        }
    } else {
        logWarn("[frontend] receiver bind primitives missing — no embedder messages");
    }

    sendSetupDevToolsFrontend(frontendEc, compatScript(), hHostPass);

    // Attach the native CDP client for clovernt_target.
    if (mCdp) {
        const int pid = parseIntKey(url, "clovernt_target=");
        if (pid >= 0 && mFromId) {
            void* pApi  = mFromId(reinterpret_cast<void*>(mWebContentsTable), pid);
            void* pIswc = pApi ? rd(static_cast<char*>(pApi) + kApiWcIswcOff) : nullptr;
            if (pIswc)
                setupCdpClient(pid, pIswc, rfh);
        }
    }
}

void NativeDevTools::sendSetupDevToolsFrontend(void* frontendEc, const std::string& apiScript, void* hHostPass) {
    if (!mMojoMsgCtor || !mMsgAlloc || !mHdrInit24 || !mSerString || !mSerRemote || !mSiehCopy || !mSendMsg ||
        !mMsgDtor) {
        logWarn("[frontend] SetupDevToolsFrontend send primitives missing — skip");
        return;
    }
    alignas(16) std::uint8_t msg[0x80] = {0};
    mMojoMsgCtor(msg, 0, 0, 0);                           // mojo::Message(ordinal=0, flags=0, size=0)
    const std::uint64_t off     = mMsgAlloc(msg + 8, 24); // allocate 24B payload, returns buffer offset
    char*               bufbase = rd<char*>(msg + 0x18);
    if (!bufbase) {
        logWarn("[frontend] setup: null bufbase");
        mMsgDtor(msg);
        return;
    }
    mHdrInit24(bufbase + off); // struct header {24, 0}

    struct {
        void*        obj;
        std::int64_t pos;
    } st{msg, -1};
    mSerString(const_cast<std::string*>(&apiScript), &st);
    bufbase = rd<char*>(msg + 0x18); // re-read: mSerString may have grown (relocated) the buffer
    if (!bufbase) {
        logWarn("[frontend] setup: re-read bufbase failed");
        mMsgDtor(msg);
        return;
    }
    const std::int64_t dataloc = (st.pos != -1) ? (reinterpret_cast<std::int64_t>(bufbase) + st.pos) : 0;
    auto*              f8      = reinterpret_cast<std::int64_t*>(bufbase + off + 8);
    *f8                        = dataloc ? (dataloc - reinterpret_cast<std::int64_t>(f8)) : 0;

    // payload+16: host pending_associated_remote<DevToolsFrontendHost> {handle, version, pad}.
    struct {
        void*         handle;
        std::uint32_t version;
        std::uint32_t pad;
    } hostStruct{hHostPass, 0, 0};
    void* hostCopy = nullptr;
    mSiehCopy(&hostCopy, &hostStruct);
    bufbase = rd<char*>(msg + 0x18);
    mSerRemote(&hostCopy, hostStruct.version, msg, bufbase + off + 16);

    mSendMsg(frontendEc, msg);
    mMsgDtor(msg);
    logInfo("[frontend] SetupDevToolsFrontend sent (api_script {} bytes)", apiScript.size());
}

void NativeDevTools::applyInspectedPageBounds(int pageId, int x, int y, int w, int h) {
    if (pageId < 0 || !mFromId || !mIswcLayout)
        return;
    void* const pageApi = mFromId(reinterpret_cast<void*>(mWebContentsTable), pageId);
    if (!pageApi)
        return;
    void* pageIswc = rd(static_cast<char*>(pageApi) + kApiWcIswcOff);
    if (!pageIswc)
        return;
    void* pageView = rd(static_cast<char*>(pageIswc) + kIswcViewOff);
    if (!pageView)
        return;
    auto* const b = reinterpret_cast<std::int32_t*>(static_cast<char*>(pageView) + kViewStrategyBoundsOff);
    b[0]          = x;
    b[1]          = y;
    b[2]          = w;
    b[3]          = h;
    *reinterpret_cast<std::uint8_t*>(static_cast<char*>(pageView) + kViewHideContentsOff) = 0u;
    mIswcLayout(pageView);
}

void NativeDevTools::closeDock(int pageId) {
    if (pageId < 0 || !mFromId || !mIswcCloseDevToolsOrch)
        return;
    void* const pageApi = mFromId(reinterpret_cast<void*>(mWebContentsTable), pageId);
    if (!pageApi)
        return;
    void* pageIswc = rd(static_cast<char*>(pageApi) + kApiWcIswcOff);
    if (!pageIswc)
        return;
    teardownCdpClient(pageIswc);
    mIswcCloseDevToolsOrch(pageIswc);
    logInfo("[dock] closeDock page={}", pageId);
}

void NativeDevTools::installReadyToCommitHook() {
    mGetRenderFrameHost = scanFn<GetRenderFrameHostFn>(kGetRenderFrameHost, "GetRenderFrameHost");
    mGetRemoteAssoc     = scanFn<GetRemoteAssocFn>(kGetRemoteAssoc, "GetRemoteAssociatedInterfaces");
    mGetAssocProxy      = scanRelFn<GetAssocProxyFn>(kGetAssocProxyCall, "GetAssociatedInterface");
    mMojoMsgCtor        = scanFn<MojoMsgCtorFn>(kMojoMsgCtor, "mojo::Message::Message");
    mMsgAlloc           = scanFn<MsgAllocFn>(kMsgAlloc, "Message::AllocPayload");
    mHdrInit24          = scanFn<HdrInitFn>(kHdrInit24, "struct header init");
    mSerString          = scanFn<SerStringFn>(kSerString, "Serialize(std::string)");
    mSerRemote          = scanFn<SerRemoteFn>(kSerRemote, "Serialize(assoc remote)");
    mSiehCopy           = scanFn<SiehCopyFn>(kSiehCopy, "SIEH copy");
    mSendMsg            = scanFn<SendMsgFn>(kSendMsg, "endpoint client Send");
    mMsgDtor            = scanFn<MsgDtorFn>(kMsgDtor, "mojo::Message::~Message");
    mDictDtor           = scanFn<DictDtorFn>(kDictDtor, "Dict::~Dict");
    mNameFrontend       = scanAsciiz(kNameDevToolsFrontend, "name DevToolsFrontend");
    mNameFrontendHost   = scanAsciiz(kNameDevToolsFrontendHost, "name DevToolsFrontendHost");
    mFhStubVtable       = scanRel(kFhStubVtableRef, "DevToolsFrontendHost stub vtable");
    mFhAcceptWrapper    = scanText(kFhAcceptWrapper, "stub vtable accept wrapper");

    mFromId                = scanFn<FromIdFn>(kFromId, "api::WebContents::FromID");
    mIswcShowDevTools      = scanFn<IswcShowDevToolsFn>(kIswcShowDevTools, "ShowDevTools");
    mIswcLayout            = scanFn<IswcLayoutFn>(kIswcLayout, "view relayout");
    mIswcCloseDevTools     = scanFn<IswcCloseDevToolsFn>(kIswcCloseDevTools, "CloseDevTools(view)");
    mIswcSetIsDocked       = scanFn<IswcSetIsDockedFn>(kIswcSetIsDocked, "SetIsDocked");
    mIswcCloseDevToolsOrch = scanFn<IswcCloseDevToolsOrchFn>(kIswcCloseDevToolsOrch, "CloseDevTools(orch)");
    mWebContentsTable      = scanRel(kWebContentsTableRef, "api::WebContents table");

    mGetOrCreateFor    = scanFn<GetOrCreateForFn>(kGetOrCreateFor, "GetOrCreateFor");
    mDispatchProtoSend = scanFn<DispatchProtoSendFn>(kDispatchProtoSend, "DispatchProtocolMessage(send)");
    mAttachClientCall  = scanFn<AttachClientFn>(kAttachClient, "AttachClient(call)");
    mCdp               = mGetOrCreateFor && mDispatchProtoSend && mAttachClientCall;

    mGetBrowserContext = scanFn<GetBrowserContextFn>(kGetBrowserContext, "GetBrowserContext");
    mCreateParams      = scanFn<CreateParamsFn>(kCreateParams, "CreateParams");
    mWcCreate          = scanFn<WcCreateFn>(kWcCreate, "WebContents::Create");
    mGurlCtor          = scanFn<GurlCtorFn>(kGurlCtor, "GURL ctor");
    mLoadUrlParams     = scanFn<LoadUrlParamsFn>(kLoadUrlParams, "LoadURLParams ctor");
    mLoadUrlWithParams = scanFn<LoadUrlWithParamsFn>(kLoadUrlWithParams, "LoadURLWithParams");
    mFromOrCreate      = scanFn<FromOrCreateFn>(kFromOrCreate, "FromOrCreate");
    if (const auto mod = Memory::findModule(kModule))
        if (const auto a = mod->findExport(kV8IsolateGetCurrentExport))
            mIsolateGetCurrent = reinterpret_cast<IsolateGetCurrentFn*>(*a);
    mNativeWcPrims = mGetBrowserContext && mCreateParams && mWcCreate && mGurlCtor && mLoadUrlParams &&
                     mLoadUrlWithParams && mFromOrCreate && mIsolateGetCurrent;

    resolveContextEntry();
    mExecJs = scanFn<ExecJsForTestsFn>(kExecJsForTests, "ExecuteJavaScriptForTests");

    loadPrefs(directory() / "clover-devtools-prefs.json"); // DevTools prefs live in this plugin's directory
    logInfo("[frontend] native CDP = {}, frontend wc primitives = {}", mCdp, mNativeWcPrims);

    if (!mGetRenderFrameHost) {
        logWarn("[frontend] GetRenderFrameHost not found — frontend-host rebuild disabled");
        return;
    }
    const auto            owner = Plugin::PluginManager::getPlugin(name());
    const Memory::Address entry = scanText(kReadyToCommit, "ReadyToCommitNavigation");
    if (!entry)
        return;
    auto hook = Memory::Hook::Inline::create<ReadyToCommitFn>(
            entry,
            [this](auto& original, void* navReq, char sameDoc) -> void {
                original(navReq, sameDoc);
                if (!mGetRenderFrameHost)
                    return;
                void* rfh = mGetRenderFrameHost(navReq);
                if (!rfh)
                    return;
                const std::string url = committingUrl(navReq);
                if (url.find("clovernt_target=") != std::string::npos)
                    onFrontendFrameCommitted(rfh, url);
            },
            Memory::Hook::Options{.owner = owner});
    if (!hook) {
        logError("[frontend] hook ReadyToCommitNavigation failed: {}", hook.error().message);
        return;
    }
    mReadyToCommitHook = std::make_unique<decltype(hook)::value_type>(std::move(hook.value()));
    logInfo("[frontend] hooked ReadyToCommitNavigation @ {:#x}", entry);
}

void NativeDevTools::resolveContextEntry() {
    const auto mod = Memory::findModule(kModule);
    if (!mod)
        return;
    auto ex = [&](const char* s) -> void* {
        if (const auto a = mod->findExport(s))
            return reinterpret_cast<void*>(*a);
        logWarn("[context] export not found: {}", s);
        return nullptr;
    };
    mGetCurrentContext = reinterpret_cast<IsolateGetCurrentContextFn*>(ex(kV8IsolateGetCurrentContextExport));
    mHandleScopeCtor   = reinterpret_cast<HandleScopeCtorFn*>(ex(kV8HandleScopeCtorExport));
    mHandleScopeDtor   = reinterpret_cast<HandleScopeDtorFn*>(ex(kV8HandleScopeDtorExport));
    mContextEnter      = reinterpret_cast<ContextEnterFn*>(ex(kV8ContextEnterExport));
    mContextExit       = reinterpret_cast<ContextExitFn*>(ex(kV8ContextExitExport));
    mGetCurrentEnv     = reinterpret_cast<GetCurrentEnvFn*>(ex(kNodeGetCurrentEnvExport));
    mGetMainContext    = reinterpret_cast<GetMainContextFn*>(ex(kNodeGetMainContextExport));
    const bool ready   = mGetCurrentContext && mHandleScopeCtor && mHandleScopeDtor && mContextEnter && mContextExit &&
                         mGetCurrentEnv && mGetMainContext && mIsolateGetCurrent;
    if (!ready) {
        logWarn("[context] context-entry primitives incomplete");
        return;
    }

    if (void* iso = mIsolateGetCurrent()) {
        alignas(16) unsigned char ctxLocal[8] = {};
        mGetCurrentContext(iso, ctxLocal);
        if (void* ctx = *reinterpret_cast<void**>(ctxLocal))
            if (void* env = mGetCurrentEnv(ctx)) {
                mNodeEnv.store(env, std::memory_order_relaxed);
                logInfo("[context] captured node env at load");
            }
    }
    if (mNodeEnv.load(std::memory_order_relaxed))
        return;

    const auto owner = Plugin::PluginManager::getPlugin(name());
    auto       hook  = Memory::Hook::Inline::create<ContextEnterFn>(
            reinterpret_cast<Memory::Address>(mContextEnter),
            [this](auto& original, void* ctx) {
                if (!mNodeEnv.load(std::memory_order_relaxed) && ctx)
                    if (void* env = mGetCurrentEnv(ctx))
                        mNodeEnv.store(env, std::memory_order_relaxed);
                original(ctx);
            },
            Memory::Hook::Options{.owner = owner});
    if (!hook) {
        logError("[context] hook v8::Context::Enter failed: {}", hook.error().message);
        return;
    }
    mCtxEnterHook = std::make_unique<decltype(hook)::value_type>(std::move(hook.value()));
    logInfo("[context] Context::Enter capture hook installed");
}

bool NativeDevTools::enterNodeContext(void* isolate, void* hsBuf, void*& ctxOut) {
    ctxOut    = nullptr;
    void* env = mNodeEnv.load(std::memory_order_relaxed);
    if (!isolate || !env || !mHandleScopeCtor || !mGetMainContext || !mContextEnter || !mHandleScopeDtor)
        return false;
    mHandleScopeCtor(hsBuf, isolate);
    alignas(16) unsigned char ctxLocal[8] = {};
    mGetMainContext(ctxLocal, env);
    void* ctx = *reinterpret_cast<void**>(ctxLocal);
    if (!ctx) {
        mHandleScopeDtor(hsBuf);
        return false;
    }
    mContextEnter(ctx);
    ctxOut = ctx;
    return true;
}

void NativeDevTools::exitNodeContext(void* ctxLocalVal, void* hsBuf) {
    if (ctxLocalVal && mContextExit)
        mContextExit(ctxLocalVal);
    if (mHandleScopeDtor)
        mHandleScopeDtor(hsBuf);
}

void NativeDevTools::installF12Hook() {
    if (!mFromId) {
        logInfo("[f12] skipped (FromId unavailable)");
        return;
    }
    const Memory::Address entry = scanText(kPreHandleKeyboard, "PreHandleKeyboardEvent");
    if (!entry)
        return;
    const auto owner = Plugin::PluginManager::getPlugin(name());
    auto       hook  = Memory::Hook::Inline::create<PreHandleKeyboardFn>(
            entry,
            [this](auto& original, void* apiWc, void* contentWc, void* event) -> std::uint64_t {
                const std::uint64_t r = original(apiWc, contentWc, event);
                if (!event || !apiWc)
                    return r;
                const int type = *reinterpret_cast<const int*>(static_cast<const char*>(event) + kKbEventTypeOff);
                if (type != 7)
                    return r; // kRawKeyDown only
                bool f12 = false;
                for (int off = 0x54; off <= 0x60; off += 4)
                    if (*reinterpret_cast<const int*>(static_cast<const char*>(event) + off) == kVkF12) {
                        f12 = true;
                        break;
                    }
                if (!f12 || !contentWc)
                    return r;
                // rcx is the delegate subobject, not the api wc; map via the source content wc instead.
                void* const table    = reinterpret_cast<void*>(mWebContentsTable);
                int         pageId   = -1;
                void*       pageIswc = nullptr;
                for (int id = 1; id <= 256; ++id) {
                    void* apiwc = mFromId(table, id);
                    if (!apiwc)
                        continue;
                    void* iswc = rd(static_cast<char*>(apiwc) + kApiWcIswcOff);
                    if (!iswc)
                        continue;
                    if (rd(static_cast<char*>(iswc) + kIswcWebContentsOff) == contentWc) {
                        pageId   = id;
                        pageIswc = iswc;
                        break;
                    }
                }
                if (pageId < 0) {
                    logWarn("[f12] contentWc={} matched no page", contentWc);
                    return r;
                }
                void*      managed = rd(static_cast<char*>(pageIswc) + kIswcManagedDevtoolsOff);
                const bool isOpen  = (managed != nullptr) || hasCdpClientForPage(pageId);
                logInfo("[f12] page={} -> {}", pageId, isOpen ? "close" : "open");
                if (isOpen)
                    closeDock(pageId);
                else
                    createAndDockDevTools(pageId);
                return 2; // intercept F12
            },
            Memory::Hook::Options{.owner = owner});
    if (!hook) {
        logError("[f12] hook PreHandleKeyboardEvent failed: {}", hook.error().message);
        return;
    }
    mF12Hook = std::make_unique<decltype(hook)::value_type>(std::move(hook.value()));
    logInfo("[f12] hooked PreHandleKeyboardEvent @ {:#x}", entry);
}

} // namespace ndt
