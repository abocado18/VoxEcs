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

template <size_t N>
void createDummySystems(vecs::Ecs &ecs, vecs::Schedule &schedule)
{
    for (size_t i = 0; i < N; i++)
    {
        ecs.addSystem<vecs::Read<Position>, vecs::Read<Health>>(schedule, [](vecs::Ecs::SystemView<vecs::Read<Position>, vecs::Read<Health>> view, vecs::Entity e, const Position& pos, const Health& h)
                                                                 {
            
            volatile uint64_t number = 0;

            auto *poss = view.get<vecs::Read<Position>>(4);

            

            number += pos.x; });
    }
}

int main()
{

    using namespace vecs;

    vecs::Ecs ecs;

    ecs.insertResource<float>(3.5f);

    constexpr size_t num_entities = 1'000'000;
    ;

    volatile double sum = 0.0;
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

    constexpr size_t num_systems = 650;

    vecs::Schedule Update;
    

    createDummySystems<num_systems>(ecs, Update);

    {
        auto start = std::chrono::high_resolution_clock::now();

        ecs.runSchedule(Update);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::micro> duration = end - start;

        std::cout << "Duration for single is: " << duration.count() << "microseconds\n";
    }

    {
        auto start = std::chrono::high_resolution_clock::now();

        ecs.runScheduleParallel(Update);

        auto end = std::chrono::high_resolution_clock::now();

        std::chrono::duration<double, std::micro> duration = end - start;

        std::cout << "Duration for paralell is: " << duration.count() << "microseconds\n";
    }
}
