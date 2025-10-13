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

int main()
{

    


    vox_ecs::Ecs ecs;

    
    ecs.insertResource<float>(3.5f);

    constexpr size_t num_entities = 3000000;

    

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

    // Benchmark single-component iteration
    auto start = std::chrono::high_resolution_clock::now();

    ecs.forEach<Position>([](vox_ecs::Ecs *ecs, vox_ecs::Entity e, Position &p)
                          {
                              p.x += 1.0f;
                          });
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Single-component (Position) iteration: "
              << std::chrono::duration<double, std::micro>(end - start).count()
              << " μs\n";

    // Benchmark two-component iteration
    start = std::chrono::high_resolution_clock::now();
    ecs.forEach<Position, Velocity>([](vox_ecs::Ecs *ecs, vox_ecs::Entity e, Position &p, Velocity &v)
                                    {
                                        p.x += v.x;
                                        p.y += v.y;
                                        p.z += v.z;
                                    });
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Two-component (Position + Velocity) iteration: "
              << std::chrono::duration<double, std::micro>(end - start).count()
              << " μs\n";

    // Benchmark three-component iteration
    start = std::chrono::high_resolution_clock::now();
    ecs.forEach<Position, Velocity, Health>([](vox_ecs::Ecs *ecs, vox_ecs::Entity e, Position &p, Velocity &v, Health &h)
                                            {

                                               float f= *ecs->getResource<float>();

                                                p.x += v.x * h.value * f;
                                                p.y += v.y * h.value;
                                                p.z += v.z * h.value;
                                            });
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Three-component (Position + Velocity + Health) iteration: "
              << std::chrono::duration<double, std::micro>(end - start).count()
              << " μs\n";

    return 0;
}
