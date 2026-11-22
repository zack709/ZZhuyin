#include "tsf/display_attr.h"

#include "tsf/guids.h"

#include <oleauto.h>

namespace zzhuyin::tsf {

extern void DllAddRef();
extern void DllRelease();

DisplayAttributeInfo::DisplayAttributeInfo() {
    DllAddRef();
    Reset();
}

DisplayAttributeInfo::~DisplayAttributeInfo() {
    DllRelease();
}

STDMETHODIMP DisplayAttributeInfo::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_ITfDisplayAttributeInfo)) {
        *ppvObj = static_cast<ITfDisplayAttributeInfo*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) DisplayAttributeInfo::AddRef() {
    return static_cast<ULONG>(++ref_count_);
}

STDMETHODIMP_(ULONG) DisplayAttributeInfo::Release() {
    const long count = --ref_count_;
    if (count == 0) {
        delete this;
    }
    return static_cast<ULONG>(count);
}

STDMETHODIMP DisplayAttributeInfo::GetGUID(GUID* pguid) {
    if (!pguid) return E_INVALIDARG;
    *pguid = kGuidDisplayAttributeInput;
    return S_OK;
}

STDMETHODIMP DisplayAttributeInfo::GetDescription(BSTR* pbstrDesc) {
    if (!pbstrDesc) return E_INVALIDARG;
    *pbstrDesc = SysAllocString(L"ZZhuyin Composition");
    return *pbstrDesc ? S_OK : E_OUTOFMEMORY;
}

STDMETHODIMP DisplayAttributeInfo::GetAttributeInfo(TF_DISPLAYATTRIBUTE* pda) {
    if (!pda) return E_INVALIDARG;
    *pda = attr_;
    return S_OK;
}

STDMETHODIMP DisplayAttributeInfo::SetAttributeInfo(const TF_DISPLAYATTRIBUTE* pda) {
    if (!pda) return E_INVALIDARG;
    attr_ = *pda;
    return S_OK;
}

STDMETHODIMP DisplayAttributeInfo::Reset() {
    attr_.crText.type = TF_CT_NONE;
    attr_.crBk.type   = TF_CT_NONE;
    attr_.lsStyle     = TF_LS_DOT;
    attr_.fBoldLine   = FALSE;
    attr_.crLine.type = TF_CT_NONE;
    attr_.bAttr       = TF_ATTR_INPUT;
    return S_OK;
}

EnumDisplayAttributeInfo::EnumDisplayAttributeInfo() {
    DllAddRef();
}

EnumDisplayAttributeInfo::~EnumDisplayAttributeInfo() {
    DllRelease();
}

STDMETHODIMP EnumDisplayAttributeInfo::QueryInterface(REFIID riid, void** ppvObj) {
    if (!ppvObj) return E_INVALIDARG;
    *ppvObj = nullptr;
    if (IsEqualIID(riid, IID_IUnknown) || IsEqualIID(riid, IID_IEnumTfDisplayAttributeInfo)) {
        *ppvObj = static_cast<IEnumTfDisplayAttributeInfo*>(this);
        AddRef();
        return S_OK;
    }
    return E_NOINTERFACE;
}

STDMETHODIMP_(ULONG) EnumDisplayAttributeInfo::AddRef() {
    return static_cast<ULONG>(++ref_count_);
}

STDMETHODIMP_(ULONG) EnumDisplayAttributeInfo::Release() {
    const long count = --ref_count_;
    if (count == 0) {
        delete this;
    }
    return static_cast<ULONG>(count);
}

STDMETHODIMP EnumDisplayAttributeInfo::Clone(IEnumTfDisplayAttributeInfo** ppEnum) {
    if (!ppEnum) return E_INVALIDARG;
    auto* clone = new (std::nothrow) EnumDisplayAttributeInfo();
    if (!clone) return E_OUTOFMEMORY;
    clone->index_ = index_;
    *ppEnum = clone;
    return S_OK;
}

STDMETHODIMP EnumDisplayAttributeInfo::Next(
    ULONG ulCount, ITfDisplayAttributeInfo** rgInfo, ULONG* pcFetched) {
    if (!rgInfo) return E_INVALIDARG;
    ULONG fetched = 0;
    while (fetched < ulCount && index_ < 1) {
        auto* info = new (std::nothrow) DisplayAttributeInfo();
        if (!info) return E_OUTOFMEMORY;
        rgInfo[fetched++] = info;
        ++index_;
    }
    if (pcFetched) *pcFetched = fetched;
    return (fetched == ulCount) ? S_OK : S_FALSE;
}

STDMETHODIMP EnumDisplayAttributeInfo::Reset() {
    index_ = 0;
    return S_OK;
}

STDMETHODIMP EnumDisplayAttributeInfo::Skip(ULONG ulCount) {
    if (index_ + ulCount > 1) {
        index_ = 1;
        return S_FALSE;
    }
    index_ += ulCount;
    return S_OK;
}

} // namespace zzhuyin::tsf
