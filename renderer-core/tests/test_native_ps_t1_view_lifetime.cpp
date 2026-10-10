#include "dsrrl/runtime/native_ps_t1_view_lifetime.hpp"
#include <cassert>
int main() {
    dsrrl::runtime::native_ps_t1_lifetime::registry<2> r;
    assert(!r.current(10,20));
    const auto a=r.init(1,10,20);
    assert(a!=0 && r.current(10,20).epoch==a);
    assert(!r.current(10,21));
    assert(r.init(1,10,20)==0 && !r.current(10,20));
    r.destroy(1,10);
    const auto b=r.init(1,10,20);
    assert(b!=0 && b!=a);
    r.destroy(1,10);
    const auto c=r.init(2,10,21);
    assert(c!=0 && c!=b && !r.current(10,20));
    assert(r.current(10,21).device==2);
    r.destroy_device(2);
    assert(!r.current(10,21));
    assert(r.init(3,10,20)!=0);
    assert(r.init(4,11,21)!=0);
    assert(r.init(5,12,22)==0);
    r.destroy(3,10);
    assert(r.init(5,12,22)!=0);
    assert(r.init(7,12,22)==0 && !r.current(12,22));
}
