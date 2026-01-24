#include <unordered_map>
#include <iostream>
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

};

template <typename T>
DHS<T>::DHS(){
    bucket_cnt = m.bucket_count();
    // std::cout << "Cnt:" << bucket_cnt << std::endl;
}

template <typename T>
T DHS<T>::get(int key){
    if (m.count(key)){
        // std::cout <<"Bucket: "<< m.bucket(key) << std::endl;
        return m[key];
    }
    return NULL;
}
template <typename T>
bool DHS<T>::put(int key, T val){
    if (m.count(key)){
        return false;
    }
    m[key] = val;
    int cnt = m.bucket_count();
    // if (cnt != bucket_cnt){
    //     std::cout << "Grew" << std::endl;
    // }
    return true;
}