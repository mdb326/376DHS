#include <unordered_map>
#include <iostream>
#include <shared_mutex>
#include <vector>

class DHSList {
    public:
        DHSList(int _size);
        std::vector<uint8_t> get(int key);
        bool put(int key, std::vector<uint8_t> val);

    private:
        std::vector<std::vector<uint8_t>> m;
        std::vector<bool> puts;
        std::vector<std::unique_ptr<std::shared_mutex>> readMutex;
        int size;

};

DHSList::DHSList(int _size){
    size = _size;
    m.resize(size);
    puts.resize(size, false);
    for (int i = 0; i < size; i++){
        readMutex.emplace_back(std::make_unique<std::shared_mutex>());
    }
}

std::vector<uint8_t> DHSList::get(int key){
    int index = key % size;
    readMutex[index]->lock_shared();
    if (puts[index]){
        auto res = m[index];
        readMutex[index]->unlock_shared();
        return res;
    }
    readMutex[index]->unlock_shared();
    return std::vector<uint8_t>();
}

bool DHSList::put(int key, std::vector<uint8_t> val){
    int index = key % size;
    readMutex[index]->lock();
    if (!puts[index]){
        readMutex[index]->unlock();
        puts[index] = true;
        m[index] = val;
        return true;
    }
    m[index] = val;
    readMutex[index]->unlock();
    return false;
}