#include "vox_ecs.h"
#include "entt.hpp"

#include<chrono>

struct Position
{
    float x;
    float y;
};

struct Velocity
{
    float dx;
    float dy;
};

void update(vecs::Ecs &ecs, entt::registry &registry)
{

    // Add to Schedule
    vecs::Schedule Update;

    ecs.addSystem<vecs::Write<Position>, vecs::Read<Velocity>>(Update, [](auto view, vecs::Entity e, Position &p, const Velocity &v)
                                                               {
                                                                   // Gets executed for each entity with matching components
                                                                   p.x += v.dx;
                                                                   p.y += v.dy; });

    // Run Systems Singlethreaded
    //ecs.runSchedule(Update);

    // RUn Systems multithreaded
    //ecs.runScheduleParallel(Update);

    // Its possible to run a custom system outside of a schedule,


    auto vecs_start = std::chrono::high_resolution_clock::now();

    ecs.forEach<vecs::Write<Position>, vecs::Read<Velocity>>([](auto view, vecs::Entity e, Position &p, const Velocity &v)

                                                             {
        //Gets executed for each entity with matching components
        p.x += v.dx;
        p.y += v.dy; });


    auto vecs_end = std::chrono::high_resolution_clock::now();
    
    
    auto vecs_r = std::chrono::duration_cast<std::chrono::microseconds>(vecs_end - vecs_start).count();
    
    
    auto entt_start = std::chrono::high_resolution_clock::now();

    

    auto view = registry.view<Position, const Velocity>();
    view.each([](const auto entity, auto &pos, const auto &vel)
              { 
        pos.x += vel.dx;
        pos.y += vel.dy; });


    auto entt_end = std::chrono::high_resolution_clock::now();

    auto eentt_r = std::chrono::duration_cast<std::chrono::microseconds>(entt_end - entt_start).count();

    std::cout << "Vecs Time: " << vecs_r << " microseconds \n";
    std::cout << "Entt Time: " << eentt_r << " microseconds \n";

    std::cout << "Difference in speed is " << (vecs_r / eentt_r) * 100 << " Prozent\n\n";
}

int main()
{

    vecs::Ecs ecs;

    constexpr uint NUM_E = 5'000'000u;

    for (auto i = 0u; i < NUM_E; i++)
    {
        const auto entity = ecs.createEntity();

        ecs.addComponent<Position>(entity, {i * 1.f, i * 1.f});

        if (i % 2 == 0)
            ecs.addComponent<Velocity>(entity, {i * 0.1f, i * 0.1f});
    }

    entt::registry registry;

    for (auto i = 0u; i < NUM_E; ++i)
    {
        const auto entity = registry.create();
        registry.emplace<Position>(entity, i * 1.f, i * 1.f);
        if (i % 2 == 0)
        {
            registry.emplace<Velocity>(entity, i * .1f, i * .1f);
        }
    }

    for(auto i = 0; i < 3; i++)
    {
        update(ecs, registry);
    }

    

    return 0;
}