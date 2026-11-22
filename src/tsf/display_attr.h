#pragma once

#include <msctf.h>
#include <atomic>

namespace zzhuyin::tsf {

class DisplayAttributeInfo final : public ITfDisplayAttributeInfo {
public:
    DisplayAttributeInfo();
    ~DisplayAttributeInfo();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfDisplayAttributeInfo
    STDMETHODIMP GetGUID(GUID* pguid) override;
    STDMETHODIMP GetDescription(BSTR* pbstrDesc) override;
    STDMETHODIMP GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) override;
    STDMETHODIMP SetAttributeInfo(const TF_DISPLAYATTRIBUTE* pda) override;
    STDMETHODIMP Reset() override;

private:
    std::atomic<long> ref_count_{1};
    TF_DISPLAYATTRIBUTE attr_{};
};

class EnumDisplayAttributeInfo final : public IEnumTfDisplayAttributeInfo {
public:
    EnumDisplayAttributeInfo();
    ~EnumDisplayAttributeInfo();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // IEnumTfDisplayAttributeInfo
    STDMETHODIMP Clone(IEnumTfDisplayAttributeInfo** ppEnum) override;
    STDMETHODIMP Next(ULONG ulCount, ITfDisplayAttributeInfo** rgInfo, ULONG* pcFetched) override;
    STDMETHODIMP Reset() override;
    STDMETHODIMP Skip(ULONG ulCount) override;

private:
    std::atomic<long> ref_count_{1};
    ULONG index_ = 0;
};

} // namespace zzhuyin::tsf
