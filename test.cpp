#include "DHS.hpp"


int main() {
    DHS<int> map = DHS<int>();

    std::cout << map.get(5) << std::endl;
    std::cout << map.put(5, 6) << std::endl;
    std::cout << map.get(5) << std::endl;
}