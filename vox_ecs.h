#pragma once
#include <vector>
#include <unordered_map>
#include <cinttypes>
#include <typeindex>
#include <memory>
#include <cassert>

namespace vox_ecs
{
    using Entity = uint64_t;
    constexpr Entity NO_ENTITY = UINT64_MAX;

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

        template <typename T1, typename... Ts, typename Func>
        void forEach(Func &&func)
        {
            SparseSet<T1> *sparse_set_t1 = getSparseSet<T1>();

            if (sparse_set_t1 == nullptr)
                return;

            // Loop over all T1s
            for (size_t i = 0; i < sparse_set_t1->dense.size(); i++)
            {

                Entity e = sparse_set_t1->dense_entities[i];

                bool valid = true;

                if constexpr (sizeof...(Ts) > 0)
                {
                    valid = hasComponents<Ts...>(e);
                }

                if (valid)
                {

                    auto &t1 = sparse_set_t1->dense[i];

                    if constexpr (sizeof...(Ts) > 0)
                    {
                        func(this, e, t1, *getComponent<Ts>(e)...);
                    }
                    else
                    {
                        func(this, e, t1);
                    }
                }
            }
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

    private:
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