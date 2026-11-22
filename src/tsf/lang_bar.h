#pragma once

#include "tsf/config_reg.h"

#include <ctfutb.h>
#include <msctf.h>
#include <atomic>
#include <functional>

namespace zzhuyin::tsf {

struct LangBarCallbacks {
    std::function<bool()> is_native;
    std::function<bool()> is_full_shape;
    std::function<PersistedConfig()> get_config;
    std::function<void()> toggle_native;
    std::function<void()> toggle_full_shape;
    std::function<void(const PersistedConfig&)> update_config;
};

class LangBarButton final
    : public ITfLangBarItemButton
    , public ITfSource {
public:
    explicit LangBarButton(LangBarCallbacks callbacks);
    ~LangBarButton();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfLangBarItem
    STDMETHODIMP GetInfo(TF_LANGBARITEMINFO* pInfo) override;
    STDMETHODIMP GetStatus(DWORD* pdwStatus) override;
    STDMETHODIMP Show(BOOL fShow) override;
    STDMETHODIMP GetTooltipString(BSTR* pbstrToolTip) override;

    // ITfLangBarItemButton
    STDMETHODIMP OnClick(TfLBIClick click, POINT pt, const RECT* prcArea) override;
    STDMETHODIMP InitMenu(ITfMenu* pMenu) override;
    STDMETHODIMP OnMenuSelect(UINT wID) override;
    STDMETHODIMP GetIcon(HICON* phIcon) override;
    STDMETHODIMP GetText(BSTR* pbstrText) override;

    // ITfSource
    STDMETHODIMP AdviseSink(REFIID riid, IUnknown* punk, DWORD* pdwCookie) override;
    STDMETHODIMP UnadviseSink(DWORD dwCookie) override;

    void NotifyUpdate();

private:
    static HICON RenderModeIcon(bool is_native, bool is_full_shape);

    std::atomic<long> ref_count_{1};
    LangBarCallbacks cb_;
    ITfLangBarItemSink* sink_ = nullptr;
    DWORD sink_cookie_ = 0;
};

} // namespace zzhuyin::tsf
