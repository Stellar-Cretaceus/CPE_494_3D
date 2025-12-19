#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>
#include <learnopengl/filesystem.h>
#include <learnopengl/shader.h>
#include <learnopengl/camera.h>
#include <learnopengl/animator.h>
#include <learnopengl/model_animation.h>
#include <learnopengl/animation.h>
#include <assimp/Importer.hpp>
#include <assimp/scene.h>
#include <assimp/postprocess.h>
#include <fstream>
#include <iostream>
#include <vector>
#include <random>

// Function declarations
void framebuffer_size_callback(GLFWwindow* window, int width, int height);
//void mouse_callback(GLFWwindow* window, double xpos, double ypos);
//void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);

unsigned int loadTexture(const char* path);

static bool fileExists(const std::string& path) {
    std::ifstream f(path.c_str());
    return f.good();
}

static bool canLoadAnimation(const std::string& path) {
    Assimp::Importer importer;
    const aiScene* scene = importer.ReadFile(path, aiProcess_Triangulate);
    if (!scene || !scene->mRootNode) {
        std::cerr << "Cannot load animation: " << path << "\n";
        std::cerr << "  Assimp error: " << importer.GetErrorString() << "\n";
        return false;
    }
    return true;
}

// Settings
const unsigned int SCR_WIDTH = 1000;
const unsigned int SCR_HEIGHT = 500;
const unsigned int textureWidth = 1000;
const unsigned int textureHeight = 500;

// Camera
Camera camera(glm::vec3(0.0f, 0.5f, 3.0f));
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// Timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Animation state machine
enum AnimState {
    IDLE = 1, IDLE_PUNCH, PUNCH_IDLE,
    IDLE_KICK, KICK_IDLE, IDLE_SLASH, SLASH_IDLE, SLASH
};

// Scene Pos
float scenePosX = 0.0f;
float speed = 1.0f;








glm::mat4 GetBoneMatrix(Model& model, Animator& animator, const std::string& boneName) {
    auto& boneMap = model.GetBoneInfoMap();
    int boneIndex = -1;
    auto it = boneMap.find(boneName);
    if (it != boneMap.end()) {
        boneIndex = it->second.id;
    }
    if (boneIndex < 0) return glm::mat4(1.0f);
    auto boneMatrices = animator.GetFinalBoneMatrices();
    glm::mat4 offset = boneMap[boneName].offset;
    // Model space: final * inverse(offset)
    return boneMatrices[boneIndex] * glm::inverse(offset);
}



int main() {
    // GLFW initialization
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Animation Blend Demo", NULL, NULL);
    if (!window) {
        std::cerr << "Failed to create GLFW window\n";
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);
    //glfwSetCursorPosCallback(window, mouse_callback);
    //glfwSetScrollCallback(window, scroll_callback);
    glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress)) {
        std::cerr << "Failed to initialize GLAD\n";
        return -1;
    }

    stbi_set_flip_vertically_on_load(true);
    glEnable(GL_DEPTH_TEST);

    Shader ourShader("anim_model.vs", "anim_model.fs");
    Shader picShader("bg_light.vs", "bg_light.fs");

    // Resource paths
    const std::string modelPath = FileSystem::getPath("resources/objects/goth_katana/run.dae");
    const std::string idlePath = FileSystem::getPath("resources/objects/goth_katana/run.dae");
    const std::string slashPath = FileSystem::getPath("resources/objects/goth_katana/slash.dae");


    const std::string katanaPath = FileSystem::getPath("resources/objects/katana/katana.dae");
    Model* katana = fileExists(katanaPath) ? new Model(katanaPath) : nullptr;

    const std::string forestPath = FileSystem::getPath("resources/objects/winter_forest/winter_forest.dae");
    Model* forest = fileExists(forestPath) ? new Model(forestPath) : nullptr;

    const std::string stonePath = FileSystem::getPath("resources/objects/winter_forest/black_energy.dae");
    Model* stone = fileExists(stonePath) ? new Model(stonePath) : nullptr;

    stbi_set_flip_vertically_on_load(true);
    unsigned int bg = loadTexture(FileSystem::getPath("resources/textures/grey_wall.jpg").c_str());


    // Verify files
    if (!fileExists(modelPath)) {
        std::cerr << "Missing model file: " << modelPath << "\n";
        glfwTerminate();
        return -1;
    }

    Model ourModel(modelPath);
    //std::cout << "Model loaded: " << modelPath << "\n";

    // Try loading each animation safely
    Animation* idleAnimation = nullptr;
    Animation* slashAnimation = nullptr;

    if (fileExists(idlePath) && canLoadAnimation(idlePath))
        idleAnimation = new Animation(idlePath, &ourModel);
    if (fileExists(slashPath) && canLoadAnimation(slashPath))
        slashAnimation = new Animation(slashPath, &ourModel);

    if (!idleAnimation) {
        std::cerr << "No valid idle animation found. Exiting.\n";
        glfwTerminate();
        return -1;
    }

    Animator animator(idleAnimation);

    AnimState charState = IDLE;
    float blendAmount = 0.0f;
    float blendRate = 0.055f;

    ///////katana
    std::string handBone = "RightHand_49";
    glm::vec3 katanaScale(0.03f);
    glm::vec3 katanaRotHand(0.0f, 90.0f, 0.0f);
    glm::vec3 katanaOffsetHand(0.1f, 0.1f, 0.1f);

    /////generate Stone
    std::vector<float> stonePositions;
    int maxX = 100; // Max range for obstacles

    std::default_random_engine generator;
    std::uniform_int_distribution<int> distribution(0, 1);

    for (int x = 10; x <= maxX; x += 5) {
        if (distribution(generator)) { // 50% chance
            stonePositions.push_back(float(x));
        }
    }


    //// quad vertice (test)
    float quadVertices[] = {
        // positions           // texcoords
     -0.5f,  0.5f, 0.0f,  0.0f, 1.0f, // top-left
    -0.5f, -0.5f, 0.0f,  0.0f, 0.0f, // bottom-left
     0.5f,  0.5f, 0.0f,  1.0f, 1.0f, // top-right
     0.5f, -0.5f, 0.0f,  1.0f, 0.0f  // bottom-right
    };

    /////////////////test
    unsigned int quadVAO, quadVBO;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadVertices), quadVertices, GL_STATIC_DRAW);

    // position attribute (3 floats now)
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    // texcoords attribute (2 floats)
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 5 * sizeof(float), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glBindVertexArray(0);



    // Render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        if (charState != SLASH) {
            scenePosX += speed * deltaTime;
        }
        camera.Position.x = scenePosX + 1.0f; // start at x=2, move with scene

        ///player collide check
        float playerRadius = 0.5f; // Adjust for your model's size
        float playerX = scenePosX; // If player moves along X
        for (float stoneX : stonePositions) {
            if (fabsf(stoneX - playerX) < playerRadius) {
                std::cout << "Player died! Collided with stone at X = " << stoneX << std::endl;
                glfwSetWindowShouldClose(window, true); // Gracefully closes window
                // Or simply exit(0); for immediate exit
                // exit(0);
                break; // Prevent further checks
            }
        }



        processInput(window);

        // Animation state logic
        switch (charState) {
        case IDLE:
            if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS && slashAnimation) {
                blendAmount = 0.0f;
                animator.PlayAnimation(idleAnimation, slashAnimation, animator.m_CurrentTime, 0.0f, blendAmount);
                charState = IDLE_SLASH;
            }
            break;

        case IDLE_SLASH:
            if (slashAnimation) {
                blendAmount += blendRate;
                blendAmount = fmin(blendAmount, 1.0f);
                animator.PlayAnimation(idleAnimation, slashAnimation, animator.m_CurrentTime, animator.m_CurrentTime2, blendAmount);
                if (blendAmount >= 1.0f) {
                    animator.PlayAnimation(slashAnimation, NULL, animator.m_CurrentTime2, 0.0f, 0.0f);
                    charState = SLASH;
                }
            }
            break;

        case SLASH:
            if (slashAnimation) {
                animator.PlayAnimation(slashAnimation, NULL, animator.m_CurrentTime, 0.0f, 0.0f);

                // Frame boundary logic only, ignore key
                float ticksPerSecond = slashAnimation->GetTicksPerSecond();
                float currentFrame = animator.m_CurrentTime * ticksPerSecond;

                //somehow 1 works
                if (currentFrame >= 1) {
                    charState = SLASH_IDLE;
                    blendAmount = 0.0f;
                }
            }
            break;

        case SLASH_IDLE:
            if (slashAnimation) {
                blendAmount += blendRate;
                blendAmount = fmin(blendAmount, 1.0f);
                animator.PlayAnimation(slashAnimation, idleAnimation, animator.m_CurrentTime, animator.m_CurrentTime2, blendAmount);
                if (blendAmount >= 1.0f) {
                    animator.PlayAnimation(idleAnimation, NULL, animator.m_CurrentTime2, 0.0f, 0.0f);
                    charState = IDLE;
                }
            }
            break;
        }

        // Update animator
        animator.UpdateAnimation(deltaTime);

        /////Calculate Collision
        float katanaOffset = 0.0f; // Adjust based on your bone/bone matrix/hand offset logic
        float katanaX = scenePosX + katanaOffset;
        float katanaHitboxRadius = 1.1f; // Or whatever size best fits your weapon

        for (size_t i = 0; i < stonePositions.size(); ) {
            float stoneX = stonePositions[i];
            if (charState == SLASH && fabsf(stoneX - katanaX) < katanaHitboxRadius) {
                stonePositions.erase(stonePositions.begin() + i);
            }
            else {
                ++i;
            }
        }



        // Render
        glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        ourShader.use();
        glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
        glm::mat4 view = camera.GetViewMatrix();
        ourShader.setMat4("projection", projection);
        ourShader.setMat4("view", view);

        auto transforms = animator.GetFinalBoneMatrices();
        for (int i = 0; i < (int)transforms.size(); ++i)
            ourShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(scenePosX, -0.4f, 0.0f));
        model = glm::rotate(model, glm::radians(45.0f), glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(0.5f));
        ourShader.setMat4("model", model);

        ourModel.Draw(ourShader);

        if (katana && charState == SLASH) { // Only draw katana while slashing
            glm::mat4 boneMat = GetBoneMatrix(ourModel, animator, handBone);
            glm::mat4 katanaMat = model * boneMat;

            // Apply attachment transform
            katanaMat = glm::translate(katanaMat, katanaOffsetHand);
            katanaMat = glm::rotate(katanaMat, glm::radians(katanaRotHand.x), glm::vec3(1, 0, 0));
            katanaMat = glm::rotate(katanaMat, glm::radians(katanaRotHand.y), glm::vec3(0, 1, 0));
            katanaMat = glm::rotate(katanaMat, glm::radians(katanaRotHand.z), glm::vec3(0, 0, 1));
            katanaMat = glm::scale(katanaMat, katanaScale);

            ourShader.setMat4("model", katanaMat);
            for (size_t i = 0; i < katana->meshes.size(); ++i)
                katana->meshes[i].Draw(ourShader);
        }

        for (float stoneX : stonePositions) {
            if (stone && stoneX > 0) {
                glm::mat4 stoneModel = glm::mat4(1.0f);
                stoneModel = glm::translate(stoneModel, glm::vec3(stoneX, 0.0f, 0.0f));
                stoneModel = glm::scale(stoneModel, glm::vec3(0.5f)); // Set scale as needed
                ourShader.setMat4("model", stoneModel);
                stone->Draw(ourShader);
            }
        }

        if (forest) {
            glm::mat4 forestModel = glm::mat4(1.0f);
            forestModel = glm::translate(forestModel, glm::vec3(40.0f, -1.4f, -20.0f)); // Try moving first
            forestModel = glm::scale(forestModel, glm::vec3(0.1f, 1.0f, 0.1f));
            forestModel = glm::rotate(forestModel, glm::radians(180.0f), glm::vec3(1, 0, 0));
            forestModel = glm::rotate(forestModel, glm::radians(45.0f), glm::vec3(0, 1, 0));  // 4. Translate last
            ourShader.setMat4("model", forestModel);
            forest->Draw(ourShader);
        }

        //////////////////PICS
        // bind the background shader
        picShader.use();
        picShader.setMat4("view", view);
        picShader.setMat4("projection", projection);

        model = glm::mat4(1.0f);

        float scaleX = 100.0f; // base width in world units
        float scaleY = 100.0f; // base height in world units

        float aspect = (float)textureWidth / (float)textureHeight;
        model = glm::translate(glm::mat4(1.0f), glm::vec3(scenePosX, -0.4f, -70.0f));
        model = glm::scale(model, glm::vec3(aspect * scaleX, scaleY, 4.0f));
        picShader.setMat4("model", model);

        glBindVertexArray(quadVAO);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, bg);
        picShader.setInt("screenTexture", 0);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);


        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    delete idleAnimation;
    delete slashAnimation;
    glfwTerminate();
    return 0;
}

// Input handling
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    //if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
    //    camera.ProcessKeyboard(FORWARD, deltaTime);
    //if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
    //    camera.ProcessKeyboard(BACKWARD, deltaTime);
    //if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
    //    camera.ProcessKeyboard(LEFT, deltaTime);
    //if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
    //    camera.ProcessKeyboard(RIGHT, deltaTime);
}

void framebuffer_size_callback(GLFWwindow* window, int width, int height) {
    glViewport(0, 0, width, height);
}

unsigned int loadTexture(char const* path)
{
    unsigned int textureID;
    glGenTextures(1, &textureID);

    int width, height, nrComponents;
    unsigned char* data = stbi_load(path, &width, &height, &nrComponents, 0);
    if (data)
    {
        GLenum format;
        if (nrComponents == 1)
            format = GL_RED;
        else if (nrComponents == 3)
            format = GL_RGB;
        else if (nrComponents == 4)
            format = GL_RGBA;

        glBindTexture(GL_TEXTURE_2D, textureID);
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

        stbi_image_free(data);
    }
    else
    {
        std::cout << "Texture failed to load at path: " << path << std::endl;
        stbi_image_free(data);
    }

    return textureID;
}
