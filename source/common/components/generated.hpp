#pragma once

#include "../ecs/component.hpp"

namespace our
{
    // A generated component is a component that denotes that the entity was generated by the level generator
    // It is used to mark an entity as having been generated so that it can be destroyed when it goes out of bounds
    class GeneratedComponent : public Component
    {
    public:
        // the offset at which the object will be destroyed
        double destructionOffset = -10.0;
        virtual void deserialize(const nlohmann::json &data) override
        {
            if (!data.is_object())
                return;
            destructionOffset = data.value("destructionOffset", destructionOffset);
        }
        static std::string getID() { return "GeneratedTag"; }
    };
}