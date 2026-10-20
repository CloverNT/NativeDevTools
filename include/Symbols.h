#pragma once

#include <cstddef>
#include <cstdint>

#include <CloverNT/API/Memory/Memory.hpp>

namespace ndt::sym {

using CloverNT::Memory::SignatureView;
using namespace CloverNT::Memory::literals;

inline constexpr const char* kModule       = "QQNT.dll";
inline constexpr const char* kTextSection  = ".text";
inline constexpr const char* kRdataSection = ".rdata";

struct RelRef {
    SignatureView sig;
    std::size_t   rel;
};

inline constexpr SignatureView kAttachClient =
        "56 57 53 48 83 EC 20 48 89 D6 48 8B 41 ?? 48 8B 51"_sigv; // DevToolsAgentHostImpl::AttachClient
inline constexpr SignatureView kOperatorNew = "40 53 48 83 EC 20 48 8B D9 EB"_sigv; // PartitionAlloc operator new(size)
inline constexpr SignatureView kSessionCtor =
        "41 56 56 57 53 48 83 EC 38 0F 29 74 24 ?? 44 89 C7"_sigv; // DevToolsSession::DevToolsSession(client, mode)
inline constexpr SignatureView kAttachSession =
        "41 57 41 56 41 54 56 57 53 48 83 EC 58 48 89 D6 48 89 CF 48 8B 05 ?? ?? ?? ?? 48 31 E0 "
        "48 89 44 24 ?? 48 8D 99 ?? ?? ?? ?? 48 89 D9"_sigv; // RFDAH::AttachSession
inline constexpr SignatureView kAttachSessionFull =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC 18 01 00 00 48 89 D7 48 89 CE 48 8B 05 ?? ?? ?? ?? "
        "48 31 E0 48 89 84 24 ?? ?? ?? ?? 48 8B 99"_sigv;
inline constexpr SignatureView kSessionDtor =
        "56 57 48 83 EC 28 89 D7 48 89 CE E8 ?? ?? ?? ?? 40 F6 C7 01 74 ?? BA C0 01 00 00 48 89 F1 "
        "E8 ?? ?? ?? ?? 48 89 F0 48 83 C4 28 5F 5E C3 CC CC CC 41 56"_sigv; // DevToolsSession deleting dtor
inline constexpr SignatureView kOperatorDelete =
        "48 8B 05 ?? ?? ?? ?? 48 8B 40 ?? 4C 8B 0D ?? ?? ?? ?? 45 31 C0 49 FF E1 "
        "CC CC CC CC CC CC CC CC 56 57"_sigv; // sized operator delete(ptr, size)
inline constexpr SignatureView kForceDetachAll =
        "56 57 53 48 83 EC 20 48 89 D6 48 89 CF 48 89 0A"_sigv; // DevToolsAgentHostImpl::ForceDetachAllSessions
inline constexpr SignatureView kDebuggerDetach =
        "56 48 83 EC 20 48 89 CE 48 8B 49 ?? 48 85 C9 74 ?? 48 83 C6 10"_sigv; // electron::api::Debugger::Detach
inline constexpr SignatureView kAddHandler =
        "56 57 48 83 EC 28 48 89 D6 48 89 CF 48 8B 0A 48 8B 97"_sigv; // DevToolsSession::AddHandler
inline constexpr RelRef kStorageCtorCall = {
        "E8 ?? ?? ?? ?? 48 8D 94 24 ?? ?? ?? ?? 4C 89 3A 48 89 F9 E8 ?? ?? ?? ?? 48 8B 4F"_sigv,
        1}; // StorageHandler::ctor
inline constexpr RelRef kSvcWorkerCtorCall = {
        "E8 ?? ?? ?? ?? 48 8D 94 24 ?? ?? ?? ?? 48 89 1A 48 89 F9 E8 ?? ?? ?? ?? 4C 89 E9 "
        "E8 ?? ?? ?? ?? 48 89 C3"_sigv,
        1}; // ServiceWorkerHandler::ctor
inline constexpr RelRef kInspectorCtorCall = {"E8 ?? ?? ?? ?? 4C 89 3B 48 8D 9C 24 ?? ?? ?? ?? 48 89 F9"_sigv,
                                              1}; // InspectorHandler::ctor


inline constexpr SignatureView kUpdateRenderer =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC A8 00 00 00 89 D3 48 89 CE 48 8B 05 ?? ?? ?? ?? "
        "48 31 E0 48 89 84 24 ?? ?? ?? ?? 66 C7 81"_sigv; // RFDAH::UpdateRendererChannel
inline constexpr RelRef kAttachToAgentCall = {
        "4C 89 F1 48 89 F2 41 89 D8 E8 ?? ?? ?? ?? 49 83 C7 08 4D 39 E7 75 ??"_sigv,
        10}; // → DevToolsSession::AttachToAgent
inline constexpr SignatureView kSiehCtor =
        "56 48 83 EC 20 48 89 CE B9 70 00 00 00"_sigv; // ScopedInterfaceEndpointHandle default ctor
inline constexpr SignatureView kCreatePair =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 83 EC 78 0F 29 74 24 ?? 48 89 D7 49 89 CD"_sigv; // SIEH::CreatePairPendingAssociation
inline constexpr SignatureView kHostBind =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC A8 00 00 00 0F 29 B4 24 ?? ?? ?? ?? "
        "4C 89 CE 4C 89 44 24 ?? 48 89 D7"_sigv; // AssociatedReceiver<T>::BindImpl
inline constexpr SignatureView kSessBind =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC A8 00 00 00 0F 29 B4 24 ?? ?? ?? ?? "
        "4C 89 CE 48 89 D7"_sigv; // AssociatedRemote<T>::Bind
inline constexpr SignatureView kIoBind =
        "41 56 56 57 53 48 83 EC 38 4C 89 C7 48 89 D3 48 89 CE 48 8B 05 ?? ?? ?? ?? 48 31 E0 "
        "48 89 44 24 ?? 4C 8D 71 ?? 4C 39 F2"_sigv; // InterfacePtrState<T>::Bind
inline constexpr SignatureView kMojoCreatePipe =
        "48 83 EC 28 83 3D ?? ?? ?? ?? 29 72 15 48 8B 05 ?? ?? ?? ?? 4C 8B 0D ?? ?? ?? ?? "
        "48 83 C4 28 49 FF E1 E8 ?? ?? ?? ?? B8 0C 00 00 00 48 83 C4 28 C3"_sigv; // MojoCreateMessagePipe
inline constexpr SignatureView kPassThroughCtor =
        "48 89 C8 48 8D 0D ?? ?? ?? ?? 48 89 08 C3 CC CC 41 57 41 56 56 57 53 48 83 EC 50 "
        "48 89 D7 48 89 CE"_sigv; // mojo::internal::PassThroughFilter ctor
inline constexpr SignatureView kProxyCtor =
        "48 89 C8 48 8D 0D ?? ?? ?? ?? 48 89 08 48 89 50 ?? C3 CC CC CC CC CC CC CC CC CC CC CC CC CC CC "
        "41 56 56 57 53 48 81 EC 88 00 00 00"_sigv; // DevToolsSession::Proxy ctor
inline constexpr SignatureView kAttachSend =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC E8 00 00 00 4C 89 CB 4C 89 44 24 ?? 48 89 D6 "
        "48 89 4C 24"_sigv; // DevToolsAgent::Proxy::AttachDevToolsSession

inline constexpr SignatureView kReadyToCommit =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC 18 03 00 00 89 D3"_sigv; // NavigationRequest::ReadyToCommitNavigation
inline constexpr SignatureView kGetRenderFrameHost =
        "56 48 83 EC 20 48 89 CE 80 B9 ?? ?? ?? ?? ?? 75 ?? 83 BE"_sigv; // NavigationRequest::GetRenderFrameHost
inline constexpr SignatureView kGetRemoteAssoc =
        "41 56 56 57 53 48 81 EC 98 01 00 00 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 84 24 ?? ?? ?? ?? 48 8B 81"_sigv; // RenderFrameHostImpl::GetRemoteAssociatedInterfaces
inline constexpr RelRef        kGetAssocProxyCall = {"E8 ?? ?? ?? ?? 48 89 D9 E8 ?? ?? ?? ?? E9 ?? ?? ?? ?? 0F 83"_sigv,
                                                     1}; // → AssociatedInterfaceProvider_Proxy::GetAssociatedInterface
inline constexpr SignatureView kMojoMsgCtor =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC C8 00 00 00 4D 89 CF 44 89 44 24 ?? 41 89 D6"_sigv; // mojo::Message::Message
inline constexpr SignatureView kMsgAlloc =
        "41 56 56 57 53 48 83 EC 48 48 89 D6 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 48 83 C6 07"_sigv; // Message::AllocPayload
inline constexpr SignatureView kSerString =
        "56 57 53 48 83 EC 40 48 89 D6 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 0F 57 C0 0F 29 44 24 ?? "
        "48 C7 44 24 ?? ?? ?? ?? ?? F6 01 01"_sigv; // mojo Serialize(std::string)
inline constexpr SignatureView kSerRemote =
        "41 57 41 56 56 57 53 48 83 EC 30 4C 89 CE 4C 89 C7 89 D3 49 89 CE 48 8B 05 ?? ?? ?? ?? 48 31 E0 "
        "48 89 44 24 ?? 4C 8D 7C 24 ?? 4C 89 F9"_sigv; // mojo Serialize(pending_associated_remote)
inline constexpr SignatureView kSendMsg =
        "56 57 55 53 48 83 EC 48 48 89 D7 48 89 CB 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 48 89 D1 "
        "E8 ?? ?? ?? ?? 48 89 C6"_sigv; // InterfaceEndpointClient::Send
inline constexpr SignatureView kHdrInit24 =
        "48 89 C8 48 C7 01 18 00 00 00 C3 CC CC CC CC CC 41 57"_sigv; // write mojo struct header {num_bytes=24,
                                                                      // version=0}
inline constexpr SignatureView kSiehCopy =
        "56 57 48 83 EC 28 48 89 D7 48 89 CE B9 70 00 00 00"_sigv; // copy ScopedInterfaceEndpointHandle
inline constexpr SignatureView kMsgDtor =
        "56 57 53 48 83 EC 20 48 89 CE 48 8B 49 ?? 48 B8 00 00 00 00 FC FF FF FF 48 21 C8 48 3B 05 ?? ?? ?? ?? 0F 84"_sigv; // mojo::Message::~Message
inline constexpr SignatureView kDictDtor =
        "56 57 53 48 83 EC 20 48 8B 19 48 85 DB 75 ?? 48 83 C4 20 5B 5F 5E C3 48 89 CE 48 8B 79 ?? 48 89 D9 "
        "48 39 FB 74 ?? 48 83 C7 E0 48 89 F9 E8 ?? ?? ?? ?? 48 39 DF 75 ?? 48 8B 0E 48 89 5E ?? 48 8B 56 ?? "
        "48 29 CA 48 83 C4 20"_sigv; // base::Value::Dict::~Dict

inline constexpr SignatureView kFromId =
        "56 57 53 48 8B 79 ?? 48 85 FF 74 ?? 48 83 79 ?? ?? 74 ?? 41 89 D0 4C 63 CA 48 8D 5F ?? 48 85 DF 74 ?? "
        "4D 89 CB 4C 39 CF 77 ?? 4C 89 C8 31 D2 48 F7 F7 49 89 D3 EB"_sigv; // api::WebContents table lookup by id
inline constexpr SignatureView kIswcShowDevTools =
        "56 57 53 48 83 EC 20 80 B9 ?? ?? ?? ?? ?? 74 ?? 48 83 C4 20 5B 5F 5E C3 48 89 CE C6 81"_sigv; // InspectableWebContentsView::ShowDevTools
inline constexpr SignatureView kIswcLayout =
        "41 57 41 56 41 54 56 57 55 53 48 83 EC 70 48 89 CE 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 48 C7 44 24"_sigv; // view relayout
inline constexpr SignatureView kIswcCloseDevTools =
        "56 57 48 83 EC 38 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 80 B9 ?? ?? ?? ?? ?? 0F 85 ?? ?? ?? ?? "
        "48 89 CE C6 81 ?? ?? ?? ?? ?? 48 8B 89"_sigv; // InspectableWebContentsView::CloseDevTools
inline constexpr SignatureView kIswcSetIsDocked =
        "41 57 41 56 56 57 53 48 81 EC 30 02 00 00 44 89 C3"_sigv; // InspectableWebContentsView::SetIsDocked
inline constexpr SignatureView kIswcCloseDevToolsOrch =
        "56 48 83 EC 20 48 89 CE 48 83 B9 C0 00 00 00 00 48 8B 81 B8 00 00 00 74 ??"_sigv; // InspectableWebContents::CloseDevTools

inline constexpr SignatureView kBundledStartDataReq =
        "41 57 41 56 41 54 56 57 55 53 48 81 EC 80 01 00 00 4C 89 C6 48 89 D7 48 8B 05"_sigv; // BundledDataSource::StartBundledDataRequest
inline constexpr SignatureView kRefCountedMemCtor =
        "56 57 53 48 83 EC 20 48 89 D7 48 89 CE C7 41 ?? ?? ?? ?? ?? 48 8D 05 ?? ?? ?? ?? 48 89 01 48 8B 1A "
        "48 B8 00 00 00 00 FC FF FF FF 48 21 D8 48 3B 05 ?? ?? ?? ?? 74"_sigv; // RefCountedStaticMemory ctor
inline constexpr SignatureView kCallbackRun =
        "56 57 48 83 EC 38 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 48 89 54 24 ?? 48 8B 11 48 85 D2 74 ?? "
        "48 8D 74 24 ?? 48 89 16 48 C7 01 00 00 00 00 48 8B 42 ?? 48 8D 7C 24 ?? 48 89 D1 48 89 FA FF 15 ?? ?? ?? ?? "
        "48 89 F1 E8 ?? ?? ?? ?? 48 8B 0F 48 85 C9 74 ?? F0 FF 49 ?? 75 ?? 48 8B 01 48 8B 00"_sigv; // GotDataCallback::Run
inline constexpr SignatureView kBrotliDecompress =
        "41 57 41 56 41 54 56 57 53 B8 88 14 00 00"_sigv; // BrotliDecoderDecompress
inline constexpr SignatureView kFromOrCreate =
        "41 56 56 57 53 48 83 EC 68 4D 89 C6 48 89 D7 48 89 CE 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 4D 85 C0"_sigv; // api::WebContents::FromOrCreate
inline constexpr SignatureView kPreHandleKeyboard =
        "41 56 56 57 53 48 81 EC C8 00 00 00 4C 89 C3 48 89 CF"_sigv; // api::WebContents::PreHandleKeyboardEvent

inline constexpr SignatureView kGetOrCreateFor =
        "56 48 83 EC 20 48 89 CE 48 81 C2 80 04 00 00"_sigv; // DevToolsAgentHost::GetOrCreateFor
inline constexpr SignatureView kDispatchProtoSend =
        "48 83 EC 38 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 48 8B 41 ?? 48 8B 49 ?? 49 89 CA"_sigv; // DevToolsAgentHost::DispatchProtocolMessage

inline constexpr SignatureView kGetBrowserContext =
        "48 83 EC 28 48 8B 01 48 8D 15 ?? ?? ?? ?? 48 39 50 ?? 75 ?? 48 81 C1 48 02 00 00 48 83 C4 28"_sigv; // WebContentsImpl::GetBrowserContext
inline constexpr SignatureView kCreateParams =
        "56 57 53 48 83 EC 50 48 89 D7 48 89 CE 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 48 8D 5C 24 ?? "
        "48 89 D9 4C 89 C2 E8 ?? ?? ?? ?? 48 89 F1"_sigv; // WebContents::CreateParams::CreateParams
inline constexpr SignatureView kWcCreate =
        "56 57 53 48 83 EC 30 48 89 D7 48 89 CE 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? 8B 52"_sigv; // WebContents::Create
inline constexpr SignatureView kGurlCtor =
        "41 57 41 56 41 55 41 54 56 57 55 53 48 81 EC 88 00 00 00 0F 29 74 24 ?? 48 89 D7 48 89 CE 48 8B 05 ?? ?? ?? "
        "?? "
        "48 31 E0 48 89 44 24 ?? 0F 57 F6 0F 11 31 48 C7 41 ?? ?? ?? ?? ?? 48 8D 59"_sigv; // GURL::GURL(string_view)
inline constexpr SignatureView kLoadUrlParams =
        "56 57 48 83 EC 38 0F 29 74 24 ?? 48 89 CE E8 ?? ?? ?? ?? 31 FF 40 88 7E ?? 40 88 BE"_sigv; // LoadURLParams::LoadURLParams(GURL)
inline constexpr SignatureView kLoadUrlWithParams =
        "41 56 56 57 53 48 83 EC 68 4C 89 C3 48 89 D6 48 89 CF 48 8B 05 ?? ?? ?? ?? 48 31 E0 48 89 44 24 ?? "
        "48 8D 4C 24 ?? 48 C7 01 00 00 00 00 E8"_sigv; // NavigationControllerImpl::LoadURLWithParams
inline constexpr SignatureView kExecJsForTests =
        "41 57 41 56 56 57 53 48 83 EC 50 44 89 CF 4C 89 C6 48 89 D3 49 89 CE 48 8B 05 ?? ?? ?? ?? 48 31 E0 "
        "48 89 44 24 ?? 49 8B 00 48 8D 54 24 ?? 48 89 02 49 C7 00 00 00 00 00 4C 8D 7C 24 ?? 4C 89 F9 E8 ?? ?? ?? ?? "
        "4C 89 7C 24 ?? 89 7C 24 ?? C6 44 24 ?? ?? 4C 89 F1 48 89 DA 45 31 C0"_sigv; // RenderFrameHostImpl::ExecuteJavaScriptForTests

inline constexpr SignatureView kBrpAcquire =
        "48 89 C8 48 C1 E8 15 25 FF 1F 00 00 48 8D 15 ?? ?? ?? ?? 44 0F B7 14 42 48 89 CA 48 81 E2 00 00 E0 FF "
        "4C 89 D0 48 C1 E0 15 49 89 D0 49 29 C0 0F 94 C0 41 B1 01 49 81 FA FE FF 00 00 0F 85 ?? ?? ?? ?? "
        "48 8B 05 ?? ?? ?? ?? 45 84 C9 0F 84 ?? ?? ?? ?? 48 01 C2 41 89 C8 41 C1 E8 09 41 81 E0 E0 0F 00 00 "
        "4E 8D 0C 02 41 0F B6 54 10 1E 83 E2 3F C1 E2 05 49 29 D1 44 89 CA 4D 8B 41 10 49 29 C1 49 81 E1 00 00 E0 FF "
        "C1 E2 09 81 E2 00 C0 1F 00 4C 09 CA 48 29 D1 41 8B 40 18 49 0F AF 48 20 48 C1 E9 2A 48 0F AF C8 "
        "48 01 D1 49 89 C9 4C 89 C9 48 C1 E1 34"_sigv;

inline constexpr RelRef kWebContentsTableRef = {"48 8D 0D ?? ?? ?? ?? 48 89 F2 41 89 D8"_sigv,
                                                3}; // api::WebContents id→* global table
inline constexpr RelRef kBrpPoolBaseRef      = {
        "89 AF B0 00 00 00 49 BE 00 00 00 00 FC FF FF FF 48 89 D8 4C 21 F0 48 3B 05 ?? ?? ?? ?? 74"_sigv,
        25}; // BackupRefPtr pool-base constant
inline constexpr RelRef kFhStubVtableRef = {
        "48 8D 05 ?? ?? ?? ?? 48 89 46 ?? 48 8D 4E ?? 48 89 FA E8 ?? ?? ?? ?? 4C 89 76 ?? 48 89 F0"_sigv,
        3}; // DevToolsFrontendHost stub vtable
inline constexpr SignatureView kFhAcceptWrapper =
        "48 8B 49 ?? 48 85 C9 0F 85 ?? ?? ?? ?? 31 C0 C3 56 57 48 83 EC 28 48 85 C9 74 ?? 48 89 D6"_sigv; // stub vtable
                                                                                                          // slot 2


inline constexpr SignatureView kAutoAttachWiring  = "4C 8B B7 B0 01 00 00 4C 8D 7C 24 30 48 89 F9 4C 89 FA E8"_sigv;
inline constexpr std::size_t   kAutoAttachDispOff = 3;    // disp32 low byte within the match
inline constexpr std::uint8_t  kAutoAttachBugByte = 0xB0; // gutted
inline constexpr std::uint8_t  kAutoAttachFixByte = 0xC8; // fixed

inline constexpr const char* kNameDevToolsSessionHost  = "blink.mojom.DevToolsSessionHost";
inline constexpr const char* kNameDevToolsSession      = "blink.mojom.DevToolsSession";
inline constexpr const char* kNameDevToolsFrontend     = "blink.mojom.DevToolsFrontend";
inline constexpr const char* kNameDevToolsFrontendHost = "blink.mojom.DevToolsFrontendHost";

inline constexpr std::uintptr_t kCbMethodInfoRva = 0x5EC58B0;
inline constexpr std::uintptr_t kCbMethodNameRva = 0x5442FE0;

inline constexpr std::size_t   kSessionsVecOff = 0x28; // vector<raw_ptr<DevToolsSession>> {begin,end,cap}
inline constexpr std::size_t   kByClientMapOff = 0x40; // flat_map<Client*, unique_ptr<DevToolsSession>> {begin,end,cap}
inline constexpr std::size_t   kSessionAgentHostOff = 0xC8; // session->agent_host_ (raw_ptr)
inline constexpr std::size_t   kRendererChannelOff  = 0x88; // agent_host embedded DevToolsRendererChannel
inline constexpr std::size_t   kAgentProxyOff       = 0xA8; // channel bound DevToolsAgent proxy
inline constexpr std::uint64_t kSessionSize         = 448;
inline constexpr std::uint64_t kBrpMask             = 0xFFFFFFFC00000000ULL;
inline constexpr std::size_t   kSuspendFlagOff      = 0xD0; // session->suspended_sending_messages_to_agent_

inline constexpr std::size_t kRecvEcOff    = 0x50; // receiver_.endpoint_client_
inline constexpr std::size_t kRecvStubOff  = 0x58; // receiver_.stub_
inline constexpr std::size_t kSessEcOff    = 0x78; // session_.endpoint_client_
inline constexpr std::size_t kSessProxyOff = 0x88; // session_.proxy_
inline constexpr std::size_t kIoBaseOff    = 0x90; // io_session_ base
inline constexpr std::size_t kUseIoOff     = 0xC0; // use_io_session_

inline constexpr std::uint64_t kStorageHandlerSize   = 248;
inline constexpr std::uint64_t kSvcWorkerHandlerSize = 112;
inline constexpr std::uint64_t kInspectorHandlerSize = 72;
inline constexpr std::size_t   kSessionUvHandleOff   = 24; // session+24 = embedded uv_handle; loop = *(handle+8)
inline constexpr std::size_t   kUvHandleLoopOff      = 8;  // uv_handle_get_loop(handle) == *(handle+8)

inline constexpr std::size_t kDbgAgentHostOff = 0x48; // agent_host_
inline constexpr std::size_t kDbgClientOff    = 0x10; // embedded DevToolsAgentHostClient

inline constexpr std::size_t kProviderProxyOff = 16; // AssociatedInterfaceProvider[2] = proxy object
inline constexpr std::size_t kDictElemStride =
        32; // base::Value::Dict flat_map element {string@0, unique_ptr<Value>@24}
inline constexpr std::size_t kNavReqUrlOff = 0xA0; // NavigationRequest->common_params_

// F12 keyboard event:
inline constexpr std::size_t kKbEventTypeOff = 40; // NativeWebKeyboardEvent -> WebInputEvent::type_ (7 = kRawKeyDown)
inline constexpr int         kVkF12          = 0x7B;

//   0 dtor
//   1 DispatchProtocolMessage
//   2 AgentHostClosed
//   3 MayAttachToURL
//   4 MayAttachToRenderFrameHost
//   5 IsTrusted
//   6 MayReadLocalFiles
//   7 MayWriteLocalFiles
//   8 AllowUnsafeOperations
//   9 GetNavigationInitiatorOrigin
//   10 UsesBinaryProtocol
//   11 GetTypeForMetrics
inline constexpr std::size_t kCdpClientVtableSlots = 12;

inline constexpr std::size_t kApiWcIswcOff           = 0x690; // inspectable_web_contents_
inline constexpr std::size_t kIswcViewOff            = 0xD0;  // view_
inline constexpr std::size_t kIswcWebContentsOff     = 0xB0;  // inspected content::WebContents*
inline constexpr std::size_t kIswcManagedDevtoolsOff = 0xB8;  // managed_devtools_web_contents_
inline constexpr std::size_t kIswcExtDevtoolsOff     = 0xC0;  // external_devtools_web_contents_
inline constexpr std::size_t kIswcDelegateOff        = 0x28;  // WebContentsDelegate subobject (SetDelegate arg)
inline constexpr std::size_t kWcSetDelegateVtOff     = 0x18;  // content::WebContents vtable SetDelegate slot
inline constexpr std::size_t kViewStrategyBoundsOff  = 0x348; // strategy_.bounds_ (gfx::Rect x/y/w/h)
inline constexpr std::size_t kViewHideContentsOff    = 0x358; // strategy_.hide_inspected_contents_

inline constexpr std::size_t kAgentHostRefCountOff     = 0x8;
inline constexpr std::size_t kAgentHostDeleteDtorVtOff = 0xE0; // deleting virtual dtor vtable slot

inline constexpr std::size_t kNavControllerOff      = 0x248; // WebContents + 0x248 = NavigationController
inline constexpr std::size_t kWcPrimaryMainFrameOff = 0x9E0; // primary_main_frame_

inline constexpr char kV8IsolateGetCurrentExport[] = "?GetCurrent@Isolate@v8@@SAPEAV12@XZ";
inline constexpr char kV8IsolateGetCurrentContextExport[] =
        "?GetCurrentContext@Isolate@v8@@QEAA?AV?$Local@VContext@v8@@@2@XZ";
inline constexpr char kV8HandleScopeCtorExport[] = "??0HandleScope@v8@@QEAA@PEAVIsolate@1@@Z";
inline constexpr char kV8HandleScopeDtorExport[] = "??1HandleScope@v8@@QEAA@XZ";
inline constexpr char kV8ContextEnterExport[]    = "?Enter@Context@v8@@QEAAXXZ";
inline constexpr char kV8ContextExitExport[]     = "?Exit@Context@v8@@QEAAXXZ";
inline constexpr char kNodeGetCurrentEnvExport[] =
        "?GetCurrentEnvironment@node@@YAPEAVEnvironment@1@V?$Local@VContext@v8@@@v8@@@Z";
inline constexpr char kNodeGetMainContextExport[] =
        "?GetMainContext@node@@YA?AV?$Local@VContext@v8@@@v8@@PEAVEnvironment@1@@Z";

using NewFn            = void*(std::uint64_t);
using CtorFn           = void*(void*, void*, std::uint32_t); // DevToolsSession::ctor(session, client, mode)
using AttachSessionFn  = char(void*, void*);                 // (agent_host, session)
using BrpAcquireFn     = void(void*);
using DtorFn           = void(void*, int);            // session deleting dtor(session, delete_flag)
using DeleteFn         = void(void*, std::uint64_t);  // operator delete(ptr, size)
using StorageCtorFn    = void*(void*, void*, void*);  // StorageHandler::ctor(this, agentHost, loop)
using SvcWorkerCtorFn  = void*(void*);                // ServiceWorkerHandler::ctor(this)
using InspectorCtorFn  = void*(void*);                // InspectorHandler::ctor(this)
using AddHandlerFn     = void(void*, void**);         // DevToolsSession::AddHandler(session, &handlerSlot)
using AttachClientFn   = std::uint64_t(void*, void*); // (agent_host, client)
using UpdateRendererFn = std::uint64_t(void*, char);  // RFDAH::UpdateRendererChannel(this, should_connect)
using AttachToAgentFn  = void(void*, void*, char);    // DevToolsSession::AttachToAgent(session, agent, force_io)
using ForceDetachAllFn = void*(void*, void*);         // ForceDetachAllSessions(agent_host, out)
using DebuggerDetachFn = std::uint64_t(void*);        // api::Debugger::Detach(dbg)
using SiehCtorFn       = void(void**);                // ScopedInterfaceEndpointHandle default ctor(*slot)
using CreatePairFn     = void(void**, void**);        // fill two SIEH holders as peers
using HostBindFn       = void(void*, void*, void*, void*, void*, void*, std::uint32_t, const void*, void*, void*);
// (&ec, &sieh, stub, &validator, &span16, taskrunner, version, name, cbInfo, cbName)
using SessBindFn = void(void*, void*, std::uint32_t, void*, void*, const void*, void*, void*);
// (&ec, &sieh, version, &validator, taskrunner, name, cbInfo, cbName)
using IoBindFn          = void(void*, void*, void*);                           // (io_base, &remote_state16, taskrunner)
using MojoCreatePipeFn  = std::int32_t(void*, std::uint64_t*, std::uint64_t*); // ipcz MojoHandle = 8-byte pointer
using PassThroughCtorFn = void(void*);
using ProxyCtorFn       = void(void*, void*); // (self16, endpoint_client)
using AttachSendFn      = void(void*, void*, void*, void*, void*, void*, char, char, void*, char);
// (agent, &hostPending16, &sessPending8, &ioHolder, &statePtr, &scriptStr,
//  usesBinary, isTrusted, &sessIdStr, waitForDebugger)
using GetRemoteAssocFn        = void*(void*);      // GetRemoteAssociatedInterfaces(rfh) -> provider
using ReadyToCommitFn         = void(void*, char); // NavigationRequest::ReadyToCommitNavigation(navReq, same_doc)
using GetRenderFrameHostFn    = void*(void*);      // NavigationRequest::GetRenderFrameHost(navReq)
using GetAssocProxyFn         = void(void*, void*, void*); // GetAssociatedInterface(proxy, &name, &receiver_sieh)
using MojoMsgCtorFn           = void(void*, std::uint32_t, int, std::uint64_t); // (msg, ordinal, flags, payload_size)
using MsgAllocFn              = std::uint64_t(void*, std::uint64_t);            // (msg+8, size)
using HdrInitFn               = void(void*);                                    // (payload) write struct header
using SerStringFn             = void(void*, void*);                             // (std::string*, &state{msg,pos})
using SerRemoteFn             = void(void*, std::uint32_t, void*, void*);       // (sieh_copy, version, msg, out_field)
using SiehCopyFn              = void(void*, void*);                             // (dest, src)
using SendMsgFn               = void(void*, void*);                             // (endpoint_client, msg)
using MsgDtorFn               = void(void*);                                    // (msg)
using DictDtorFn              = void(void*);                                    // base::Value::Dict::~Dict(dict)
using FromIdFn                = void*(void*, int);         // (table, id) -> api::WebContents* (null if absent)
using IswcShowDevToolsFn      = void(void*, char);         // (view, activate)
using IswcLayoutFn            = void(void*);               // (view) relayout
using IswcCloseDevToolsFn     = void(void*);               // (view)
using IswcSetIsDockedFn       = void(void*, char, char);   // (view, docked, activate)
using IswcCloseDevToolsOrchFn = void(void*);               // (iswc) orchestration
using BundledStartDataReqFn   = void(void*, void*, void*); // (self, &path std::string, &GotDataCallback)
using RefCountedMemCtorFn     = void*(void*, const void*); // (obj, &{const void* data, size_t size})
using CallbackRunFn           = void(void*, void*);        // (callback, bytes)
// BrotliDecoderDecompress: (encoded_size, encoded_buffer, in/out decoded_size, decoded_buffer)
using BrotliDecompressFn  = int(std::size_t, const unsigned char*, std::size_t*, unsigned char*);
using FromOrCreateFn      = void(void*, void*, void*);          // (&gin::Handle(sret 16B), isolate, wc)
using IsolateGetCurrentFn = void*();                            // v8::Isolate::GetCurrent
using PreHandleKeyboardFn = std::uint64_t(void*, void*, void*); // (api_wc, content_wc, &event)
using GetOrCreateForFn    = void(void*, void*);                 // (&scoped_refptr out, content::WebContents*)
using DispatchProtoSendFn = void(void*, void*, void*);          // (agent_host, client, &span{data,size})
using GetBrowserContextFn = void*(void*);                       // (wc) -> content::BrowserContext*
using CreateParamsFn      = void(void*, void*, void*);          // (cp, ctx, &base::Location)
using WcCreateFn          = void(void*, void*);                 // (&out unique_ptr<WC>, &CreateParams)
using GurlCtorFn          = void(void*, void*);                 // (gurl, &string_view{data,len})
using LoadUrlParamsFn     = void(void*, void*);                 // (lp, &GURL)
using LoadUrlWithParamsFn = void(void*, void*, void*);          // (navctrl, &out WeakPtr sret, &LoadURLParams)
using ExecJsForTestsFn    = void(void*, void*, void*, int);     // (rfh, &u16js, &OnceCallback, world_id)

// context entry (node main context) — all resolved via Module::findExport:
using IsolateGetCurrentContextFn = void(void*, void*); // (isolate, &out Local<Context> sret)
using HandleScopeCtorFn          = void(void*, void*); // (hsBuf>=24B, isolate)
using HandleScopeDtorFn          = void(void*);
using ContextEnterFn             = void(void*); // this = context Local value
using ContextExitFn              = void(void*);
using GetCurrentEnvFn            = void*(void*);       // node::GetCurrentEnvironment(Local<Context>) -> Environment*
using GetMainContextFn           = void(void*, void*); // (&out Local<Context> sret, Environment*)

} // namespace ndt::sym
