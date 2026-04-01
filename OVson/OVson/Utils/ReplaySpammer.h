#pragma once
#include <string>

namespace Utils {
    class ReplaySpammer {
    public:
        static ReplaySpammer& getInstance();
        void toggle();
        void setEnabled(bool e);
        bool isEnabled() const;
        void tick();

    private:
        ReplaySpammer() = default;
        bool enabled = false;
        int state = 0;
        unsigned long long lastAction = 0;
        int spamCount = 0;
    };
}
