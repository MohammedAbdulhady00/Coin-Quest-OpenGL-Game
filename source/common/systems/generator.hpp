#pragma once

#include "../ecs/world.hpp"
#include "../components/mesh-renderer.hpp"
#include "../components/movement.hpp"
#include "../components/tags/coin.hpp"
#include "../components/collision.hpp"
#include "../components/free-camera-controller.hpp"
#include "../components/camera.hpp"
#include "../components/player.hpp"
#include <glm/glm.hpp>
#include <glm/gtc/constants.hpp>
#include <glm/trigonometric.hpp>
#include <glm/gtx/fast_trigonometry.hpp>
#include <random>
#include <iostream>

namespace our
{

    class GeneratorSystem
    {
    private:
        int last_gen = 0;
        int generationPlaneOffset = 100;  // The distance from the player at which generated entitites are generated
        int destructionPlaneOffset = -10; // The distance from the player at which generated entities are destroyed
        int horizontalDistance = 7;       // The horizontal distance from the player at which generated entities are generated
        double generationChance = 0.25;   // The chance that an entity will be generated
        double coinChance = 0.25;         // The chance that a coin will be generated
        double obstacleChance = 0.25;     // The chance that an obstacle will be generated
        double heartChance = 0.25;        // The chance that a heart will be generated
        int generationStep = 1;           // The distance between each generation attempt
        std::mt19937 rng;
        std::uniform_real_distribution<double> distribution;
        nlohmann::json coinConfig;
        nlohmann::json obstacleConfig;
        nlohmann::json heartConfig;

        inline Entity *randomEntityFactory(Entity *entity)
        {
            // Generate a random number between 0 and 1
            double random = distribution(rng);

            double currentChance = coinChance;
            // If the random number is less than the coin chance, generate a coin
            if (random < currentChance)
            {
                entity->deserialize(coinConfig);
            }
            // If the random number is less than the obstacle chance, generate an obstacle
            else if (random < currentChance + obstacleChance)
            {
                entity->deserialize(obstacleConfig);
            }
            // must generate a heart
            else
            {
                entity->deserialize(heartConfig);
            }

            // for cascading
            return entity;
        }

    public:
        GeneratorSystem()
        {
            std::random_device rand_dev;
            rng = std::mt19937(rand_dev());
            distribution = std::uniform_real_distribution<double>(0.0, 1.0);
        }

        void initialize(const nlohmann::json &config)
        {
            last_gen = config.value("initialGeneration", last_gen);
            generationPlaneOffset = config.value("generationPlaneOffset", generationPlaneOffset);
            destructionPlaneOffset = config.value("destructionPlaneOffset", destructionPlaneOffset);
            horizontalDistance = config.value("horizontalDistance", horizontalDistance);
            generationChance = config.value("generationChance", generationChance);
            generationStep = config.value("generationStep", generationStep);
            coinChance = config.value("coinChance", coinChance);
            obstacleChance = config.value("obstacleChance", obstacleChance);
            heartChance = config.value("heartChance", heartChance);
            coinConfig = config.value("coin", nlohmann::json());
            obstacleConfig = config.value("obstacle", nlohmann::json());
            heartConfig = config.value("heart", nlohmann::json());
        }

        // This should be called every frame to update all entities containing a CoinComponent.
        void update(World *world, float deltaTime)
        {
            glm::vec3 playerPosition = glm::vec3(0, 0, 0);
            // For each entity in the world get the player position
            for (auto entity : world->getEntities())
            {
                PlayerComponent *player = entity->getComponent<PlayerComponent>();
                if (player)
                    playerPosition = player->getOwner()->localTransform.position;
            }

            // If the number of coins is less than the max number of coins, add a new coin
            while (last_gen > playerPosition.z - generationPlaneOffset)
            {
                for (int i = -horizontalDistance / 2; i <= horizontalDistance / 2; ++i)
                {
                    if (distribution(rng) <= generationChance)
                    {
                        Entity *generatedEntity = world->add();
                        randomEntityFactory(generatedEntity);
                        if (generatedEntity->getComponent<ObstacleTagComponent>())
                            std::cout << "has obstacle component";
                        generatedEntity->localTransform.position = glm::vec3(i, generatedEntity->localTransform.position.y, last_gen);
                        std::cout << "Generated entity at " << generatedEntity->localTransform.position.x << ", " << generatedEntity->localTransform.position.y << ", " << generatedEntity->localTransform.position.z << std::endl;
                    }
                }
                last_gen -= generationStep;
            }
        }
    };
}
