#include <unordered_map>
#include <iostream>
#include <shared_mutex>
#include <vector>

class DHS {
    public:
        DHS();
        std::vector<uint8_t> get(int key);
        bool put(int key, std::vector<uint8_t> val);

    private:
        std::unordered_map<int, std::vector<uint8_t>> m;
        std::shared_mutex readMutex;

};

DHS::DHS(){
    // std::cout << "Cnt:" << bucket_cnt << std::endl;
}

std::vector<uint8_t> DHS::get(int key){
    readMutex.lock_shared();
    if (m.count(key)){
        // std::cout <<"Bucket: "<< m.bucket(key) << std::endl;
        std::vector<uint8_t> val = m[key];
        readMutex.unlock_shared();
        return val;
    }
    readMutex.unlock_shared();
    return std::vector<uint8_t>();
}

bool DHS::put(int key, std::vector<uint8_t> val){
    readMutex.lock();
    if (m.count(key)){
        readMutex.unlock();
        return false;
    }
    m[key] = val;
    readMutex.unlock();
    // if (cnt != bucket_cnt){
    //     std::cout << "Grew" << std::endl;
    // }
    return true;
}