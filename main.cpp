#include "vox_ecs.h"

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

void update(vecs::Ecs &ecs)
{

    // Add to Schedule
    vecs::Schedule Update;

    ecs.addSystem<vecs::Write<Position>, vecs::Read<Velocity>>(Update, [](auto view, vecs::Entity e, Position &p, const Velocity &v)
                                                               {
                                                                   // Gets executed for each entity with matching components
                                                                   p.x += v.dx;
                                                                   p.y += v.dy; });

    // Run Systems Singlethreaded
    ecs.runSchedule(Update);

    // RUn Systems multithreaded
    ecs.runScheduleParallel(Update);

    // Its possible to run a custom system outside of a schedule,

    ecs.forEach<vecs::Write<Position>, vecs::Read<Velocity>>([](auto view, vecs::Entity e, Position &p, const Velocity &v)

                                                             {
        //Gets executed for each entity with matching components
        p.x += v.dx;
        p.y += v.dy; });
}

int main()
{

    vecs::Ecs ecs;

    for (auto i = 0u; i < 1000u; i++)
    {
        const auto entity = ecs.createEntity();

        ecs.addComponent<Position>(entity, {i * 1.f, i * 1.f});

        if (i % 2 == 0)
            ecs.addComponent<Velocity>(entity, {i * 0.1f, i * 0.1f});
    }

    update(ecs);

    return 0;
}