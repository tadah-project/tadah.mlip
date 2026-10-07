#pragma once
// Uses probe::Widget without including <probe/widget.h>.
namespace probe {
inline int widget_size(const Widget& w) { return w.n; }
}  // namespace probe
