#include <unordered_map>
#include <iostream>
#include <shared_mutex>
#include <vector>

struct VersionedValue {
    uint64_t version;
    std::vector<uint8_t> data;
};


class DHSVersions {
    public:
        DHSVersions(int _size);
        VersionedValue get(int key);
        bool put(int key, VersionedValue val);
        bool getLock(int key, int operation);
        bool unLock(int key, int operation);

    private:
        std::vector<VersionedValue> m;
        std::vector<bool> puts;
        std::vector<std::unique_ptr<std::shared_mutex>> readMutex;
        std::vector<int> serverLockings; //if something is locked, hold what is lockign it
        std::vector<std::unique_ptr<std::shared_mutex>> serverLockingsLocks;
        int size;
};

DHSVersions::DHSVersions(int _size){
    size = _size;
    m.resize(size);
    puts.resize(size, false);
    serverLockings.resize(size, NULL);
    for (int i = 0; i < size; i++){
        readMutex.emplace_back(std::make_unique<std::shared_mutex>());
        serverLockingsLocks.emplace_back(std::make_unique<std::shared_mutex>());
    }
}

VersionedValue DHSVersions::get(int key){
    int index = key % size;
    VersionedValue val = {
        0,
        std::vector<uint8_t>()
    };
    readMutex[index]->lock_shared();
    if (puts[index]){
        auto res = m[index];
        readMutex[index]->unlock_shared();
        return res;
    }
    readMutex[index]->unlock_shared();
    return val;
}

bool DHSVersions::put(int key, VersionedValue val){
    //Locking must be done before calling these
    int index = key % size;
    readMutex[index]->lock();
    if (!puts[index] || val.version > m[index].version){ 
        puts[index] = true;
        m[index] = val;
        readMutex[index]->unlock();
        return true;
    }
    // m[index] = val;
    readMutex[index]->unlock();
    return false;
}
bool DHSVersions::getLock(int key, int operation){ //, std::string serverIP
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
bool DHSVersions::unLock(int key, int operation){
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

