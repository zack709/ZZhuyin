#include "core/candidate.h"

#include <algorithm>

namespace zzhuyin {

void CandidateManager::OpenHomophone(std::vector<std::u16string> items) {
    items_ = std::move(items);
    active_ = !items_.empty();
    is_associate_ = false;
    has_selection_ = active_;
    selected_index_ = 0;
}

void CandidateManager::OpenAssociate(std::vector<std::u16string> items) {
    items_ = std::move(items);
    active_ = !items_.empty();
    is_associate_ = true;
    has_selection_ = false;
    selected_index_ = 0;
}

void CandidateManager::Close() {
    active_ = false;
    is_associate_ = false;
    has_selection_ = false;
    selected_index_ = 0;
    items_.clear();
}

void CandidateManager::PageDown() {
    const size_t n = items_.size();
    if (!active_ || n <= kPageSize) {
        return;
    }
    if (selected_index_ + kPageSize < n) {
        selected_index_ += kPageSize;
    } else if (selected_index_ < n - (n % kPageSize)) {
        // On second-to-last page, but same column does not exist on last page -> jump to last item
        selected_index_ = n - 1;
    } else {
        // Already on last page -> wrap to matching column on first page
        selected_index_ = selected_index_ % kPageSize;
    }
}

void CandidateManager::PageUp() {
    if (!active_) {
        return;
    }
    if (selected_index_ >= kPageSize) {
        selected_index_ -= kPageSize;
    }
    // Intentionally does NOT wrap (Phase 4 §2.2)
}

void CandidateManager::MoveUp() {
    const size_t n = items_.size();
    if (!active_ || n == 0) {
        return;
    }
    if (!has_selection_) {
        has_selection_ = true;
        const size_t page_start = (selected_index_ / kPageSize) * kPageSize;
        selected_index_ = std::min(page_start + kPageSize, n) - 1;
        return;
    }
    selected_index_ = (selected_index_ > 0) ? (selected_index_ - 1) : (n - 1);
}

void CandidateManager::MoveDown() {
    const size_t n = items_.size();
    if (!active_ || n == 0) {
        return;
    }
    if (!has_selection_) {
        has_selection_ = true;
        selected_index_ = (selected_index_ / kPageSize) * kPageSize;
        return;
    }
    selected_index_ = (selected_index_ + 1 < n) ? (selected_index_ + 1) : 0;
}

std::optional<std::u16string> CandidateManager::SelectOnPage(size_t page_offset) const {
    if (!active_ || page_offset >= kPageSize) {
        return std::nullopt;
    }
    const size_t page_start = (selected_index_ / kPageSize) * kPageSize;
    const size_t idx = page_start + page_offset;
    if (idx >= items_.size()) {
        return std::nullopt;
    }
    return items_[idx];
}

std::optional<std::u16string> CandidateManager::SelectCurrent() const {
    if (!active_ || !has_selection_ || selected_index_ >= items_.size()) {
        return std::nullopt;
    }
    return items_[selected_index_];
}

CandidateView CandidateManager::View() const {
    CandidateView view;
    if (!active_) {
        return view;
    }
    view.active = true;
    view.is_associate = is_associate_;
    view.has_selection = has_selection_;
    view.total_count = items_.size();
    view.selected_index = selected_index_;
    view.page_start = (selected_index_ / kPageSize) * kPageSize;
    const size_t page_end = std::min(view.page_start + kPageSize, items_.size());
    view.page_items.assign(items_.begin() + view.page_start, items_.begin() + page_end);
    return view;
}

} // namespace zzhuyin
