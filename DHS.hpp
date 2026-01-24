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

};

template <typename T>
DHS<T>::DHS(){
    
}

template <typename T>
T DHS<T>::get(int key){
    if (m.count(key)){
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
    return true;
}