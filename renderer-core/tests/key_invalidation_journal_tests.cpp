#include "dsrrl/runtime/key_invalidation_journal.hpp"
#include <cstdint>
#include <cstdio>
#include <mutex>
#include <thread>
#include <atomic>

namespace {
int fail(const char *msg) { std::fprintf(stderr,"%s\n",msg);return 1; }
}

int main()
{
    dsrrl::runtime::key_invalidation_journal<8u> journal;
    std::uint64_t e = journal.current_epoch();
    std::uint64_t next = 0u;
    if (!journal.survives(0x11u,e,next) || next!=e)
        return fail("initial positive identity");

    journal.publish(0x22u);
    if (!journal.survives(0x11u,e,next) || next!=e+1u)
        return fail("unrelated SRV destruction must preserve owner identity");

    journal.publish(0x11u);
    if (journal.survives(0x11u,e,next))
        return fail("same native SRV rebinding must invalidate even with pointer reuse");

    e=journal.current_epoch();
    journal.publish(0u);
    if (journal.survives(0x11u,e,next))
        return fail("device lifetime/reset sentinel must invalidate all");

    e=journal.current_epoch();
    for(unsigned i=0;i<8u;++i)
        journal.publish(0x100u + i);
    if (journal.survives(0x11u,e,next))
        return fail("overwritten bounded journal must fail closed");

    // Reader/writer overlap: no retained verdict may survive a mutation
    // for its own key once the writer has published it.
    dsrrl::runtime::key_invalidation_journal<512u> concurrent;
    std::mutex writer_mutex;
    std::atomic<bool> running{true};
    std::atomic<bool> broken{false};
    std::thread reader([&]() {
        while(running.load(std::memory_order_acquire)) {
            const auto observed=concurrent.current_epoch();
            std::uint64_t now=0u;
            if (concurrent.survives(0x3333u,observed,now) &&
                now < observed)
                broken.store(true,std::memory_order_release);
        }
    });
    for(unsigned i=0;i<2000u;++i) {
        std::lock_guard<std::mutex> lock(writer_mutex);
        concurrent.publish(i%97u==0u ? 0x3333u : 0x4444u);
    }
    running.store(false,std::memory_order_release);
    reader.join();
    if (broken.load(std::memory_order_acquire))
        return fail("concurrent snapshot went backwards");
    return 0;
}
