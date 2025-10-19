#include "vox_ecs.h"

#include <chrono>

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

struct Time
{
    float delta_time = 0.2f;
};

void update(vecs::Ecs &ecs)
{

    // Add to Schedule
    vecs::Schedule Update;

    ecs.addSystem<vecs::Write<Position>, vecs::Read<Velocity>>(Update, [](auto view, vecs::Entity e, Position &p, const Velocity &v)
                                                               {
                                                                   // Gets executed for each entity with matching components
                                                                   p.x += v.dx;
                                                                   p.y += v.dy; 
                                                                

                                                                   std::cout << "Pos";
                                                                

                                                                });

    // Run Systems Singlethreaded
    // ecs.runSchedule(Update);

    // RUn Systems multithreaded
    // ecs.runScheduleParallel(Update);

    // Its possible to run a custom system outside of a schedule,

    auto vecs_start = std::chrono::high_resolution_clock::now();

    ecs.forEach<vecs::Write<Position>, vecs::Read<Velocity>>([](auto view, vecs::Entity e, Position &p, const Velocity &v)

                                                             {
                                                                                  // Gets executed for each entity with matching components

                                                                                  std::cout << "Pos";


                                                                                  vecs::Ecs::get<Position>(view, 0);

                                                                                  p.x += v.dx;
                                                                                  p.y += v.dy; });

    auto vecs_end = std::chrono::high_resolution_clock::now();

    auto vecs_r = std::chrono::duration_cast<std::chrono::microseconds>(vecs_end - vecs_start).count();

    std::cout << "Vecs Time: " << vecs_r << " microseconds \n";
}

int main()
{

    vecs::Ecs ecs;

    ecs.insertResource<Time>({0.2f});

    constexpr uint NUM_E = 1'000000u;

    for (auto i = 0u; i < NUM_E; i++)
    {
        const auto entity = ecs.createEntity();

        ecs.addComponent<Position>(entity, {i * 5.f, i * 1.f});

        if (i % 2 == 0)
            ecs.addComponent<Velocity>(entity, {5 * 0.1f, i * 0.1f});
    }


    for (size_t i = 0; i < 5 ; i++)
    {
        update(ecs);
    }
    

    

    return 0;
}