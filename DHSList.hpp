#include <unordered_map>
#include <iostream>
#include <shared_mutex>
#include <vector>

class DHSList {
    public:
        DHSList(int _size);
        std::vector<uint8_t> get(int key);
        bool put(int key, std::vector<uint8_t> val);
        bool getLock(int key, int operation);
        bool unLock(int key, int operation);

    private:
        std::vector<std::vector<uint8_t>> m;
        std::vector<bool> puts;
        std::vector<std::unique_ptr<std::mutex>> readMutex;
        std::vector<int> serverLockings; //if something is locked, hold what is lockign it
        std::vector<std::unique_ptr<std::mutex>> serverLockingsLocks;
        int size;

};

DHSList::DHSList(int _size){
    size = _size;
    m.resize(size);
    puts.resize(size, false);
    serverLockings.resize(size, NULL);
    for (int i = 0; i < size; i++){
        readMutex.emplace_back(std::make_unique<std::mutex>());
        serverLockingsLocks.emplace_back(std::make_unique<std::mutex>());
    }
}

std::vector<uint8_t> DHSList::get(int key){
    int index = key % size;
    // readMutex[index]->lock_shared();
    if (puts[index]){
        auto res = m[index];
        // readMutex[index]->unlock_shared();
        return res;
    }
    // readMutex[index]->unlock_shared();
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
    return true;
}
bool DHSList::getLock(int key, int operation){ //, std::string serverIP
    //since the same server will be telling everyone to lock it we can skip it if it is already locked by that ip
    //actaully jk, make it on the server to only lock things once
    int index = key % size;
    serverLockingsLocks[index]->lock();
    if(serverLockings[index] == operation){
        serverLockingsLocks[index]->unlock();
        return true;
    }
    bool got = readMutex[index]->try_lock();
    if(got) serverLockings[index] = operation;
    serverLockingsLocks[index]->unlock();
    return got;
}
bool DHSList::unLock(int key, int operation){
    int index = key % size;
    serverLockingsLocks[index]->lock();
    if(serverLockings[index] != operation){
        serverLockingsLocks[index]->unlock();
        return false;
    }
    serverLockings[index] = NULL;
    readMutex[index]->unlock();
    serverLockingsLocks[index]->unlock();
    return true;
}

