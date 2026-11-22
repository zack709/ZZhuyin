#pragma once

#include "core/dictionary.h"
#include "core/engine.h"
#include "tsf/candidate_wnd.h"
#include "tsf/config_reg.h"
#include "tsf/lang_bar.h"

#include <ctffunc.h>
#include <msctf.h>
#include <atomic>
#include <memory>

namespace zzhuyin::tsf {

class TextService final
    : public ITfTextInputProcessorEx
    , public ITfThreadMgrEventSink
    , public ITfKeyEventSink
    , public ITfCompositionSink
    , public ITfDisplayAttributeProvider
    , public ITfCompartmentEventSink
    , public ITfFnConfigure {
public:
    TextService();
    ~TextService();

    // IUnknown
    STDMETHODIMP QueryInterface(REFIID riid, void** ppvObj) override;
    STDMETHODIMP_(ULONG) AddRef() override;
    STDMETHODIMP_(ULONG) Release() override;

    // ITfTextInputProcessor
    STDMETHODIMP Activate(ITfThreadMgr* ptim, TfClientId tid) override;
    STDMETHODIMP Deactivate() override;

    // ITfTextInputProcessorEx
    STDMETHODIMP ActivateEx(ITfThreadMgr* ptim, TfClientId tid, DWORD dwFlags) override;

    // ITfThreadMgrEventSink
    STDMETHODIMP OnInitDocumentMgr(ITfDocumentMgr* pdim) override;
    STDMETHODIMP OnUninitDocumentMgr(ITfDocumentMgr* pdim) override;
    STDMETHODIMP OnSetFocus(ITfDocumentMgr* pdimFocus, ITfDocumentMgr* pdimPrevFocus) override;
    STDMETHODIMP OnPushContext(ITfContext* pic) override;
    STDMETHODIMP OnPopContext(ITfContext* pic) override;

    // ITfKeyEventSink
    STDMETHODIMP OnSetFocus(BOOL fForeground) override;
    STDMETHODIMP OnTestKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyDown(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnTestKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnKeyUp(ITfContext* pic, WPARAM wParam, LPARAM lParam, BOOL* pfEaten) override;
    STDMETHODIMP OnPreservedKey(ITfContext* pic, REFGUID rguid, BOOL* pfEaten) override;

    // ITfCompositionSink
    STDMETHODIMP OnCompositionTerminated(TfEditCookie ecWrite, ITfComposition* pComposition) override;

    // ITfDisplayAttributeProvider
    STDMETHODIMP EnumDisplayAttributeInfo(IEnumTfDisplayAttributeInfo** ppEnum) override;
    STDMETHODIMP GetDisplayAttributeInfo(REFGUID guid, ITfDisplayAttributeInfo** ppInfo) override;

    // ITfCompartmentEventSink
    STDMETHODIMP OnChange(REFGUID rguid) override;

    // ITfFunction / ITfFnConfigure (Windows Settings -> 鍵盤選項)
    STDMETHODIMP GetDisplayName(BSTR* pbstrName) override;
    STDMETHODIMP Show(HWND hwndParent, LANGID langid, REFGUID rguidProfile) override;

private:
    void OpenSettingsDialog();
    bool InitDictionary();
    bool InitDisplayAttributeAtom();
    bool InitThreadMgrEventSink();
    void UninitThreadMgrEventSink();
    bool InitKeyEventSink();
    void UninitKeyEventSink();
    void InitOpenCloseCompartment();
    void UninitOpenCloseCompartment();
    void SyncOpenCloseCompartment();
    void InitLangBar();
    void UninitLangBar();
    void ApplyPersistedConfig(const PersistedConfig& cfg);

    zzhuyin::KeyEvent TranslateKeyEvent(WPARAM wParam, LPARAM lParam, bool is_key_up) const;
    void ApplyOutput(ITfContext* context, const zzhuyin::EngineOutput& out);
    HRESULT ApplyOutputInEditSession(
        TfEditCookie ec, ITfContext* context, const zzhuyin::EngineOutput& out);
    void SetCompositionDisplayAttribute(TfEditCookie ec, ITfContext* context, ITfRange* range);
    void ClearCompositionDisplayAttribute(TfEditCookie ec, ITfContext* context, ITfRange* range);
    void TerminateComposition(ITfContext* context);
    void OnCandidateMouseSelect(size_t page_offset);

    std::atomic<long> ref_count_{1};

    ITfThreadMgr* thread_mgr_ = nullptr;
    TfClientId client_id_ = TF_CLIENTID_NULL;
    DWORD thread_mgr_sink_cookie_ = TF_INVALID_COOKIE;
    TfGuidAtom display_attr_atom_ = TF_INVALID_GUIDATOM;

    ITfCompartment* open_close_comp_ = nullptr;
    DWORD open_close_sink_cookie_ = TF_INVALID_COOKIE;
    bool updating_compartment_ = false;

    ITfComposition* composition_ = nullptr;
    ITfContext* active_context_ = nullptr;

    std::unique_ptr<zzhuyin::IDictionary> dict_;
    PersistedConfig config_{};
    std::unique_ptr<zzhuyin::ZhuyinEngine> engine_;
    CandidateWindow candidate_wnd_;
    LangBarButton* lang_bar_btn_ = nullptr;
};

} // namespace zzhuyin::tsf
