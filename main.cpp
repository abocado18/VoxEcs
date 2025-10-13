#include "vox_ecs.h"

#include <iostream>

#include <string>

#include<type_traits>

struct Summer
{
    uint64_t a;
};

struct Number
{
    float a = 4.5f;
};


int main(int, char **)
{
    using namespace vox_ecs;

    volatile uint64_t total_sum = 0;

    constexpr uint64_t NUMBER = 1000000;


    int a = 3;

    assert(std::is_const(a));

    Ecs ecs = Ecs();

    for (size_t i = 0; i < NUMBER; i++)
    {
        Entity e = ecs.createEntity();

        Summer s = {};
        s.a = i;

        ecs.addComponent<Summer>(e, s);
    }

    Number v;
    v.a = 3.5f;

    ecs.insertResource<Number>(v);

    ecs.forEach<Summer>([&total_sum](Entity &e, Summer &s) {
        total_sum += s.a;
    });


    
    auto b = ecs.getResource<Number>();

    float c = b->a;

    std::cout << c << "\n";

    std::cout << total_sum;
}
