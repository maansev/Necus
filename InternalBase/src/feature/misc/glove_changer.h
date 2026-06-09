#pragma once

class c_glove_changer {
public:
    static c_glove_changer& get() { static c_glove_changer inst; return inst; }
    void run(int stage);
};
