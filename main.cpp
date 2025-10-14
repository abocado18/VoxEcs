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

    


    vecs::Ecs ecs;

    
    ecs.insertResource<float>(3.5f);

    constexpr size_t num_entities = 1000000;

    
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


    

    // Benchmark single-component iteration
    auto start = std::chrono::high_resolution_clock::now();

    ecs.forEach<Position>([&](vecs::Ecs *ecs, vecs::Entity e, Position &p)
                          {
                              p.x += 1.0f;
                              sum += p.x;
                          });
    auto end = std::chrono::high_resolution_clock::now();
    std::cout << "Single-component (Position) iteration: "
              << std::chrono::duration<double, std::micro>(end - start).count()
              << " μs\n";

    // Benchmark two-component iteration
    start = std::chrono::high_resolution_clock::now();
    ecs.forEach<Position, Velocity>([&](vecs::Ecs *ecs, vecs::Entity e, Position &p, Velocity &v)
                                    {
                                        p.x += v.x;
                                        p.y += v.y;
                                        p.z += v.z;

                                        sum += p.x;

                                    });
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Two-component (Position + Velocity) iteration: "
              << std::chrono::duration<double, std::micro>(end - start).count()
              << " μs\n";

    // Benchmark three-component iteration
    start = std::chrono::high_resolution_clock::now();
    ecs.forEach<Position, Velocity, Health>([&](vecs::Ecs *ecs, vecs::Entity e, Position &p, Velocity &v, Health &h)
                                            {

                                               float f= *ecs->getResource<float>();

                                                p.x += v.x * h.value * f;
                                                p.y += v.y * h.value;
                                                p.z += v.z * h.value;

                                                sum + p.x;
                                            });
    end = std::chrono::high_resolution_clock::now();
    std::cout << "Three-component (Position + Velocity + Health) iteration: "
              << std::chrono::duration<double, std::micro>(end - start).count()
              << " μs\n";

    std::cout << sum << "\n";

    return 0;
}
