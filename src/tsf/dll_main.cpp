#include "tsf/guids.h"
#include "tsf/text_service.h"

#include <msctf.h>
#include <olectl.h>
#include <atomic>
#include <string>

namespace zzhuyin::tsf {

HINSTANCE g_hInst = nullptr;
std::atomic<long> g_dllRefCount{0};

void DllAddRef() {
    ++g_dllRefCount;
}

void DllRelease() {
    --g_dllRefCount;
}

namespace {

class ClassFactory final : public IClassFactory {
public:
    ClassFactory() { DllAddRef(); }
    ~ClassFactory() { DllRelease(); }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IClassFactory)) {
            *ppvObj = static_cast<IClassFactory*>(this);
            AddRef();
            return S_OK;
        }
        return E_NOINTERFACE;
    }

    STDMETHODIMP_(ULONG) AddRef() override {
        return static_cast<ULONG>(++ref_count_);
    }

    STDMETHODIMP_(ULONG) Release() override {
        const long count = --ref_count_;
        if (count == 0) {
            delete this;
        }
        return static_cast<ULONG>(count);
    }

    STDMETHODIMP CreateInstance(IUnknown* pUnkOuter, REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;
        if (pUnkOuter != nullptr) return CLASS_E_NOAGGREGATION;

        auto* service = new (std::nothrow) TextService();
        if (!service) return E_OUTOFMEMORY;

        const HRESULT hr = service->QueryInterface(riid, ppvObj);
        service->Release();
        return hr;
    }

    STDMETHODIMP LockServer(BOOL fLock) override {
        if (fLock) {
            DllAddRef();
        } else {
            DllRelease();
        }
        return S_OK;
    }

private:
    std::atomic<long> ref_count_{1};
};

std::wstring GuidToString(REFGUID guid) {
    wchar_t buf[64] = {};
    StringFromGUID2(guid, buf, 64);
    return buf;
}

const GUID kSupportCategories[] = {
    GUID_TFCAT_TIP_KEYBOARD,
    GUID_TFCAT_DISPLAYATTRIBUTEPROVIDER,
    GUID_TFCAT_TIPCAP_SECUREMODE,
    GUID_TFCAT_TIPCAP_UIELEMENTENABLED,
    GUID_TFCAT_TIPCAP_INPUTMODECOMPARTMENT,
    GUID_TFCAT_TIPCAP_IMMERSIVESUPPORT,
    GUID_TFCAT_TIPCAP_SYSTRAYSUPPORT,
};

HRESULT RegisterComServer() {
    wchar_t dll_path[MAX_PATH] = {};
    if (GetModuleFileNameW(g_hInst, dll_path, MAX_PATH) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    const std::wstring clsid_key = L"CLSID\\" + GuidToString(kClsidTextService);
    const std::wstring inproc_key = clsid_key + L"\\InprocServer32";

    HKEY hKey = nullptr;
    LSTATUS st = RegCreateKeyExW(
        HKEY_CLASSES_ROOT, clsid_key.c_str(), 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (st != ERROR_SUCCESS) return HRESULT_FROM_WIN32(st);

    RegSetValueExW(
        hKey, nullptr, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(kClsidDescription),
        static_cast<DWORD>((wcslen(kClsidDescription) + 1) * sizeof(wchar_t)));
    RegCloseKey(hKey);

    st = RegCreateKeyExW(
        HKEY_CLASSES_ROOT, inproc_key.c_str(), 0, nullptr,
        REG_OPTION_NON_VOLATILE, KEY_WRITE, nullptr, &hKey, nullptr);
    if (st != ERROR_SUCCESS) return HRESULT_FROM_WIN32(st);

    RegSetValueExW(
        hKey, nullptr, 0, REG_SZ,
        reinterpret_cast<const BYTE*>(dll_path),
        static_cast<DWORD>((wcslen(dll_path) + 1) * sizeof(wchar_t)));

    constexpr wchar_t kThreadingModel[] = L"Apartment";
    RegSetValueExW(
        hKey, L"ThreadingModel", 0, REG_SZ,
        reinterpret_cast<const BYTE*>(kThreadingModel),
        static_cast<DWORD>(sizeof(kThreadingModel)));
    RegCloseKey(hKey);

    return S_OK;
}

void UnregisterComServer() {
    const std::wstring clsid_key = L"CLSID\\" + GuidToString(kClsidTextService);
    RegDeleteTreeW(HKEY_CLASSES_ROOT, clsid_key.c_str());
}

HRESULT RegisterProfilesAndCategories() {
    wchar_t dll_path[MAX_PATH] = {};
    if (GetModuleFileNameW(g_hInst, dll_path, MAX_PATH) == 0) {
        return HRESULT_FROM_WIN32(GetLastError());
    }

    ITfInputProcessorProfileMgr* profile_mgr = nullptr;
    HRESULT hr = CoCreateInstance(
        CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profile_mgr));
    if (FAILED(hr)) return hr;

    hr = profile_mgr->RegisterProfile(
        kClsidTextService,
        kLangIdZhTw,
        kGuidProfile,
        kProfileDescription,
        static_cast<ULONG>(wcslen(kProfileDescription)),
        dll_path,
        static_cast<ULONG>(wcslen(dll_path)),
        0,
        nullptr,
        0,
        TRUE,
        0);
    profile_mgr->Release();
    if (FAILED(hr)) return hr;

    ITfCategoryMgr* cat_mgr = nullptr;
    hr = CoCreateInstance(
        CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
        IID_ITfCategoryMgr, reinterpret_cast<void**>(&cat_mgr));
    if (FAILED(hr)) return hr;

    for (const auto& cat : kSupportCategories) {
        hr = cat_mgr->RegisterCategory(kClsidTextService, cat, kClsidTextService);
        if (FAILED(hr)) break;
    }
    cat_mgr->Release();
    return hr;
}

void UnregisterProfilesAndCategories() {
    ITfCategoryMgr* cat_mgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(
            CLSID_TF_CategoryMgr, nullptr, CLSCTX_INPROC_SERVER,
            IID_ITfCategoryMgr, reinterpret_cast<void**>(&cat_mgr)))) {
        for (const auto& cat : kSupportCategories) {
            cat_mgr->UnregisterCategory(kClsidTextService, cat, kClsidTextService);
        }
        cat_mgr->Release();
    }

    ITfInputProcessorProfileMgr* profile_mgr = nullptr;
    if (SUCCEEDED(CoCreateInstance(
            CLSID_TF_InputProcessorProfiles, nullptr, CLSCTX_INPROC_SERVER,
            IID_ITfInputProcessorProfileMgr, reinterpret_cast<void**>(&profile_mgr)))) {
        profile_mgr->UnregisterProfile(kClsidTextService, kLangIdZhTw, kGuidProfile, 0);
        profile_mgr->Release();
    }
}

} // namespace
} // namespace zzhuyin::tsf

BOOL WINAPI DllMain(HINSTANCE hInstance, DWORD dwReason, LPVOID /*lpReserved*/) {
    if (dwReason == DLL_PROCESS_ATTACH) {
        zzhuyin::tsf::g_hInst = hInstance;
        DisableThreadLibraryCalls(hInstance);
    }
    return TRUE;
}

STDAPI DllCanUnloadNow() {
    return (zzhuyin::tsf::g_dllRefCount.load() == 0) ? S_OK : S_FALSE;
}

STDAPI DllGetClassObject(REFCLSID rclsid, REFIID riid, void** ppv) {
    if (!ppv) return E_INVALIDARG;
    *ppv = nullptr;
    if (!IsEqualCLSID(rclsid, zzhuyin::tsf::kClsidTextService)) {
        return CLASS_E_CLASSNOTAVAILABLE;
    }
    auto* factory = new (std::nothrow) zzhuyin::tsf::ClassFactory();
    if (!factory) return E_OUTOFMEMORY;
    const HRESULT hr = factory->QueryInterface(riid, ppv);
    factory->Release();
    return hr;
}

STDAPI DllRegisterServer() {
    HRESULT hr = zzhuyin::tsf::RegisterComServer();
    if (FAILED(hr)) return hr;

    const HRESULT co_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    hr = zzhuyin::tsf::RegisterProfilesAndCategories();
    if (SUCCEEDED(co_hr)) CoUninitialize();
    return hr;
}

STDAPI DllUnregisterServer() {
    const HRESULT co_hr = CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED);
    zzhuyin::tsf::UnregisterProfilesAndCategories();
    if (SUCCEEDED(co_hr)) CoUninitialize();
    zzhuyin::tsf::UnregisterComServer();
    return S_OK;
}
