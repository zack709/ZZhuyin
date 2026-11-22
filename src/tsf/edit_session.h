#pragma once

#include <msctf.h>
#include <atomic>
#include <functional>

namespace zzhuyin::tsf {

extern void DllAddRef();
extern void DllRelease();

// Generic lightweight ITfEditSession executing a lambda inside DoEditSession.
class CallbackEditSession final : public ITfEditSession {
public:
    using Callback = std::function<HRESULT(TfEditCookie)>;

    explicit CallbackEditSession(Callback cb) : cb_(std::move(cb)) {
        DllAddRef();
    }

    ~CallbackEditSession() {
        DllRelease();
    }

    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override {
        if (!ppvObj) return E_INVALIDARG;
        *ppvObj = nullptr;
        if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfEditSession)) {
            *ppvObj = static_cast<ITfEditSession*>(this);
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

    STDMETHODIMP DoEditSession(TfEditCookie ec) override {
        return cb_ ? cb_(ec) : S_OK;
    }

private:
    std::atomic<long> ref_count_{1};
    Callback cb_;
};

} // namespace zzhuyin::tsf
