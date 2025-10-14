#pragma once
#include <vector>
#include <unordered_map>
#include <cinttypes>
#include <typeindex>
#include <memory>
#include <cassert>
#include <unordered_set>

namespace vecs
{
    using Entity = uint64_t;
    constexpr Entity NO_ENTITY = UINT64_MAX;

    class Ecs; // Forward Decl

    struct Schedule
    {

        explicit Schedule() : systems({}) {};
        ~Schedule() = default;

        std::unordered_set<void (*)(Ecs *)> systems;
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

    /// @brief Returns type per Index using recursive
    /// @tparam First
    /// @tparam ...Rest
    /// @tparam idx
    template <size_t idx, typename First, typename... Rest>
    struct TypeAt
    {
        using type = typename TypeAt<idx - 1, Rest...>::type;
    };

    // Used when Index is 0
    template <typename First, typename... Rest>
    struct TypeAt<0, First, Rest...>
    {
        using type = First;
    };

    class Ecs
    {
    public:
        Ecs() = default;
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

        template <typename T>
        void addComponent(Entity e, T component)
        {
            SparseSet<T> &set = getOrCreateSparseSet<T>();

            set.dense.push_back(component);
            set.dense_entities.push_back(e);

            uint64_t component_index = set.dense.size() - 1;

            if (e + 1 > set.sparse.size())
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

            static_assert(std::is_invocable_v<Func, Ecs *, Entity, Ts &...>,
                          "Lambda signature must start with (Ecs*, Entity, T1&, Ts&...)");

            size_t dense_sizes[] = {getSparseSet<Ts>()->dense.size()...};

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
            SparseSetBase *result = nullptr;

            ((count++ == smallest_index ? (iterateSparseSet<Ts, Ts...>(getSparseSet<Ts>(), func), true) : false) || ...);

           
        }

        template <typename T>
        T *getComponent(Entity e)
        {
            SparseSet<T> *set = getSparseSet<T>();

            if (set == nullptr || e + 1 > set->sparse.size() || set->sparse[e] == NO_ENTITY)
                return nullptr;

            return &set->dense[set->sparse[e]];
        }

        template <typename... Ts>
        bool hasComponents(Entity e)
        {
            return (... && (getSparseSet<Ts>() &&
                            e < getSparseSet<Ts>()->sparse.size() &&
                            getSparseSet<Ts>()->sparse[e] != NO_ENTITY));
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

            if (id + 1 > resources.size())
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

            if (id + 1 > resources.size())
            {
                return nullptr;
            }

            Resource<T> *resource = static_cast<Resource<T> *>(resources[id]);

            return &resource->data;
        }

        void addSystem(Schedule &schedule, void (*func_ptr)(Ecs *))
        {
            schedule.systems.insert(func_ptr);
        }

        void removeSystem(Schedule &schedule, void (*func_ptr)(Ecs *))
        {
            schedule.systems.erase(func_ptr);
        }

        void runSchedule(Schedule schedule)
        {
            for (auto &sys : schedule.systems)
            {
                sys(this);
            }
        }

    private:
        template <typename smallest_T, typename... Ts, typename Func>
        void iterateSparseSet(SparseSet<smallest_T> *smallest_set, Func &&func)
        {

            if (smallest_set == nullptr)
                return;

            for (size_t i = 0; i < smallest_set->dense.size(); i++)
            {
                Entity e = smallest_set->dense_entities[i];

                if constexpr (sizeof...(Ts) > 1) //Smallest set also in Ts
                {
                    if (hasComponents<Ts...>(e) == false)
                        continue;

                    func(this, e, *getComponent<Ts>(e)...);
                }
                else
                {
                    func(this, e, smallest_set->dense[i]);
                }
            }
        }

        template <typename T>
        uint64_t getComponentCount()
        {
            return getSparseSet<T>()->dense.size();
        }

        template <typename T>
        SparseSet<T> &getOrCreateSparseSet()
        {
            uint64_t type_id = getTypeId<T>();

            if (type_id + 1 > sets.size())
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
    };
}