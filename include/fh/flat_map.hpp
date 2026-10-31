#ifndef FH_FLAT_MAP_HPP_
#define FH_FLAT_MAP_HPP_

#include <cstddef>
#include <cstdint>
#include <vector>

namespace fh {

// A hash map for the hot path: open addressing with linear probing, entries stored in
// one array, no allocation per entry. Erasing shifts later entries back, so there are
// no tombstones and lookups never slow down as entries come and go.
//
// It is unordered. Code that needs sorted output builds it from `for_each`.
// `Hash` must give well-mixed low bits (the table is a power of two in size).
template <typename Key, typename Value, typename Hash>
class FlatMap {
 public:
  [[nodiscard]] std::size_t size() const noexcept { return size_; }
  [[nodiscard]] bool empty() const noexcept { return size_ == 0; }

  [[nodiscard]] const Value* find(const Key& key) const noexcept {
    return const_cast<FlatMap*>(this)->find(key);
  }

  [[nodiscard]] Value* find(const Key& key) noexcept {
    if (size_ == 0) return nullptr;
    for (std::size_t i = home(key);; i = next(i)) {
      if (!used_[i]) return nullptr;
      if (entries_[i].key == key) return &entries_[i].value;
    }
  }

  [[nodiscard]] bool contains(const Key& key) const noexcept { return find(key) != nullptr; }

  // Inserts if absent. Returns false, leaving the old value, if the key is there.
  bool insert(const Key& key, const Value& value) {
    grow_for_one_more();
    std::size_t i = home(key);
    for (; used_[i]; i = next(i)) {
      if (entries_[i].key == key) return false;
    }
    used_[i] = 1;
    entries_[i] = Entry{key, value};
    ++size_;
    return true;
  }

  // The value at `key`, inserting `fresh` first if it is absent.
  Value& get_or_insert(const Key& key, const Value& fresh) {
    grow_for_one_more();
    std::size_t i = home(key);
    for (; used_[i]; i = next(i)) {
      if (entries_[i].key == key) return entries_[i].value;
    }
    used_[i] = 1;
    entries_[i] = Entry{key, fresh};
    ++size_;
    return entries_[i].value;
  }

  bool erase(const Key& key) noexcept {
    if (size_ == 0) return false;
    std::size_t i = home(key);
    for (;; i = next(i)) {
      if (!used_[i]) return false;
      if (entries_[i].key == key) break;
    }
    // Close the hole: pull back every following entry that is allowed to sit in it.
    std::size_t hole = i;
    for (std::size_t j = next(hole); used_[j]; j = next(j)) {
      const std::size_t h = home(entries_[j].key);
      // Entry j may move into the hole unless its home lies cyclically in (hole, j].
      const bool home_in_range = (hole <= j) ? (hole < h && h <= j) : (hole < h || h <= j);
      if (home_in_range) continue;
      entries_[hole] = entries_[j];
      hole = j;
    }
    used_[hole] = 0;
    --size_;
    return true;
  }

  template <typename Fn>
  void for_each(Fn&& fn) const {
    for (std::size_t i = 0; i < entries_.size(); ++i) {
      if (used_[i]) fn(entries_[i].key, entries_[i].value);
    }
  }

 private:
  struct Entry {
    Key key{};
    Value value{};
  };

  [[nodiscard]] std::size_t home(const Key& key) const noexcept {
    return static_cast<std::size_t>(Hash{}(key)) & (entries_.size() - 1);
  }
  [[nodiscard]] std::size_t next(std::size_t i) const noexcept {
    return (i + 1) & (entries_.size() - 1);
  }

  // Keeps the table at most 70% full, so a probe always ends at an empty slot.
  void grow_for_one_more() {
    if (entries_.empty()) {
      entries_.resize(kInitial);
      used_.assign(kInitial, 0);
      return;
    }
    if ((size_ + 1) * 10 <= entries_.size() * 7) return;
    std::vector<Entry> old_entries(entries_.size() * 2);
    std::vector<std::uint8_t> old_used(old_entries.size(), 0);
    old_entries.swap(entries_);
    old_used.swap(used_);
    for (std::size_t k = 0; k < old_entries.size(); ++k) {
      if (!old_used[k]) continue;
      std::size_t i = home(old_entries[k].key);
      while (used_[i]) i = next(i);
      used_[i] = 1;
      entries_[i] = old_entries[k];
    }
  }

  static constexpr std::size_t kInitial = 16;

  std::vector<Entry> entries_;
  std::vector<std::uint8_t> used_;
  std::size_t size_{0};
};

// A 64-bit integer mixer (the finaliser of splitmix64), so that sequential order ids
// and neighbouring prices spread over the table.
constexpr std::uint64_t mix64(std::uint64_t x) noexcept {
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return x;
}

}  // namespace fh

#endif  // FH_FLAT_MAP_HPP_
