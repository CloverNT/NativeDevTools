
#include "NativeDevTools.h"

#include <cstdlib>
#include <fstream>
#include <iterator>
#include <map>
#include <string>

#include <CloverNT/API/Plugin/PluginManager.hpp>

namespace ndt {

using namespace sym;

bool NativeDevTools::loadBundledPak() {
    const auto pakPath = directory() / "devtools_resources.pak";
    const auto mapPath = directory() / "devtools_resources.map";

    std::ifstream pf(pakPath, std::ios::binary);
    if (!pf) {
        logWarn("[dock] devtools_resources.pak missing: {}", pakPath.string());
        return false;
    }
    mPakData.assign(std::istreambuf_iterator<char>(pf), std::istreambuf_iterator<char>());
    const std::size_t n    = mPakData.size();
    const auto* const p    = reinterpret_cast<const unsigned char*>(mPakData.data());
    const auto        rd16 = [&](std::size_t o) -> std::uint16_t {
        return static_cast<std::uint16_t>(p[o] | (p[o + 1] << 8));
    };
    const auto rd32 = [&](std::size_t o) -> std::uint32_t {
        return std::uint32_t(p[o]) | (std::uint32_t(p[o + 1]) << 8) | (std::uint32_t(p[o + 2]) << 16) |
               (std::uint32_t(p[o + 3]) << 24);
    };
    if (n < 12 || rd32(0) != 5) {
        logWarn("[dock] devtools_resources.pak: not a v5 pak ({} bytes)", n);
        return false;
    }
    const std::uint16_t   rc     = rd16(8);  // resource_count
    const std::uint16_t   ac     = rd16(10); // alias_count
    constexpr std::size_t kIndex = 12;
    if (kIndex + (std::size_t(rc) + 1) * 6 + std::size_t(ac) * 4 > n) {
        logWarn("[dock] devtools_resources.pak: index truncated");
        return false;
    }
    // (id u16, offset u32) x (rc+1); data[id_i] spans [offset_i, offset_{i+1}).
    for (std::uint16_t i = 0; i < rc; ++i) {
        const std::size_t   eo = kIndex + std::size_t(i) * 6;
        const std::uint32_t s  = rd32(eo + 2);
        const std::uint32_t e  = rd32(eo + 8); // next entry's offset
        if (s > e || e > n) {
            logWarn("[dock] devtools_resources.pak: entry {} out of range", rd16(eo));
            return false;
        }
        mPakIndex.emplace(rd16(eo), std::pair{s, e});
    }

    const std::size_t aliasOff = kIndex + (std::size_t(rc) + 1) * 6;
    for (std::uint16_t i = 0; i < ac; ++i) {
        const std::size_t   ao  = aliasOff + std::size_t(i) * 4;
        const std::uint16_t idx = rd16(ao + 2);
        if (idx < rc) {
            const std::size_t eo = kIndex + std::size_t(idx) * 6;
            mPakIndex.emplace(rd16(ao), std::pair{rd32(eo + 2), rd32(eo + 8)});
        }
    }

    std::ifstream mf(mapPath);
    if (!mf) {
        logWarn("[dock] devtools_resources.map missing: {}", mapPath.string());
        return false;
    }
    std::string line;
    while (std::getline(mf, line)) {
        if (!line.empty() && line.back() == '\r')
            line.pop_back();
        const auto sp = line.find(' ');
        if (sp == std::string::npos)
            continue;
        const unsigned long id = std::strtoul(line.c_str(), nullptr, 10);
        if (id && sp + 1 < line.size())
            mPathToId.emplace(line.substr(sp + 1), static_cast<std::uint16_t>(id));
    }
    logInfo("[dock] bundled DevTools pak: {} resources, {} paths ({} bytes)", mPakIndex.size(), mPathToId.size(), n);
    return !mPakIndex.empty() && !mPathToId.empty();
}

const std::string* NativeDevTools::decodeResource(const std::string& rel) {
    static std::map<std::string, std::string> cache;
    if (auto it = cache.find(rel); it != cache.end())
        return it->second.empty() ? nullptr : &it->second;
    const auto fail = [&]() -> const std::string* {
        cache.emplace(rel, std::string{});
        return nullptr;
    };

    const auto pit = mPathToId.find(rel);
    if (pit == mPathToId.end()) {
        logWarn("[dock] devtools:// path not bundled: {}", rel);
        return fail();
    }
    const auto xit = mPakIndex.find(pit->second);
    if (xit == mPakIndex.end())
        return fail();
    const auto* const blob = reinterpret_cast<const unsigned char*>(mPakData.data()) + xit->second.first;
    const std::size_t len  = xit->second.second - xit->second.first;

    std::string out;
    if (len >= 8 && blob[0] == 0x1e && blob[1] == 0x9b) { // magic
        std::uint64_t dec = 0;
        for (int i = 0; i < 6; ++i)
            dec |= std::uint64_t(blob[2 + i]) << (8 * i);
        out.resize(static_cast<std::size_t>(dec));
        std::size_t cap = out.size();
        const int   ok  = mBrotli(len - 8, blob + 8, &cap, reinterpret_cast<unsigned char*>(out.data()));
        if (ok != 1 || cap != out.size()) {
            logWarn("[dock] brotli decode failed: {} (ok={} got={} want={})", rel, ok, cap, out.size());
            return fail();
        }
    } else {
        out.assign(reinterpret_cast<const char*>(blob), len); // raw (uncompressed) entry
    }
    auto [it, _] = cache.emplace(rel, std::move(out));
    static int n = 0;
    if (++n <= 6 || n % 100 == 0)
        logInfo("[dock] devtools:// decoded #{} {} ({}B)", n, rel, it->second.size());
    return it->second.empty() ? nullptr : &it->second;
}

void NativeDevTools::installBundledDataSourceHook() {
    const Memory::Address entry = scanText(kBundledStartDataReq, "BundledDataSource::StartBundledDataRequest");
    if (!entry) {
        logWarn("[dock] StartBundledDataRequest not found — devtools:// resource supply disabled");
        return;
    }
    mRefCountedMemCtor = scanFn<RefCountedMemCtorFn>(kRefCountedMemCtor, "RefCountedStaticMemory ctor");
    mCallbackRun       = scanFn<CallbackRunFn>(kCallbackRun, "GotDataCallback::Run");
    mBrotli            = scanFn<BrotliDecompressFn>(kBrotliDecompress, "BrotliDecoderDecompress");
    if (!mRefCountedMemCtor || !mCallbackRun || !mBrotli || !mNew) {
        logWarn("[dock] supply primitives not found — devtools:// resource supply disabled");
        return;
    }
    if (!loadBundledPak()) {
        logWarn("[dock] bundled DevTools pak unavailable — devtools:// resource supply disabled");
        return;
    }
    const auto owner = Plugin::PluginManager::getPlugin(name());
    auto       hook  = Memory::Hook::Inline::create<BundledStartDataReqFn>(
            entry,
            [this](auto& original, void* self, void* pathStr, void* callback) -> void {
                std::string rel = readLibcxxString(pathStr);
                if (!rel.empty())
                    if (auto q = rel.find_first_of("?#"); q != std::string::npos)
                        rel.resize(q);
                const std::string* content = rel.empty() ? nullptr : decodeResource(rel);
                if (!content) {
                    original(self, pathStr, callback);
                    return;
                }
                void* obj = mNew(32);
                if (!obj) {
                    original(self, pathStr, callback);
                    return;
                }
                struct {
                    const void* data;
                    std::size_t size;
                } ds{content->data(), content->size()};
                mRefCountedMemCtor(obj, &ds);                                       // stores {data,size}
                *reinterpret_cast<volatile long*>(static_cast<char*>(obj) + 8) = 1; // refcount = 1
                mCallbackRun(callback, obj);                                        // consumes callback, releases bytes
            },
            Memory::Hook::Options{.owner = owner});
    if (!hook) {
        logError("[dock] hook StartBundledDataRequest failed: {}", hook.error().message);
        return;
    }
    mBundledHook = std::make_unique<decltype(hook)::value_type>(std::move(hook.value()));
    logInfo("[dock] hooked BundledDataSource::StartBundledDataRequest @ {:#x} ({} bundled resources)",
            entry,
            mPathToId.size());
}

} // namespace ndt
