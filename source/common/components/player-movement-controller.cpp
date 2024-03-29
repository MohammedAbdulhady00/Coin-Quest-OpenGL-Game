#include "free-camera-controller.hpp"
#include "../ecs/entity.hpp"
#include "../deserialize-utils.hpp"
#include "player-movement-controller.hpp"

namespace our
{
    // Reads sensitivities & speedupFactor from the given json object
    void PlayerMovementControllerComponent::deserialize(const nlohmann::json &data)
    {
        if (!data.is_object())
            return;
        rotationSensitivity = data.value("rotationSensitivity", rotationSensitivity);
        fovSensitivity = data.value("fovSensitivity", fovSensitivity);
        positionSensitivity = data.value("positionSensitivity", positionSensitivity);
        speedupFactor = data.value("speedupFactor", speedupFactor);
        maxHorizontalDistance = data.value("maxHorizontalDistance", maxHorizontalDistance);
        jumpSpeed = data.value("jumpSpeed", jumpSpeed);
    }
}