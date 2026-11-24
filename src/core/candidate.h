#pragma once

#include "core/types.h"

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace zzhuyin {

class CandidateManager {
public:
    static constexpr size_t kPageSize = 9;

    void OpenHomophone(std::vector<std::u16string> items);
    void OpenAssociate(std::vector<std::u16string> items);
    void Close();

    bool IsActive() const { return active_; }
    bool IsAssociate() const { return is_associate_; }
    bool HasSelection() const { return has_selection_; }
    size_t Count() const { return items_.size(); }
    size_t SelectedIndex() const { return selected_index_; }

    // Pagination & cursor movement (exact Win7 IMTCCORE.DLL semantics)
    void PageDown(); // Space / PageDown (wraps from last page to first page)
    void PageUp();   // PageUp (does NOT wrap)
    void MoveUp();   // Up arrow (wraps 0 -> N-1)
    void MoveDown(); // Down arrow (wraps N-1 -> 0)

    // Select item on current page (page_offset in 0..8)
    std::optional<std::u16string> SelectOnPage(size_t page_offset) const;

    // Select currently highlighted item
    std::optional<std::u16string> SelectCurrent() const;

    CandidateView View() const;

private:
    bool active_ = false;
    bool is_associate_ = false;
    bool has_selection_ = false;
    size_t selected_index_ = 0;
    std::vector<std::u16string> items_;
};

} // namespace zzhuyin
