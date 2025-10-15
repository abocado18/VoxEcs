#pragma once
#include <vector>
#include <unordered_map>
#include <cinttypes>
#include <typeindex>
#include <memory>
#include <cassert>
#include <unordered_set>
#include <type_traits>
#include <functional>
#include <thread>
#include "dynamic_bitset.h"
#include "thread_pool.h"

namespace vecs
{
    using Entity = uint64_t;
    constexpr Entity NO_ENTITY = UINT64_MAX;

    class Ecs; // Forward Decl

    struct Schedule
    {

        explicit Schedule() {};
        ~Schedule() = default;

        std::unordered_set<uint64_t> systems;
    };

    template <typename T>
    struct Write
    {
        using type = T;
    };

    template <typename T>
    struct Read
    {
        using type = T;
    };

    template <typename T>
    struct unwrap_component
    {
        using type = T;
    };

    template <typename T>
    struct unwrap_component<Read<T>>
    {
        using type = T;
    };

    template <typename T>
    struct unwrap_component<Write<T>>
    {
        using type = T;
    };

    template <typename T>
    using component_t = typename unwrap_component<T>::type;

    template <typename T>
    struct lambda_type
    {
        using type = typename unwrap_component<T>::type &; // default: writable
    };

    template <typename T>
    struct lambda_type<Read<T>>
    {
        using type = const T &; // read-only
    };

    template <typename T>
    struct lambda_type<Write<T>>
    {
        using type = T &; // writable
    };

    template <typename T>
    using lambda_t = typename lambda_type<T>::type;

    template <typename T>
    struct is_read_or_write : std::false_type
    {
    };

    template <typename T>
    struct is_read_or_write<Read<T>> : std::true_type
    {
    };

    template <typename T>
    struct is_read_or_write<Write<T>> : std::true_type
    {
    };

    template <typename T>
    struct is_read : std::false_type
    {
    };

    template <typename T>
    struct is_read<Read<T>> : std::true_type
    {
    };

    template <typename T>
    struct is_read<Write<T>> : std::false_type
    {
    };

    struct SystemWrapper
    {
        SystemWrapper() : callback({}), read(0), write(0) {};

        SystemWrapper(std::function<void(Ecs *)> callback, bit::Bitset read, bit::Bitset write)
            : callback(callback),
              read(read),
              write(write)
        {
        }

        std::function<void(Ecs *)> callback;

        bit::Bitset read;
        bit::Bitset write;
    };

    struct SparseSetBase
    {
        virtual ~SparseSetBase() = default;
    };

    template <typename T>
    struct SparseSet : SparseSetBase
    {

        std::vector<T> dense;
        std::vector<Entity> dense_entities;
        std::vector<uint64_t> sparse;
    };

    struct ResourceBase
    {
        virtual ~ResourceBase() = default;
    };

    template <typename T>
    struct Resource : ResourceBase
    {
        T data;
    };

    class Ecs
    {
    public:
        Ecs() : pool(thread_pool::ThreadPool()) {

                };

        ~Ecs()
        {

            for (auto &set : sets)
            {
                delete set;
            }

            for (auto &resource : resources)
            {
                delete resource;
            }
        }

        template <typename... Ts>
        class SystemView
        {
        public:
            SystemView(Ecs *ecs) : ecs(ecs) {};

            template <typename T>
            auto get(Entity e)
            {
                static_assert((std::is_same_v<T, component_t<Ts>> || ...),
                              "Component T is not in this system's query!");

                static_assert((is_read_or_write<T>::value) == true);

                using Comp = component_t<T>;

                if constexpr (is_read<T>::value)
                    return static_cast<const Comp *>(ecs->getComponent<Comp>(e));
                else
                    return ecs->getComponent<Comp>(e);
            }

        private:
            Ecs *ecs;
        };

        template <typename T>
        void addComponent(Entity e, T component)
        {

            if (hasComponents<T>(e))
                return;

            static_assert(!is_read_or_write<T>());

            SparseSet<T> &set = getOrCreateSparseSet<T>();

            set.dense.push_back(component);
            set.dense_entities.push_back(e);

            uint64_t component_index = set.dense.size() - 1;

            if (e >= set.sparse.size())
            {
                set.sparse.resize(e + 1, NO_ENTITY);
            }

            set.sparse[e] = component_index;
        }

        template <typename T>
        void removeComponent(Entity e)
        {

            SparseSet<T> *set = getSparseSet<T>();

            if (set == nullptr)
                return;

            uint64_t component_index = set->sparse[e];

            if (component_index == NO_ENTITY)
                return;

            Entity last_entity = set->dense_entities.back();

            set->dense[component_index] = set->dense.back();
            set->dense_entities[component_index] = last_entity;

            set->sparse[last_entity] = component_index;

            set->dense.pop_back();
            set->dense_entities.pop_back();
            set->sparse[e] = NO_ENTITY;
        }

        template <typename... Ts, typename Func>
        void forEach(Func &&func)
        {

            static_assert((is_read_or_write<Ts>::value && ...),
                          "All components must be wrapped in Read<T> or Write<T>!");

            using ComponentTypes = std::tuple<typename unwrap_component<Ts>::type...>;

            size_t dense_sizes[] = {getSparseSet<typename unwrap_component<Ts>::type>()->dense.size()...};

            size_t smallest_index = 0;
            size_t smallest_size = dense_sizes[0];

            for (size_t i = 0; i < sizeof...(Ts); i++)
            {
                if (dense_sizes[i] < smallest_size)
                {
                    smallest_index = i;
                    smallest_size = dense_sizes[i];
                }
            }

            size_t count = 0;

            ((count++ == smallest_index
                  ? (iterateSparseSet<typename unwrap_component<Ts>::type, Ts...>(
                         getSparseSet<typename unwrap_component<Ts>::type>(), func),
                     true)
                  : false) ||
             ...);
        }

        template <typename T>
        T *getComponent(Entity e)
        {
            SparseSet<T> *set = getSparseSet<T>();

            if (set == nullptr || e >= set->sparse.size() || set->sparse[e] == NO_ENTITY)
                return nullptr;

            return &set->dense[set->sparse[e]];
        }

        Entity createEntity()
        {
            static Entity e = 0;
            return e++;
        }

        template <typename T>
        void insertResource(T data)
        {
            uint64_t id = getResourceId<T>();

            if (id >= resources.size())
            {
                resources.resize(id + 1, nullptr);
                resources[id] = new Resource<T>();
            }

            Resource<T> &ref = *static_cast<Resource<T> *>(resources[id]);

            ref.data = data;
        }

        template <typename T>
        T *getResource()
        {
            uint64_t id = getResourceId<T>();

            if (id >= resources.size())
            {
                return nullptr;
            }

            Resource<T> *resource = static_cast<Resource<T> *>(resources[id]);

            return &resource->data;
        }

        template <typename... Ts, typename Func>
        uint64_t addSystem(Schedule &schedule, Func &&func)
        {

            static_assert(std::is_member_function_pointer_v<decltype(&std::decay_t<Func>::operator())>,
                          "func must define operator(), i.e., must be a lambda or functor");

            static_assert((is_read_or_write<Ts>::value && ...),
                          "All components must be wrapped in Read<T> or Write<T>!");

            // Unique Lookup Tables for each combination, gets only created once on first call
            static const auto lookup_write_table = [&]()
            {
                bit::Bitset write(sizeof...(Ts));

                ((!is_read<Ts>::value ? (write.setBit(getTypeId<Ts>(), true), true) : false), ...);

                return write;
            }();

            static const auto lookup_read_table = [&]()
            {
                bit::Bitset read(sizeof...(Ts));

                ((is_read<Ts>::value ? (read.setBit(getTypeId<Ts>(), true), true) : false), ...);

                return read;
            }();

            std::function<void(Ecs *)> wrapper = [func](Ecs *ecs)
            {
                ecs->forEach<Ts...>(func);
            };

            uint64_t system_id = getNextSystemId();

            if (system_id >= systems.size())
            {
                systems.resize(system_id + 1);
            }

            SystemWrapper system(wrapper, lookup_read_table, lookup_write_table);

            systems[system_id] = system;

            schedule.systems.insert(system_id);

            return system_id;
        }

        void removeSystem(Schedule &schedule, uint64_t system_id)
        {
            schedule.systems.erase(system_id);
        }

        void runSchedule(Schedule schedule)
        {

            std::vector<uint64_t> system_ids(schedule.systems.begin(), schedule.systems.end());

            for (uint64_t system_id : schedule.systems)
            {
                SystemWrapper &current = systems[system_id];

                current.callback(this);
            }
        }

        void runScheduleParallel(Schedule schedule)
        {

            auto checkConflict = [&schedule](const SystemWrapper &a, const SystemWrapper &b)
            {
                return ((a.write & b.write).any() || (a.write & b.read).any() || (b.write & a.read).any());
            };

            std::vector<uint64_t> system_ids(schedule.systems.begin(), schedule.systems.end());

            std::vector<std::vector<SystemWrapper *>> batches = {};

            for (uint64_t system_id : system_ids)
            {
                SystemWrapper &current = systems[system_id];
                bool added_to_batch = false;

                for (auto &batch : batches)
                {
                    bool conflict = false;

                    for (SystemWrapper *existing : batch)
                    {
                        if (checkConflict(current, *existing))
                        {
                            conflict = true;
                            break;
                        }
                    }

                    if (!conflict)
                    {
                        batch.push_back(&current);
                        added_to_batch = true;
                        break;
                    }
                }

                // Need more place, no place in other batches
                if (!added_to_batch)
                {
                    batches.push_back({&current});
                }
            }

            for (auto &batch : batches)
            {

                std::atomic<size_t> jobs_remaining = batch.size();
                std::condition_variable cv;
                std::mutex cvMutex;

                for (auto *sys : batch)
                {

                    pool.enqueue([this, sys, &jobs_remaining, &cv]()
                                 {
                                     sys->callback(this);

                                     if (--jobs_remaining == 0)
                                         cv.notify_one(); // wake up main thread when all jobs done
                                 });
                }

                {
                    std::unique_lock<std::mutex> lock(cvMutex);
                    cv.wait(lock, [&]()
                            { return jobs_remaining == 0; });
                }
            }
        }

    private:
        thread_pool::ThreadPool pool;

        template <typename... Ts>
        bool hasComponents(Entity e)
        {
            return (... && (getSparseSet<component_t<Ts>>() &&
                            e < getSparseSet<component_t<Ts>>()->sparse.size() &&
                            getSparseSet<component_t<Ts>>()->sparse[e] != NO_ENTITY));
        }

        template <typename smallest_T, typename... Ts, typename Func>
        void iterateSparseSet(SparseSet<smallest_T> *smallest_set, Func &&func)
        {

            if (smallest_set == nullptr)
                return;

            for (size_t i = 0; i < smallest_set->dense.size(); i++)
            {
                Entity e = smallest_set->dense_entities[i];

                if constexpr (sizeof...(Ts) > 1) // Smallest set also in Ts
                {
                    if (hasComponents<Ts...>(e) == false)
                        continue;

                    SystemView<Ts...> view(this);

                    func(view, e, *getComponent<component_t<Ts>>(e)...);
                }
                else
                {

                    SystemView<smallest_T> view(this);

                    func(view, e, *getComponent<component_t<smallest_T>>(e));
                }
            }
        }

        template <typename T>
        SparseSet<T> &getOrCreateSparseSet()
        {
            uint64_t type_id = getTypeId<T>();

            if (type_id >= sets.size())
                sets.resize(type_id + 1, nullptr);

            if (sets[type_id] == nullptr)
            {
                sets[type_id] = new SparseSet<T>();
            }

            return *static_cast<SparseSet<T> *>(sets[type_id]);
        }

        template <typename T>
        SparseSet<T> *getSparseSet()
        {
            uint64_t type_id = getTypeId<T>();

            if (type_id >= sets.size())
                return nullptr;

            if (sets[type_id] == nullptr)
                return nullptr;

            return static_cast<SparseSet<T> *>(sets[type_id]);
        }

        template <typename T>
        uint64_t getTypeId()
        {
            static uint64_t id = next_id++; // Static = Unique per Component Type
            return id;
        }

        static inline uint64_t next_id = 0;

        std::vector<SparseSetBase *> sets = {};

        template <typename T>
        uint64_t getResourceId()
        {
            static uint64_t id = next_resource_id++; // Static = Unique per Resource Type
            return id;
        }

        static inline uint64_t next_resource_id = 0;

        std::vector<ResourceBase *> resources = {};

        inline uint64_t getNextSystemId()
        {
            static uint64_t next_system_id = 0;
            uint64_t id = next_system_id++;
            return id;
        };

        std::vector<SystemWrapper> systems;
    };
}