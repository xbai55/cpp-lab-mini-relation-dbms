#include "minidb.hpp"

namespace minidb {

BPlusTreeIndex::BPlusTreeIndex(bool string_key)
    : string_key_(string_key), root_(new Node(true)), size_(0) {}

BPlusTreeIndex::~BPlusTreeIndex() {
    clear_node(root_);
}

void BPlusTreeIndex::clear_node(Node* node) {
    if (!node) return;
    if (!node->leaf) {
        for (std::size_t i = 0; i < node->children.size(); ++i) clear_node(node->children[i]);
    }
    delete node;
}

int BPlusTreeIndex::compare_key(const Value& a, const Value& b) const {
    if (string_key_) return a.str_value.compare(b.str_value);
    if (a.int_value < b.int_value) return -1;
    if (a.int_value > b.int_value) return 1;
    return 0;
}

std::size_t BPlusTreeIndex::lower_bound_key(const MiniVector<Value>& keys, const Value& key) const {
    std::size_t l = 0, r = keys.size();
    while (l < r) {
        std::size_t m = (l + r) / 2;
        if (compare_key(keys[m], key) < 0) l = m + 1;
        else r = m;
    }
    return l;
}

BPlusTreeIndex::Node* BPlusTreeIndex::find_leaf(const Value& key) const {
    Node* node = root_;
    while (node && !node->leaf) {
        std::size_t pos = lower_bound_key(node->keys, key);
        if (pos < node->keys.size() && compare_key(node->keys[pos], key) == 0) ++pos;
        node = node->children[pos];
    }
    return node;
}

bool BPlusTreeIndex::insert_recursive(Node* node, const Value& key, long offset, Value& promote_key, Node*& promote_child) {
    if (node->leaf) {
        std::size_t pos = lower_bound_key(node->keys, key);
        if (pos < node->keys.size() && compare_key(node->keys[pos], key) == 0) return false;

        node->keys.push_back(key);
        node->offsets.push_back(offset);
        for (std::size_t i = node->keys.size() - 1; i > pos; --i) {
            node->keys[i] = node->keys[i - 1];
            node->offsets[i] = node->offsets[i - 1];
        }
        node->keys[pos] = key;
        node->offsets[pos] = offset;
        ++size_;

        if (node->keys.size() < ORDER) {
            promote_child = nullptr;
            return true;
        }

        Node* right = new Node(true);
        std::size_t mid = node->keys.size() / 2;
        while (mid < node->keys.size()) {
            right->keys.push_back(node->keys[mid]);
            right->offsets.push_back(node->offsets[mid]);
            node->keys.erase(mid);
            node->offsets.erase(mid);
        }
        right->next = node->next;
        node->next = right;
        promote_key = right->keys[0];
        promote_child = right;
        return true;
    }

    std::size_t child_pos = lower_bound_key(node->keys, key);
    if (child_pos < node->keys.size() && compare_key(node->keys[child_pos], key) == 0) ++child_pos;

    Value child_key;
    Node* child_split = nullptr;
    if (!insert_recursive(node->children[child_pos], key, offset, child_key, child_split)) return false;
    if (!child_split) {
        promote_child = nullptr;
        return true;
    }

    insert_into_parent(node, child_pos, child_key, child_split);
    if (node->keys.size() < ORDER) {
        promote_child = nullptr;
        return true;
    }

    Node* right = new Node(false);
    std::size_t mid = node->keys.size() / 2;
    promote_key = node->keys[mid];

    for (std::size_t i = mid + 1; i < node->keys.size();) {
        right->keys.push_back(node->keys[i]);
        node->keys.erase(i);
    }
    for (std::size_t i = mid + 1; i < node->children.size();) {
        right->children.push_back(node->children[i]);
        node->children.erase(i);
    }
    node->keys.erase(mid);
    promote_child = right;
    return true;
}

void BPlusTreeIndex::insert_into_parent(Node* parent, std::size_t child_pos, const Value& key, Node* child) {
    parent->keys.push_back(key);
    for (std::size_t i = parent->keys.size() - 1; i > child_pos; --i) parent->keys[i] = parent->keys[i - 1];
    parent->keys[child_pos] = key;

    parent->children.push_back(child);
    for (std::size_t i = parent->children.size() - 1; i > child_pos + 1; --i) {
        parent->children[i] = parent->children[i - 1];
    }
    parent->children[child_pos + 1] = child;
}

bool BPlusTreeIndex::insert(const Value& key, long offset) {
    Value promote_key;
    Node* promote_child = nullptr;
    if (!insert_recursive(root_, key, offset, promote_key, promote_child)) return false;
    if (promote_child) {
        Node* new_root = new Node(false);
        new_root->keys.push_back(promote_key);
        new_root->children.push_back(root_);
        new_root->children.push_back(promote_child);
        root_ = new_root;
    }
    return true;
}

bool BPlusTreeIndex::find(const Value& key, long* offset) const {
    Node* leaf = find_leaf(key);
    if (!leaf) return false;
    std::size_t pos = lower_bound_key(leaf->keys, key);
    if (pos >= leaf->keys.size() || compare_key(leaf->keys[pos], key) != 0) return false;
    if (offset) *offset = leaf->offsets[pos];
    return true;
}

bool BPlusTreeIndex::remove(const Value& key) {
    Node* leaf = find_leaf(key);
    if (!leaf) return false;
    std::size_t pos = lower_bound_key(leaf->keys, key);
    if (pos >= leaf->keys.size() || compare_key(leaf->keys[pos], key) != 0) return false;
    leaf->keys.erase(pos);
    leaf->offsets.erase(pos);
    --size_;
    return true;
}

void BPlusTreeIndex::clear() {
    clear_node(root_);
    root_ = new Node(true);
    size_ = 0;
}

bool BPlusTreeIndex::load(const std::filesystem::path& path) {
    clear();
    std::ifstream in(path);
    if (!in.good()) return true;
    std::string type;
    std::getline(in, type);
    string_key_ = type == "string";
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream ss(line);
        std::string key_part;
        std::string off_part;
        if (!std::getline(ss, key_part, '\t') || !std::getline(ss, off_part)) continue;
        Value key;
        key.type = string_key_ ? ColumnType::String : ColumnType::Int;
        if (string_key_) key.str_value = key_part;
        else key.int_value = std::stoi(key_part);
        insert(key, std::stol(off_part));
    }
    return true;
}

void BPlusTreeIndex::save_leaf_chain(std::ofstream& out) const {
    Node* node = root_;
    while (node && !node->leaf) node = node->children[0];
    while (node) {
        for (std::size_t i = 0; i < node->keys.size(); ++i) {
            out << value_to_string(node->keys[i]) << "\t" << node->offsets[i] << "\n";
        }
        node = node->next;
    }
}

bool BPlusTreeIndex::save(const std::filesystem::path& path) const {
    std::ofstream out(path, std::ios::trunc);
    if (!out.good()) return false;
    out << (string_key_ ? "string" : "int") << "\n";
    save_leaf_chain(out);
    return true;
}

} // namespace minidb

