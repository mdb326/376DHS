std::vector<uint8_t> serialize(int val){
    std::vector<uint8_t> res;
    res.reserve(5);

    //1 for int
    res.push_back(1);
    uint8_t buf[4];

    std::memcpy(buf, &val, 4);
    res.insert(res.end(), buf, buf + 4);

    return res;
}
int deserializeInt(std::vector<uint8_t>& vals){
    int val;
    std::memcpy(&val, &vals[1], 4);

    return val;
}