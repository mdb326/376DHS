#include <unordered_map>
#include <iostream>
#include <shared_mutex>
//use bucket interface for locks?

template <typename T>
class DHS {
    public:
        DHS();
        T get(int key);
        bool put(int key, T val);

    private:
        std::unordered_map<int, T> m;
        int bucket_cnt;
        std::shared_mutex readMutex;

};

template <typename T>
DHS<T>::DHS(){
    bucket_cnt = m.bucket_count();
    // std::cout << "Cnt:" << bucket_cnt << std::endl;
}

template <typename T>
T DHS<T>::get(int key){
    readMutex.lock_shared();
    if (m.count(key)){
        // std::cout <<"Bucket: "<< m.bucket(key) << std::endl;
        readMutex.unlock_shared();
        return m[key];
    }
    readMutex.unlock_shared();
    return NULL;
}
template <typename T>
bool DHS<T>::put(int key, T val){
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