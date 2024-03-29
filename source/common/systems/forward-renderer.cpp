#include "forward-renderer.hpp"
#include "../mesh/mesh-utils.hpp"
#include "../texture/texture-utils.hpp"
#include "../components/postprocess.hpp"
#include "../components/light_spectrum.hpp"
#include <ctime>
namespace our
{

    void ForwardRenderer::initialize(glm::ivec2 windowSize, const nlohmann::json &config)
    {
        // First, we store the window size for later use
        this->windowSize = windowSize;

        // Then we check if there is a sky texture in the configuration
        if (config.contains("sky"))
        {
            // First, we create a sphere which will be used to draw the sky
            this->skySphere = mesh_utils::sphere(glm::ivec2(16, 16));

            // We can draw the sky using the same shader used to draw textured objects
            ShaderProgram *skyShader = new ShaderProgram();
            skyShader->attach("assets/shaders/textured.vert", GL_VERTEX_SHADER);
            skyShader->attach("assets/shaders/textured.frag", GL_FRAGMENT_SHADER);
            skyShader->link();

            // pipeline state for sky
            PipelineState skyPipelineState;
            skyPipelineState.depthTesting.enabled = true;
            // since the sphere is drawn far away (z = 1).
            skyPipelineState.depthTesting.function = GL_LEQUAL;
            skyPipelineState.faceCulling.enabled = true;
            // cull the front face since we are inside the sphere
            skyPipelineState.faceCulling.culledFace = GL_FRONT;

            // Load the sky texture (note that we don't need mipmaps since we want to avoid any unnecessary blurring while rendering the sky)
            std::string skyTextureFile = config.value<std::string>("sky", "");
            Texture2D *skyTexture = texture_utils::loadImage(skyTextureFile, false);

            // Setup a sampler for the sky
            Sampler *skySampler = new Sampler();
            skySampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
            skySampler->set(GL_TEXTURE_WRAP_S, GL_REPEAT);
            skySampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

            // Combine all the aforementioned objects (except the mesh) into a material
            this->skyMaterial = new TexturedMaterial();
            this->skyMaterial->shader = skyShader;
            this->skyMaterial->texture = skyTexture;
            this->skyMaterial->sampler = skySampler;
            this->skyMaterial->pipelineState = skyPipelineState;
            this->skyMaterial->tint = glm::vec4(1.0f, 1.0f, 1.0f, 1.0f);
            this->skyMaterial->alphaThreshold = 1.0f;
            this->skyMaterial->transparent = false;
        }

        // Then we check if there is a postprocessing shader in the configuration
        if (config.contains("postprocess_load"))
        {
            if (const auto &shaders = config["postprocess_load"]; shaders.is_array())
            {
                int i = 0;
                for (auto &shader : shaders)
                {
                    GLuint postProcessFrameBuffer;
                    // use postprocessframebuffer of forwardrenderer as our framebuffer
                    glGenFramebuffers(1, &postProcessFrameBuffer);
                    this->postProcessFrameBuffers.push_back(postProcessFrameBuffer);

                    //  Hints: The color format can be (Red, Green, Blue and Alpha components with 8 bits for each channel).
                    //  The depth format can be (Depth component with 24 bits).
                    // the enums of GL_RGBA8 and GL_DEPTH_COMPONENT24 didn't work, so I used GL_RGBA and GL_DEPTH_COMPONENT instead
                    this->depthTarget = texture_utils::empty(GL_DEPTH_COMPONENT, this->windowSize);
                    this->colorTarget = texture_utils::empty(GL_RGBA, this->windowSize);

                    // bind the framebuffer before attach our textures
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postProcessFrameBuffer);
                    // attach the color and depth texture to the framebuffer
                    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, this->colorTarget->getOpenGLName(), 0);
                    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, this->depthTarget->getOpenGLName(), 0);
                    // Unbind the framebuffer just to be safe
                    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

                    GLuint postProcessVertexArray;
                    // Create a vertex array to use for drawing the texture
                    glGenVertexArrays(1, &postProcessVertexArray);
                    this->postProcessVertexArrays.push_back(postProcessVertexArray);

                    // Create a sampler to use for sampling the scene texture in the post processing shader
                    Sampler *postprocessSampler = new Sampler();
                    postprocessSampler->set(GL_TEXTURE_MIN_FILTER, GL_LINEAR);
                    postprocessSampler->set(GL_TEXTURE_MAG_FILTER, GL_LINEAR);
                    postprocessSampler->set(GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                    postprocessSampler->set(GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

                    // Create the post processing shader
                    ShaderProgram *postprocessShader = new ShaderProgram();
                    postprocessShader->attach("assets/shaders/fullscreen.vert", GL_VERTEX_SHADER);

                    postprocessShader->attach(shader, GL_FRAGMENT_SHADER);

                    postprocessShader->link();

                    // Create a post processing material
                    this->postProcessMaterials.push_back(new TexturedMaterial());
                    this->postProcessMaterials[i]->shader = postprocessShader;
                    this->postProcessMaterials[i]->texture = this->colorTarget;
                    this->postProcessMaterials[i]->depthTexture = this->depthTarget;
                    this->postProcessMaterials[i]->sampler = postprocessSampler;

                    // The default options are fine but we don't need to interact with the depth buffer
                    // so it is more performant to disable the depth mask
                    this->postProcessMaterials[i]->pipelineState.depthMask = false;
                    i++;
                }
            }
        }
        // check if there is permanent postprocess effects
        if (config.contains("postprocess_permanent"))
        {
            if (const auto &indicies = config["postprocess_permanent"]; indicies.is_array())
                for (const auto &index : indicies)
                    this->postProcessMaterialIndices.push_back(index);
        }
    }

    void ForwardRenderer::destroy()
    {
        // Delete all objects related to the sky
        if (skyMaterial)
        {
            delete skySphere;
            delete skyMaterial->shader;
            delete skyMaterial->texture;
            delete skyMaterial->sampler;
            delete skyMaterial;
        }
        // Delete all objects related to post processing
        if (!postProcessMaterials.empty())
        {
            for (auto postprocessFrameBuffer : postProcessFrameBuffers)
            {
                glDeleteFramebuffers(1, &postprocessFrameBuffer);
            }
            for (auto postProcessVertexArray : postProcessVertexArrays)
            {
                glDeleteVertexArrays(1, &postProcessVertexArray);
            }
            delete colorTarget;
            delete depthTarget;
        }
        for (auto postprocessMaterial : postProcessMaterials)
        {
            delete postprocessMaterial->sampler;
            delete postprocessMaterial->shader;
            delete postprocessMaterial;
        }

        // clear all the vectors
        postProcessMaterials.clear();
        postProcessMaterialIndices.clear();
    }

    void ForwardRenderer::render(World *world, float deltaTime)
    {
        // First of all, we search for a camera and for all the mesh renderers
        CameraComponent *camera = nullptr;
        opaqueCommands.clear();
        transparentCommands.clear();
        lights.clear();

        // store extra postprocess material indices
        std::vector<int> extraPostProcessMaterialIndices;
        searchWorldEntities(extraPostProcessMaterialIndices, world, camera);

        // If there is no camera, we return (we cannot render without a camera)
        if (camera == nullptr)
            return;

        // get the camera forward direction in world space looking at the negative z axis
        glm::vec3 cameraForward = camera->getOwner()->getLocalToWorldMatrix() * glm::vec4(0.0, 0.0, -1.0f, 0.0f);
        // sort the transparent commands based on the distance from the camera
        // this is done to prevent the transparent objects from being drawn in the wrong order
        std::sort(transparentCommands.begin(), transparentCommands.end(), [cameraForward](const RenderCommand &first, const RenderCommand &second)
                  { 
            // get the distance from the camera to the center of the first and second commands
            return glm::dot(first.center, cameraForward) < glm::dot(second.center, cameraForward); });

        // get the camera view projection matrix
        glm::mat4 VP = camera->getProjectionMatrix(windowSize) * camera->getViewMatrix();

        // set the viewport to the size of the window
        glViewport(0, 0, windowSize.x, windowSize.y);

        // set the clear color to black and the clear depth to 1
        glClearColor(0, 0, 0, 1);
        glClearDepth(1);

        // enable writing to the color and depth buffers
        glColorMask(true, true, true, true);
        glDepthMask(true);

        // If there is postprocessing material, bind the framebuffer
        postProcessInitialFrame(extraPostProcessMaterialIndices, world);

        // clear the color and depth buffers
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // define the max number of lights
        const int MAX_LIGHT_COUNT = 8;

        // draw all the opaque commands
        // no need to sort them because of the z-buffer
        for (auto &command : opaqueCommands)
        {
            command.material->setup();
            // set the camera position
            command.material->shader->set("camera_position", camera->getOwner()->getWorldTranslation());
            // set the M matrix for the shader
            command.material->shader->set("M", command.localToWorld);
            // set the M_IT matrix for the shader
            command.material->shader->set("M_IT", glm::transpose(glm::inverse(command.localToWorld)));
            // set the VP matrix for the shader
            command.material->shader->set("VP", VP);
            // set the MVP matrix for the shader
            command.material->shader->set("transform", VP * command.localToWorld);
            // set the light count
            command.material->shader->set("light_count", static_cast<int>(lights.size()));
            // set the time
            command.material->shader->set("time", static_cast<float>(time));
            // set the light properties
            int light_index = 0;
            // add the light effects
            for (auto light : lights)
            {
                // skip the sky light
                if (light->typeLight == LightType::SKY)
                    continue;
                // skip the light if it is disabled
                if (!light->enabled)
                    continue;

                // get the light position in world space (for point and spot lights)
                glm::vec3 lightPosition = light->getOwner()->getWorldTranslation();
                // get the light direction in world space (for directional and spot lights)
                glm::vec3 lightDirection = light->getOwner()->getLocalToWorldMatrix() * glm::vec4(light->direction, 0.0f);
                lightDirection = glm::normalize(lightDirection);

                // set the light properties in the shader
                std::string prefix = "lights[" + std::to_string(light_index) + "].";
                command.material->shader->set(prefix + "type", static_cast<int>(light->typeLight));

                // set the light color for spectrum lights in shader
                if (dynamic_cast<LightSpectrumComponent *>(light) != nullptr)
                    command.material->shader->set(prefix + "color", ((LightSpectrumComponent *)light)->getColor(time));
                else
                    command.material->shader->set(prefix + "color", light->color);

                // set the light properties based on its type (directional, point, spot)
                switch (light->typeLight)
                {
                    // set the light direction for directional lights
                case LightType::DIRECTIONAL:
                    command.material->shader->set(prefix + "direction", lightDirection);
                    break;
                    // set the light position and attenuation for point lights
                case LightType::POINT:
                    command.material->shader->set(prefix + "position", lightPosition);
                    command.material->shader->set(prefix + "attenuation", glm::vec3(light->attenuation.quadratic,
                                                                                    light->attenuation.linear, light->attenuation.constant));
                    break;
                    // set the light position, direction, attenuation and cone angles for spot lights
                case LightType::SPOT:
                    command.material->shader->set(prefix + "position", lightPosition);
                    command.material->shader->set(prefix + "direction", lightDirection);
                    command.material->shader->set(prefix + "attenuation", glm::vec3(light->attenuation.quadratic,
                                                                                    light->attenuation.linear, light->attenuation.constant));
                    command.material->shader->set(prefix + "cone_angles", glm::vec2(light->spot_angle.inner, light->spot_angle.outer));
                    break;
                }
                // increment the light index
                light_index++;
                // break if we reached the max number of lights
                if (light_index >= MAX_LIGHT_COUNT)
                    break;
            }

            // draw the mesh
            command.mesh->draw();
        }

        // If there is a sky material, draw the sky
        if (this->skyMaterial)
        {
            skyMaterial->setup();
            // get camera position in world space
            glm::vec3 cameraPosition = camera->getOwner()->getLocalToWorldMatrix() * glm::vec4(0.0f, 0.0f, 0.0f, 1.0f);
            // move the sky sphere to the camera position
            glm::mat4 skyModel = glm::translate(glm::mat4(1.0f), cameraPosition);

            // make the sky sphere always behind everything else
            // this is done by setting the z component of the position to 1
            // by scaling z to 0 then translate it to 1
            glm::mat4 alwaysBehindTransform = glm::mat4(
                1.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 1.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 0.0f, 0.0f,
                0.0f, 0.0f, 1.0f, 1.0f);
            // set the MVP matrix for the shader
            skyMaterial->shader->set("transform", alwaysBehindTransform * VP * skyModel);
            // draw the sky sphere
            skySphere->draw();
        }

        // draw all the transparent commands
        for (auto &command : transparentCommands)
        {
            command.material->setup();
            // set the MVP matrix for the shader
            command.material->shader->set("transform", VP * command.localToWorld);
            command.mesh->draw();
        }
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);

        // draw postprocess
        drawPostProcess(extraPostProcessMaterialIndices, camera);

        time += deltaTime;
    }
    inline void ForwardRenderer::searchWorldEntities(std::vector<int> &extraPostProcessMaterialIndices, World *world, CameraComponent *&camera)
    {
        // Search for all world entities
        for (auto entity : world->getEntities())
        {
            // If we hadn't found a camera yet, we look for a camera in this entity
            if (!camera)
                camera = entity->getComponent<CameraComponent>();
            // If this entity has a mesh renderer component
            if (auto meshRenderer = entity->getComponent<MeshRendererComponent>(); meshRenderer)
            {
                // We construct a command from it
                RenderCommand command;
                command.localToWorld = meshRenderer->getOwner()->getLocalToWorldMatrix();
                command.center = glm::vec3(command.localToWorld * glm::vec4(0, 0, 0, 1));
                command.mesh = meshRenderer->mesh;
                command.material = meshRenderer->material;
                // if it is transparent, we add it to the transparent commands list
                if (command.material->transparent)
                {
                    transparentCommands.push_back(command);
                }
                else
                {
                    // Otherwise, we add it to the opaque command list
                    opaqueCommands.push_back(command);
                }
            }
            // If this entity has a light component and it is enabled we add it to the lights list (if it is a sky light, we don't add it)
            auto light = entity->getComponent<LightComponent>();
            if (light && light->enabled)
            {
                // if the light is a sky light, we set its shader uniforms
                if (light->typeLight == LightType::SKY)
                {
                    auto litShader = AssetLoader<ShaderProgram>::get("light");
                    litShader->use();
                    litShader->set("sky.top", glm::vec3(0.0f, 0.1f, 0.5f));
                    litShader->set("sky.horizon", glm::vec3(0.3f, 0.3f, 0.3f));
                    litShader->set("sky.bottom", glm::vec3(0.1f, 0.1f, 0.1f));
                }
                else
                    lights.push_back(light);
            }

            // find the postprocess component
            auto component = entity->getComponent<PostProcessComponent>();
            // if the entity has a PostProcessComponent component check if it is enabled
            if (!component)
                continue;
            if (!component->isEnabled)
                continue;

            // if it has a postprocess component, check if it has a postprocess index
            if (component->postProcessIndex < postProcessMaterials.size())
            {
                extraPostProcessMaterialIndices.push_back(component->postProcessIndex);
            }
        }
    }
    inline void ForwardRenderer::postProcessInitialFrame(std::vector<int> &extraPostProcessMaterialIndices, World *world)
    {

        // If there is a postprocess material, bind the framebuffer
        if (!postProcessMaterialIndices.empty())
        {
            // bind the framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postProcessFrameBuffers[postProcessMaterialIndices[0]]);
        }
        else if (!extraPostProcessMaterialIndices.empty())
        {
            // bind the framebuffer
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postProcessFrameBuffers[extraPostProcessMaterialIndices[0]]);
        }
    }
    inline void ForwardRenderer::drawPostProcess(std::vector<int> &extraPostProcessMaterialIndices, CameraComponent *camera)
    {
        int indx = -1;
        // If there is a postprocess material, apply postprocessing
        // Permanent postprocess effects
        for (int i = 0; i < postProcessMaterialIndices.size(); ++i)
        {
            // this was done by unbinding the postprocessFrameBuffer
            if (i + 1 < postProcessMaterialIndices.size())
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postProcessFrameBuffers[postProcessMaterialIndices[i + 1]]);
            else if (!extraPostProcessMaterialIndices.empty())
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postProcessFrameBuffers[extraPostProcessMaterialIndices[0]]);

            // setup postprocess material
            postProcessMaterials[postProcessMaterialIndices[i]]->setup();
            // send shader uniforms
            postProcessMaterials[postProcessMaterialIndices[i]]->shader->set("inverse_projection", glm::inverse(camera->getProjectionMatrix(windowSize)));
            postProcessMaterials[postProcessMaterialIndices[i]]->shader->set("time", time);

            // bind the postprocess vertex array to draw vertices
            glBindVertexArray(postProcessVertexArrays[postProcessMaterialIndices[i]]);
            // draw the vertices
            glDrawArrays(GL_TRIANGLES, 0, 3);
            indx = postProcessMaterialIndices[i];
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        }
        // If there is an extra postprocess material, apply postprocessing
        // game state postprocess effects
        for (int i = 0; i < extraPostProcessMaterialIndices.size(); ++i)
        {
            // this was done by unbinding the postprocessFrameBuffer
            if (i + 1 < extraPostProcessMaterialIndices.size())
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, postProcessFrameBuffers[extraPostProcessMaterialIndices[i + 1]]);

            // setup postprocess material
            postProcessMaterials[extraPostProcessMaterialIndices[i]]->setup();
            // send shader uniforms
            postProcessMaterials[extraPostProcessMaterialIndices[i]]->shader->set("inverse_projection", glm::inverse(camera->getProjectionMatrix(windowSize)));
            postProcessMaterials[extraPostProcessMaterialIndices[i]]->shader->set("time", time);

            // bind the postprocess vertex array to draw vertices
            glBindVertexArray(postProcessVertexArrays[extraPostProcessMaterialIndices[i]]);
            // draw the vertices
            glDrawArrays(GL_TRIANGLES, 0, 3);
            indx = extraPostProcessMaterialIndices[i];
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        }
    }
}
