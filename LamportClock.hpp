#include <atomic>
#include <algorithm>

struct LamportTimestamp {
    uint64_t time;
    uint16_t node_id;
};

class LamportClock {
private:
    std::atomic<uint64_t> clock;
    uint16_t node_id;

public:
    LamportClock(uint16_t id) : clock(0), node_id(id) {}

    LamportTimestamp tick() {
        uint64_t new_time = clock.fetch_add(1) + 1;
        return {new_time, node_id};
    }

    LamportTimestamp receive(uint64_t remote_time) {
        uint64_t local_time = clock.load();

        uint64_t new_time =
            std::max(local_time, remote_time) + 1;

        clock.store(new_time);

        return {new_time, node_id};
    }
};