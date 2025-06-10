#pragma once

#include <vector>
#include <algorithm>
#include <cassert>
#include <unordered_map>
#include <type_traits>
#include <functional>

#include "cppecs/sparse_set.hpp"

#define assertm(msg, expr) assert(((void)msg, (expr)))

namespace cppecs {

using ComponentID = uint32_t;
using Entity = uint32_t;

struct Resource{};
struct Component{};

template<typename Category>
class IndexGetter final {
public:
    template <typename T>
    static ComponentID Get() {
        static ComponentID id = ++m_curIdx;
        return id;
    }
private:
    inline static ComponentID m_curIdx = 0;
};

template<typename T, typename = std::enable_if<std::is_integral_v<T>>>
struct IDGenerator final {
public:
    static T Gen() {
        return ++m_curId;
    }
private:
    inline static T m_curId = {};
};

using EntityGenerator = IDGenerator<Entity>;

class Commands;
class Queryer;

using FStartUpSystem = void(*)(Commands&, Queryer&);
using FSystem = void(*)(Commands&, Queryer&);

struct World final {
public:
    friend class Commands;
    friend class Queryer;

    using ComponentContainer = std::unordered_map<ComponentID, void*>;

public:
    World(World&) = delete;
    World& operator=(World&) = delete;
    World() = default;
    World& operator=(World&&) = default;
    ~World() { Shutdown(); }

    World& AddStartUpSystem(FStartUpSystem _system) {
        m_startUpSystems.emplace_back(_system);
        return *this;
    }
    World& AddSystem(FSystem _system) {
        m_systems.emplace_back(_system);
        return *this;
    }
    World& RemoveSystem(FSystem _system) {
        auto it = std::find(m_systems.begin(), m_systems.end(), _system);
        if (it == m_systems.end()) {
            assertm("system not registe", false);
        }
        std::swap(*it, m_systems.back());
        m_systems.pop_back();
        return *this;
    }

    void StartUp();

    void Update();

    void Shutdown() {
        m_entities.clear();

        m_componentMap.clear();

        m_resources.clear();

        m_startUpSystems.clear();
        m_systems.clear();
    }

private:
    std::vector<FStartUpSystem> m_startUpSystems;
    std::vector<FSystem> m_systems;

private:
    //Entity and Component
    struct Pool final {
        std::vector<void*> m_instance;
        std::vector<void*> m_cache;

        using CreateFunc = void*(*)(void);
        using DestroyFunc = void(*)(void*);

        CreateFunc m_create;
        DestroyFunc m_destroy;

        Pool() = delete;
        Pool(const Pool&) = delete;
        Pool& operator=(const Pool&) = delete;
        Pool(Pool&&) = default;
        Pool& operator=(Pool&&) = default;

        Pool(CreateFunc create, DestroyFunc destroy) 
            : m_create(create), m_destroy(destroy) { }
        ~Pool() {
            DestroyData();
            m_create = nullptr;
            m_destroy = nullptr;
        }

        void* Create() {
            if (!m_cache.empty()) {
                m_instance.push_back(m_cache.back());
                m_cache.pop_back();
            } else {
                m_instance.push_back(m_create());
            }
            return m_instance.back();
        }

        void Destroy(void *elem) {
            auto it = std::find(m_instance.begin(), m_instance.end(), elem);
            if (it != m_instance.end()) {
                m_cache.push_back(*it);
                std::swap(*it, m_instance.back());
                m_instance.pop_back();
            } else {
                assertm("your element not in pool", false);
            }
        }

        void DestroyData() {
            for (auto dat : m_instance) {
                m_destroy(dat);
            }
            m_instance.clear();

            for (auto dat : m_cache) {
                m_destroy(dat);
            }
            m_cache.clear();
        }
    };

    struct ComponentInfo{
        Pool m_pool;
        basic_sparse_set<Entity, 32> m_sparseSet;

        ComponentInfo() = delete;
        ComponentInfo(const ComponentInfo&) = delete;
        ComponentInfo& operator=(const ComponentInfo&) = delete;
        ComponentInfo(ComponentInfo&&) = default;
        ComponentInfo& operator=(ComponentInfo&&) = default;
        ~ComponentInfo() {
            m_pool.~Pool();
            m_sparseSet.clear();
        };

        ComponentInfo(Pool::CreateFunc create, Pool::DestroyFunc destroy) : m_pool(create, destroy) { }
    };

    using ComponentMap = std::unordered_map<ComponentID, ComponentInfo>;
    ComponentMap m_componentMap;

    std::unordered_map<Entity, ComponentContainer> m_entities;

    //Resource
    struct ResourceInfo {
        void* resource{nullptr};

        using DestroyFunc = void(*)(void*);

        DestroyFunc m_destroy;

        ResourceInfo() = delete;
        ResourceInfo(const ResourceInfo&) = delete;
        ResourceInfo& operator=(const ResourceInfo&) = delete;
        ResourceInfo(ResourceInfo&&) = default;
        ResourceInfo& operator=(ResourceInfo&&) = default;

        ResourceInfo(DestroyFunc destroy) :m_destroy(destroy) {}
        ~ResourceInfo() {
            if (resource) {
                m_destroy(resource);
            }
            resource = nullptr;
            m_destroy = nullptr;
        };
    };

    std::unordered_map<ComponentID, ResourceInfo> m_resources;

private:
    std::shared_ptr<Commands> createCommands();
    void destoryCommands() {
        m_commandsCache.insert(m_commandsCache.end(), m_commands.begin(), m_commands.end());
        m_commands.clear();
    }
    std::vector<std::shared_ptr<Commands>> m_commands;
    std::vector<std::shared_ptr<Commands>> m_commandsCache;
};

class Commands final {
public:
    friend class World;

private:
    Commands(World& world) : m_world(world) {}
public:
    Commands(const Commands&) = default;
    Commands& operator=(const Commands&) = default;
    Commands(Commands&&) = default;
    Commands& operator=(Commands&&) = default;
    
public:
    template<typename ...ComponentTypes>
    Entity SpawnAndReturn(ComponentTypes&& ...components) {
        Entity entity = EntityGenerator::Gen();
        EntitySpawnInfo spawnInfo(entity); 
        doSpawn(spawnInfo, std::forward<ComponentTypes>(components)...);
        m_spawnEntities.push_back(std::move(spawnInfo));
        return entity;
    }

    template<typename ...ComponentTypes>
    Commands& Spawn(ComponentTypes&& ...components) {
        SpawnAndReturn(std::forward<ComponentTypes>(components)...);
        return *this;
    }

    Commands& Destroy(Entity entity) {
        m_destroyEntities.push_back(entity);
        return *this;
    }

    template<typename ComponentType>
    ComponentType& SetResource(ComponentType&& component) {
        ComponentID componentId = IndexGetter<Resource>::Get<ComponentType>();
        auto it = m_world.m_resources.find(componentId);
        if (it == m_world.m_resources.end()) {
            auto emret = m_world.m_resources.try_emplace(
                    componentId, 
                    World::ResourceInfo(
                        [](void* data) {
                            delete (ComponentType*)data;
                        }
                    ));
            it = emret.first;
        }
        World::ResourceInfo& resourceInfo = it->second;
        assertm("resource already set", resourceInfo.resource == nullptr);

        void* compData = new ComponentType(std::forward<ComponentType>(component));
        ResourceCreateInfo info(componentId, compData);
        m_createResources.push_back(std::move(info));
        return *((ComponentType*)compData);
    }

    template<typename ComponentType>
    Commands& RemoveResource() {
        ComponentID componentId = IndexGetter<Resource>::Get<ComponentType>();
        m_destoryResources.push_back(componentId);
        return *this;
    }

    void Execute() {
        for (auto& entity : m_destroyEntities) {
            destroyEntity(entity);
        }
        m_destroyEntities.clear();

        for (auto& componentId : m_destoryResources) {
            destroyResource(componentId);
        }
        m_destoryResources.clear();

        for (auto& entitySpawnInfo : m_spawnEntities) {
            doSpawnWithoutType(entitySpawnInfo);
        }
        m_spawnEntities.clear();

        for (auto& resourceCreateInfo : m_createResources) {
            createResourceWithoutType(resourceCreateInfo);
        }
        m_createResources.clear();
    }

private:
    struct ComponentSpawnInfo {
        ComponentID m_componentId {0}; 
        void* m_componentData {nullptr};
        ComponentSpawnInfo(ComponentID componentId, void* rawData)
            : m_componentId(componentId), m_componentData(rawData) {}

        ComponentSpawnInfo() = delete;
        ComponentSpawnInfo(const ComponentSpawnInfo&) = delete;
        ComponentSpawnInfo& operator=(const ComponentSpawnInfo&) = delete;
        ComponentSpawnInfo(ComponentSpawnInfo&&) = default;
        ComponentSpawnInfo& operator=(ComponentSpawnInfo&&) = default;
        ~ComponentSpawnInfo() { 
            m_componentId = 0; 
            m_componentData = nullptr;
        }
    };
    struct EntitySpawnInfo {
        Entity m_entity;
        std::vector<ComponentSpawnInfo> m_components;

        EntitySpawnInfo(Entity entity) : m_entity(entity) {}

        EntitySpawnInfo() = delete;
        EntitySpawnInfo(const EntitySpawnInfo&) = delete;
        EntitySpawnInfo& operator=(const EntitySpawnInfo&) = delete;
        EntitySpawnInfo(EntitySpawnInfo&&) = default;
        EntitySpawnInfo& operator=(EntitySpawnInfo&&) = default;
    };

    template<typename ComponentType, typename ...Remains>
    void doSpawn(EntitySpawnInfo &spawnInfo, ComponentType&& component, Remains&&... remains) {
        ComponentID componentId = IndexGetter<Component>::Get<ComponentType>();
        auto it = m_world.m_componentMap.find(componentId);
        if (it == m_world.m_componentMap.end()) {
            auto emret = m_world.m_componentMap.try_emplace(
                componentId, 
                World::ComponentInfo(
                    []() -> void* {
                        return new ComponentType;
                    }, 
                    [](void* elemData) {
                        delete (ComponentType*)elemData;
                    }
                ));
            it = emret.first;
        }

        World::ComponentInfo& componentInfo = it->second;
        void* elemRawData = componentInfo.m_pool.Create();
        *((ComponentType*)elemRawData) = std::forward<ComponentType>(component);

        spawnInfo.m_components.emplace_back(componentId, elemRawData);

        if constexpr(sizeof...(remains) != 0) {
            doSpawn<Remains...>(spawnInfo, std::forward<Remains>(remains)...);
        }
    }

    void doSpawnWithoutType(EntitySpawnInfo &spawnInfo) {
        Entity entity = spawnInfo.m_entity;
        for (auto& componentSpawnInfo : spawnInfo.m_components) {
            ComponentID componentId = componentSpawnInfo.m_componentId;
            auto it = m_world.m_componentMap.find(componentId);
            if (it == m_world.m_componentMap.end()) {
                assertm("componentInfo not create", false);
            }
            World::ComponentInfo& componentInfo = it->second;

            componentInfo.m_sparseSet.insert(entity);

            m_world.m_entities[entity][componentId] = componentSpawnInfo.m_componentData;
        }
    }

    struct ResourceCreateInfo{
        ComponentID m_componentId {0};
        void* m_componentData {nullptr};

        ResourceCreateInfo(ComponentID componentId, void* componentData)
            : m_componentId(componentId), m_componentData(componentData) {}
        ResourceCreateInfo() = delete;
        ResourceCreateInfo(const ResourceCreateInfo&) = delete;
        ResourceCreateInfo& operator=(const ResourceCreateInfo&) = delete;
        ResourceCreateInfo(ResourceCreateInfo&&) = default;
        ResourceCreateInfo& operator=(ResourceCreateInfo&&) = default;
        ~ResourceCreateInfo() {
            m_componentId = 0;
            m_componentData = nullptr;
        }
    };

    void createResourceWithoutType(ResourceCreateInfo& info) {
        ComponentID componentId = info.m_componentId;
        auto it = m_world.m_resources.find(componentId);
        if (it == m_world.m_resources.end()) {
            assertm("resourceinfo not create", false);
        }
        World::ResourceInfo& resourceInfo = it->second;
        assertm("resource already set", resourceInfo.resource == nullptr);
        resourceInfo.resource = info.m_componentData;
    }

    void destroyEntity(Entity entity) {
        auto it = m_world.m_entities.find(entity);
        if (it != m_world.m_entities.end()) {
            for (auto [componentId, elemData]: it->second) {
                auto it = m_world.m_componentMap.find(componentId);
                if (it != m_world.m_componentMap.end()) {
                    auto& componentInfo = it->second;
                    componentInfo.m_pool.Destroy(elemData);
                    componentInfo.m_sparseSet.remove(entity);
                }
            }
            m_world.m_entities.erase(it);
        }
    }

    void destroyResource(ComponentID componentId) {
        auto it = m_world.m_resources.find(componentId);
        if (it != m_world.m_resources.end()) {
            World::ResourceInfo& resourceInfo = it->second;
            if (resourceInfo.resource) {
                resourceInfo.m_destroy(resourceInfo.resource);
                resourceInfo.resource = nullptr;
            }
        }
    }

private:
    World& m_world;

    std::vector<Entity> m_destroyEntities; //待销毁的实体
    std::vector<ComponentID> m_destoryResources; //待销毁的资源
    std::vector<EntitySpawnInfo> m_spawnEntities; //待创建的实体
    std::vector<ResourceCreateInfo> m_createResources; //待创建的实体
};

class Queryer final {
public:
    Queryer(World& world) : m_world(world) {}

    template<typename ...ComponentTypes>
    std::vector<Entity> Query() const {
        return doQuery<ComponentTypes...>();
    }

    //是否存在实体
    bool Exist(Entity entity) const { 
        return m_world.m_entities.find(entity) == m_world.m_entities.end();
    }

    template<typename ComponentType>
    bool Has(Entity entity) const { 
        auto eit = m_world.m_entities.find(entity);
        if (eit == m_world.m_entities.end()) {
            return false;
        }
        auto& componentContainer = eit->second;
        ComponentID componentId = IndexGetter<Component>::Get<ComponentType>();
        auto cit = componentContainer.find(componentId);
        if (cit == componentContainer.end()) {
            return false;
        }
        return true;
    }

    //获取实体组件
    template<typename ComponentType>
    ComponentType& Get(Entity entity) const { 
        auto eit = m_world.m_entities.find(entity);
        if (eit == m_world.m_entities.end()) {
            assertm("entity not found", false);
        }
        auto& componentContainer = eit->second;
        ComponentID componentId = IndexGetter<Component>::Get<ComponentType>();
        auto cit = componentContainer.find(componentId);
        if (cit == componentContainer.end()) {
            assertm("component not create", false);
        }
        return *((ComponentType*)cit->second);
    }

    template<typename ComponentType>
    bool HasResource() const { 
        ComponentID componentId = IndexGetter<Resource>::Get<ComponentType>();
        auto rit = m_world.m_resources.find(componentId);
        if (rit == m_world.m_resources.end()) {
            return false;
        }
        World::ResourceInfo& resourceInfo = rit->second;
        if (!resourceInfo.resource) {
            return false;
        }
        return resourceInfo.resource != nullptr;
    }

    //获取资源
    template<typename ComponentType>
    ComponentType& GetResource() const { 
        ComponentID componentId = IndexGetter<Resource>::Get<ComponentType>();
        auto rit = m_world.m_resources.find(componentId);
        if (rit == m_world.m_resources.end()) {
            assertm("component not create", false);
        }
        World::ResourceInfo& resourceInfo = rit->second;
        if (!resourceInfo.resource) {
            assertm("component not create", false);
        }
        return *(ComponentType*)resourceInfo.resource;
    }

private:
    template<typename ComponentType, typename ...Remains>
    std::vector<Entity> doQuery() const {
        std::vector<Entity> entities;
        ComponentID componentId = IndexGetter<Component>::Get<ComponentType>();
        auto cit = m_world.m_componentMap.find(componentId);
        if (cit == m_world.m_componentMap.end()) {
            return entities;
        }

        World::ComponentInfo& componentInfo = cit->second;
        auto eit = componentInfo.m_sparseSet.cbegin();
        for (; eit != componentInfo.m_sparseSet.end(); eit++) {
            Entity entity = *eit;
            if constexpr(sizeof...(Remains) == 0) {
                entities.push_back(entity);
            } else {
                if (doQueryR<Remains...>(entity)) {
                    entities.push_back(entity);
                }
            }
        }
        return entities;
    }

    template<typename ComponentType, typename ...Remains>
    bool doQueryR(Entity entity) const {
        ComponentID componentId = IndexGetter<Component>::Get<ComponentType>();
        auto cit = m_world.m_componentMap.find(componentId);
        if (cit == m_world.m_componentMap.end()) {
            return false;
        }

        World::ComponentInfo& componentInfo = cit->second;
        if (!componentInfo.m_sparseSet.contain(entity)) {
            return false;
        }

        if constexpr(sizeof...(Remains) != 0) {
            return doQueryR<Remains...>(entity);
        } else {
            return true;
        }
    }

private:
    World& m_world;
};

} // namespace cppecs

