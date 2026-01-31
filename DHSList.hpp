#include <unordered_map>
#include <iostream>
#include <shared_mutex>
#include <vector>

class DHSList {
    public:
        DHSList();
        std::vector<uint8_t> get(int key);
        bool put(int key, std::vector<uint8_t> val);

    private:
        std::vector<std::vector<uint8_t>> m;
        std::vector<bool> puts;
        std::vector<std::shared_mutex> mutexes;
        int size;

};

DHSList::DHSList(int _size){
    size = _size;
    m.resize(size);
    puts.resize(size);
    for (int i = 0; i < size; i++){
        mutexes.emplace_back();
        puts[i] = false;
    }
}

std::vector<uint8_t> DHSList::get(int key){
    int index = key % size;
    readMutex[index].lock_shared();
    if (puts[index]){
        auto res = m[key];
        readMutex.unlock_shared();
        return res;
    }
    readMutex[index].unlock_shared();
    return std::vector<uint8_t>();
}

bool DHSList::put(int key, std::vector<uint8_t> val){
    int index = key % size;
    readMutex.lock();
    if (puts[index]){
        readMutex.unlock();
        return false;
    }
    m[key] = val;
    readMutex.unlock();
    return true;
}