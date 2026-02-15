#include <unordered_map>
#include <iostream>
#include <shared_mutex>
#include <vector>

class DHSList {
    public:
        DHSList(int _size);
        std::vector<uint8_t> get(int key);
        bool put(int key, std::vector<uint8_t> val);
        void getLock(int key);
        void unLock(int key);

    private:
        std::vector<std::vector<uint8_t>> m;
        std::vector<bool> puts;
        std::vector<std::unique_ptr<std::shared_mutex>> readMutex;
        // std::vector<std::string> serverLockings; //if something is locked, hold what is lockign it
        int size;

};

DHSList::DHSList(int _size){
    size = _size;
    m.resize(size);
    puts.resize(size, false);
    // serverLockings.resize(size, "");
    for (int i = 0; i < size; i++){
        readMutex.emplace_back(std::make_unique<std::shared_mutex>());
    }
}

std::vector<uint8_t> DHSList::get(int key){
    int index = key / size;
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
    //Locking must be done before calling these
    int index = key % size;
    // readMutex[index]->lock();
    if (!puts[index]){ 
        puts[index] = true;
        m[index] = val;
        // readMutex[index]->unlock();
        return true;
    }
    m[index] = val;
    // readMutex[index]->unlock();
    return false;
}
void DHSList::getLock(int key){ //, std::string serverIP
    //since the same server will be telling everyone to lock it we can skip it if it is already locked by that ip
    //actaully jk, make it on the server to only lock things once
    int index = key % size;
    // if(serverLockings[index] == serverIP){
    //     return;
    // }
    readMutex[index]->lock();
    // serverLockings[index] = serverIP; //slight concerns about deadlock here
}
void DHSList::unLock(int key){
    int index = key % size;
    readMutex[index]->unlock();
    // serverLockings[index] = "";
}