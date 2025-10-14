#include "vox_ecs.h"
#include <chrono>
#include <iostream>

struct Position
{
    float x, y, z;
};
struct Velocity
{
    float x, y, z;
};
struct Health
{
    float value;
};

template <typename T>
constexpr bool is_const_val(T &&)
{
    return std::is_const<std::remove_reference_t<T>>::value;
}



void test_s(vox_ecs::Ecs *ecs)
{
    ecs->forEach<Velocity>([](vox_ecs::Ecs *ecs, vox_ecs::Entity entity, Velocity &vel)
    {
        vel.x += 4.5f;
        std::cout << "Increased";
    });
}

int main()
{

    


    vox_ecs::Ecs ecs;

    
    ecs.insertResource<float>(3.5f);

    constexpr size_t num_entities = 1;

    

    // Create entities and add components
    for (size_t i = 0; i < num_entities; ++i)
    {
        auto e = ecs.createEntity();
        ecs.addComponent<Position>(e, Position{float(i), float(i), float(i)});
        if (i % 2 == 0)
            ecs.addComponent<Velocity>(e, Velocity{0.1f, 0.2f, 0.3f});
        if (i % 4 == 0)
            ecs.addComponent<Health>(e, Health{100.0f});
    }


    ecs.addSystem(vox_ecs::Schedule::Startup, test_s);

    ecs.runSchedule(vox_ecs::Schedule::Startup);

    ecs.removeSystem(vox_ecs::Schedule::Startup, test_s);

    ecs.runSchedule(vox_ecs::Schedule::Startup);

    
}
