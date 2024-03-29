#pragma once

#include <unordered_set>
#include "entity.hpp"

namespace our
{

    // This class holds a set of entities
    class World
    {
        std::unordered_set<Entity *> entities;         // These are the entities held by this world
        std::unordered_set<Entity *> markedForRemoval; // These are the entities that are awaiting to be deleted
                                                       // when deleteMarkedEntities is called
    public:
        World() = default;

        // This will deserialize a json array of entities and add the new entities to the current world
        // If parent pointer is not null, the new entities will be have their parent set to that given pointer
        // If any of the entities has children, this function will be called recursively for these children
        void deserialize(const nlohmann::json &data, Entity *parent = nullptr);

        // helper function to deserialize a single entity
        // This will deserialize a json object of an entity and its children and add the new entity to the current world
        // If parent pointer is not null, the new entity will be have its parent set to that given pointer
        Entity *deserializeEntity(const nlohmann::json &data, Entity *parent = nullptr);

        // This adds an entity to the entities set and returns a pointer to that entity
        // WARNING The entity is owned by this world so don't use "delete" to delete it, instead, call "markForRemoval"
        // to put it in the "markedForRemoval" set. The elements in the "markedForRemoval" set will be removed and
        // deleted when "deleteMarkedEntities" is called.
        Entity *add()
        {
            // Create a new entity
            Entity *entity = new Entity();
            // Set its world member variable to this
            entity->world = this;
            // Insert it in the entites container
            entities.insert(entity);
            // Return a pointer to that entity
            return entity;
        }

        // This returns and immutable reference to the set of all entites in the world.
        const std::unordered_set<Entity *> &getEntities()
        {
            return entities;
        }

        // This marks an entity for removal by adding it to the "markedForRemoval" set.
        // The elements in the "markedForRemoval" set will be removed and deleted when "deleteMarkedEntities" is called.
        void markForRemoval(Entity *entity)
        {
            // If the entity is in this world, add it to the "markedForRemoval" set.
            if (entities.find(entity) != entities.end())
            {
                markedForRemoval.insert(entity);
                for (Entity *child : entities)
                    if (child->parent == entity)
                        markForRemoval(child);
            }
        }

        // This removes the elements in "markedForRemoval" from the "entities" set.
        // Then each of these elements are deleted.
        void deleteMarkedEntities()
        {
            // Remove and delete all the entities that have been marked for removal
            for (Entity *entity : markedForRemoval)
            {
                // Erase the entity from the entities set
                entities.erase(entity);
                // Delete the entity
                delete entity;
            }

            // Clear the "markedForRemoval" set
            markedForRemoval.clear();
        }

        // This deletes all entities in the world
        void clear()
        {
            // Delete all the entities that have been marked for removal
            deleteMarkedEntities();

            // Delete all the entites and make sure that the containers are empty
            for (Entity *entity : entities)
            {
                delete entity;
            }

            // Clear the entities set
            entities.clear();
        }

        // Since the world owns all of its entities, they should be deleted alongside it.
        ~World()
        {
            clear();
        }

        // The world should not be copyable
        World(const World &) = delete;
        World &operator=(World const &) = delete;
    };

}