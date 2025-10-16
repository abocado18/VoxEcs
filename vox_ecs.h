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

#include <cassert>

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

    template <typename T>
    struct is_write : std::false_type
    {
    };

    template <typename T>
    struct is_write<Write<T>> : std::true_type
    {
    };

    template <typename T>
    struct ResMut
    {
        using type = T;
    };

    template <typename T>
    struct Res
    {
        using type = T;
    };

    template <typename T>
    struct unwrapResource
    {
        using type = T;
    };

    template <typename T>
    struct unwrapResource<ResMut<T>>
    {
        using type = T;
    };

    template <typename T>
    struct unwrapResource<Res<T>>
    {
        using type = T;
    };

    template <typename T>
    struct resourceRef
    {
        using type = typename unwrapResource<T>::type *;
    };

    template <typename T>
    struct resourceRef<Res<T>>
    {
        using type = const T *;
    };

    template <typename T>
    struct resourceRef<ResMut<T>>
    {
        using type = T *;
    };

    template <typename T>
    using resource_r = typename resourceRef<T>::type;

    template <typename T>
    struct isMutableResource : std::false_type
    {
    };

    template <typename T>
    struct isMutableResource<ResMut<T>> : std::true_type
    {
    };

    template <typename T>
    struct isConstResource : std::false_type
    {
    };

    template <typename T>
    struct isConstResource<Res<T>> : std::true_type
    {
    };

    template <typename...>
    struct Filter;

    template <>
    struct Filter<>
    {
        using type = std::tuple<>;
    };

    /// @brief Uses Recursion to find Components in Read/Write
    /// @tparam T
    /// @tparam ...Rest
    template <typename T, typename... Rest>
    struct Filter<T, Rest...>
    {
        using type = std::conditional_t<
            is_read_or_write<T>::value,
            decltype(std::tuple_cat(std::tuple<T>{}, typename Filter<Rest...>::type{})),
            typename Filter<Rest...>::type>;
    };

    struct SystemWrapper
    {
        SystemWrapper() : callback({}), c_read(0), c_write(0), r_read(0), r_write(0) {};

        SystemWrapper(std::function<void(Ecs *)> callback, bit::Bitset c_read, bit::Bitset c_write, bit::Bitset r_read, bit::Bitset r_write)
            : callback(callback),
              c_read(c_read),
              c_write(c_write),
              r_read(r_read),
              r_write(r_write)
        {
        }

        std::function<void(Ecs *)> callback;

        bit::Bitset c_read;
        bit::Bitset c_write;

        bit::Bitset r_read;
        bit::Bitset r_write;
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
            auto getComponent(Entity e)
            {
                static_assert((std::is_same_v<T, Ts> || ...),
                              "Component T is not in this system's query!");

                static_assert(is_read_or_write<T>::value, "Must be a component in Read / Write Wrapper for Multithreading");

                using Comp = component_t<T>;
                Comp *ptr = ecs->getComponent<Comp>(e);

                if constexpr (is_read<T>::value)
                    return static_cast<const Comp &>(*ptr); // return reference to const
                else
                    return *ptr; // return reference for write
            }

        private:
            Ecs *ecs;
        };

        // Static Systemhelper to avoid dependent template and get the correct dependent
        template <typename T, typename... Ts>
        static inline decltype(auto) get(SystemView<Ts...> &view, Entity e)
        {

            using Wrapper = std::conditional_t<
                (std::is_same_v<Read<T>, Ts> || ...),
                Read<T>,
                std::conditional_t<
                    (std::is_same_v<Write<T>, Ts> || ...),
                    Write<T>,
                    void // static_assert will catch this
                    >>;

            static_assert(!std::is_void_v<Wrapper>, "Type T not found in SystemView or a Resource");

            return view.template getComponent<Wrapper>(e);
        }

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

            static_assert(((is_read_or_write<Ts>::value || isConstResource<Ts>::value || isMutableResource<Ts>::value) && ...),
                          "All components/resources must be wrapped in Read<T> ,Write<T>, Res<T> or ResMut<T>!");

            uint64_t dense_size_counter = 0;

            dense_size_counter = ((is_read_or_write<Ts>::value ? dense_size_counter += 1 : dense_size_counter), ...);
            size_t dense_sizes[] = {(is_read_or_write<Ts>::value ? getSparseSet<typename unwrap_component<Ts>::type>()->dense.size() : 0)...};

            size_t smallest_index = 0;
            size_t smallest_size = dense_sizes[0];

            for (size_t i = 0; i < dense_size_counter; i++)
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

            static_assert(((is_read_or_write<Ts>::value || isConstResource<Ts>::value || isMutableResource<Ts>::value) && ...),
                          "All components must be wrapped in Read<T> or Write<T>!");

            // Unique Lookup Tables for each combination, gets only created once on first call
            static const auto c_lookup_write_table = [&]()
            {
                bit::Bitset write(sizeof...(Ts));

                ((is_write<Ts>::value ? (write.setBit(getTypeId<typename unwrap_component<Ts>::type>(), true), true) : false), ...);

                return write;
            }();

            static const auto c_lookup_read_table = [&]()
            {
                bit::Bitset read(sizeof...(Ts));

                ((is_read<Ts>::value ? (read.setBit(getTypeId<typename unwrap_component<Ts>::type>(), true), true) : false), ...);

                return read;
            }();

            static const auto r_lookup_write_table = [&]()
            {
                bit::Bitset write(sizeof...(Ts));

                ((isMutableResource<Ts>::value ? (write.setBit(getResourceId<typename unwrapResource<Ts>::type>(), true), true) : false), ...);

                return write;
            }();

            static const auto r_lookup_read_table = [&]()
            {
                bit::Bitset read(sizeof...(Ts));

                ((isConstResource<Ts>::value ? (read.setBit(getResourceId<typename unwrapResource<Ts>::type>(), true), true) : false), ...);

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

            SystemWrapper system(wrapper, c_lookup_read_table, c_lookup_write_table, r_lookup_read_table, r_lookup_write_table);

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
                bool c_conflict = ((a.c_write & b.c_write).any() || (a.c_write & b.c_read).any() || (b.c_write & a.c_read).any());
                bool r_conflict = ((a.r_write & b.r_write).any() || (a.r_write & b.r_read).any() || (b.r_write & a.r_read).any());

                return (c_conflict || r_conflict);
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

        template <typename T>
        resource_r<T> getResourceForLoop()
        {
            return getResource<typename unwrapResource<T>::type>();
        }

        template <typename T>
        lambda_t<T> getComponentForLoop(Entity e)
        {
            return *getComponent<component_t<T>>(e);

            // Lambda ensures read write access
        }

        template <typename... Ts>
        bool hasComponents(Entity e)
        {

            return (... && (checkIfEntityHasComponent<Ts>(e)));
        }

        template <typename T>
        bool checkIfEntityHasComponent(Entity e)
        {
            if constexpr (isConstResource<T>::value || isMutableResource<T>::value)
            {
                // Resources are global and return always true
                return true;
            }
            else
            {
                return getSparseSet<component_t<T>>() &&
                       e < getSparseSet<component_t<T>>()->sparse.size() &&
                       getSparseSet<component_t<T>>()->sparse[e] != NO_ENTITY;
            }
        }

        template <typename T>
        auto getComponentOrResourceForLoop(Entity e) -> decltype(auto)
        {
            if constexpr (is_read_or_write<T>::value)
                return getComponentForLoop<T>(e);

            else
                return getResourceForLoop<T>();
        }

        template <typename smallest_T, typename... Ts, typename Func>
        void iterateSparseSet(SparseSet<smallest_T> *smallest_set, Func &&func)
        {

            static_assert(is_read_or_write<smallest_T>::value == false, "No Wrapper allowed");

            if (smallest_set == nullptr)
                return;

            for (size_t i = 0; i < smallest_set->dense.size(); i++)
            {
                Entity e = smallest_set->dense_entities[i];

                if (hasComponents<Ts...>(e) == false)
                    continue;

                SystemView<Ts...> view(this);

                func(view, e, getComponentOrResourceForLoop<Ts>(e)...);
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