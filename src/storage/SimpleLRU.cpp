#include "SimpleLRU.h"
#include <iostream>
namespace Afina {
namespace Backend {

void SimpleLRU::delete_node_from_list(lru_node *node) {
    if (node == _lru_head.get()) {
        if (node->next.get() == nullptr) {
            _lru_head.reset(nullptr);
            return;
        }
        node->next->prev = node->prev;
        std::swap(node->next, _lru_head);
        node->next.reset(nullptr);
        return;
    }
    node->next->prev = node->prev;
    std::swap(node->next, node->prev->next);
    node->next.reset(nullptr);
    return;
}

void SimpleLRU::to_end(lru_node *node) {
    if (node->next.get() == nullptr) {
        return;
    }
    if (node == _lru_head.get()) {
		std::swap(node->prev->next, _lru_head);
		std::swap(node->next, _lru_head);
		_lru_head->prev = node;
		return;
    }
	node->next->prev = node->prev;
    std::swap(node->prev->next, node->next);
    std::swap(node->next, _lru_head->prev->next);
    node->prev = _lru_head->prev;
    _lru_head->prev = node;
    return;
}

bool SimpleLRU::Put(const std::string &key, const std::string &value) {
    if (key.size() + value.size() > _max_size) {
        return false;
    }
    auto cur = _lru_index.find(key);
   if (cur == _lru_index.end()) {
        size_t elem_size = key.size() + value.size();
        while (elem_size + current_size > _max_size) {
            Delete(_lru_head->key);
        }
        lru_node *tmp = new lru_node{key, value};
        if (_lru_head.get() != nullptr) {
            tmp->prev = _lru_head->prev;
            std::cout<<"AAAAA "<<_lru_head->prev->next.get()<<std::endl;
            _lru_head->prev->next.reset(tmp);
            _lru_head->prev = tmp;
            tmp->next.reset(nullptr);
        } else {
            _lru_head.reset(tmp);
            tmp->next.reset(nullptr);
            tmp->prev = tmp;
        }
        _lru_index.insert(
            std::make_pair(std::reference_wrapper<std::string>(tmp->key), std::reference_wrapper<lru_node>(*tmp)));
        current_size += elem_size;
        return true;
    } else {
        return Set(key, value);
    }
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::PutIfAbsent(const std::string &key, const std::string &value) {
   if (key.size() + value.size() > _max_size) {
        return false;
    }
    if (_lru_index.find(key) != _lru_index.end()) {
        return false;
    }
    return Put(key, value);
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Set(const std::string &key, const std::string &value) {
    if (value.size() + key.size() > _max_size) {
        return false;
    }
    auto cur = _lru_index.find(key);
    if (cur == _lru_index.end()) {
        return false;
    }
    lru_node &cur_node = cur->second;
    int difference = value.size() - cur_node.value.size();
    to_end(&cur_node);
    if (difference > 0) {
        while (difference + current_size > _max_size) {
            Delete(_lru_head->key);
        }
    }
    cur_node.value = value;
    return true;
}

// See MapBasedGlobalLockmpl.h
bool SimpleLRU::Delete(const std::string &key) {
    auto cur = _lru_index.find(key);
    if (cur == _lru_index.end()) {
        return false;
    }
    lru_node &cur_node = cur->second;
    current_size -= (cur_node.key.size() + cur_node.value.size());
    _lru_index.erase(cur);
    delete_node_from_list(&cur_node);
    return true;
}

// See MapBasedGlobalLockImpl.h
bool SimpleLRU::Get(const std::string &key, std::string &value) {
    auto cur = _lru_index.find(key);
    if (cur == _lru_index.end()) {
        return false;
    }
    lru_node &cur_node = cur->second;
    value = cur_node.value;
    to_end(&cur_node);
    return true;
}

} // namespace Backend
} // namespace Afina
