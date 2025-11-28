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
    IDLE = 1,
    MAGIC, IDLE_MAGIC, MAGIC_IDLE,
    JUMP, IDLE_JUMP, JUMP_IDLE,
    CROUCH,IDLE_CROUCH, CROUCH_IDLE
};

struct Orb {
    float x;
    float y;
    float z;
    float speed;
    bool alive = true;
};
std::vector<Orb> orbs;

struct Stone {
    float x;
    float scale; // 0.2 or 1.0
};
std::vector<Stone> stones;



// Scene Pos
float scenePosX = 0.0f;
float speed = 2.0f;



std::vector<std::string> legBones = {
    "LeftFoot_58",
    "LeftToeBase_57",
    "RightFoot_63",
    "RightToeBase_62",
    "LeftLeg_59",
    "LeftUpLeg_60",
    "RightLeg_64",
    "RightUpLeg_65",
    "Hips_66",
    "Neck_4",
    "Head_3",
    "LeftEye_1",
    "RightEye_2",
    "HeadTop_End_0"
};




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



void PrintAllBoneNames(Model& model) {
    auto& boneMap = model.GetBoneInfoMap();
    std::cout << "Bones in model:" << std::endl;
    for (auto& pair : boneMap) {
        std::cout << "  " << pair.first << " (ID: " << pair.second.id << ")" << std::endl;
    }
}

void DrawOrbs(
    std::vector<Orb>& orbs,
    Shader& orbShader,
    Model* lightingOrb,
    const glm::mat4& view,
    const glm::mat4& projection,
    const glm::vec3& cameraPos
    ) {
    if (!lightingOrb) return;

    orbShader.use();
    orbShader.setMat4("view", view);
    orbShader.setMat4("projection", projection);

    // Basic lighting uniforms (won't go black)
    orbShader.setVec3("viewPos", cameraPos);

    glm::vec3 lightPos = cameraPos + glm::vec3(0.0f, 2.0f, 2.0f);
    orbShader.setVec3("lightPos", lightPos);
    orbShader.setVec3("lightColor", glm::vec3(1.0f));
    orbShader.setFloat("ambientStrength", 0.25f);

    // Glow color (optional)
    orbShader.setVec3("emissionColor", glm::vec3(0.3f, 0.5f, 1.0f));
    orbShader.setFloat("emissionStrength", 1.0f);

    for (auto& orb : orbs) {
        if (!orb.alive) continue;

        glm::mat4 model = glm::mat4(1.0f);
        model = glm::translate(model, glm::vec3(orb.x, orb.y, orb.z));
        model = glm::scale(model, glm::vec3(0.05f));

        orbShader.setMat4("model", model);

        glm::mat3 normalMatrix = glm::mat3(glm::transpose(glm::inverse(model)));
        orbShader.setMat3("normalMatrix", normalMatrix);

        lightingOrb->Draw(orbShader);
    }
}

void DrawBackgroundPic(
    Shader& picShader,
    GLuint quadVAO,
    GLuint textureID,
    int textureWidth,
    int textureHeight,
    float scenePosX,
    const glm::mat4& view,
    const glm::mat4& projection
) {
    picShader.use();
    picShader.setMat4("view", view);
    picShader.setMat4("projection", projection);

    float scaleX = 100.0f;
    float scaleY = 100.0f;

    float aspect = (float)textureWidth / (float)textureHeight;

    glm::mat4 model = glm::mat4(1.0f);
    model = glm::translate(model, glm::vec3(scenePosX, -0.4f, -70.0f));
    model = glm::scale(model, glm::vec3(aspect * scaleX, scaleY, 4.0f));

    picShader.setMat4("model", model);

    glBindVertexArray(quadVAO);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    picShader.setInt("screenTexture", 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
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
    Shader orbShader("orbShader.vs", "orbShader.fs");

    // Resource paths
    const std::string modelPath = FileSystem::getPath("resources/objects/goth_katana/run.dae");
    const std::string idlePath = FileSystem::getPath("resources/objects/goth_katana/run.dae");
    const std::string shootMagicPath = FileSystem::getPath("resources/objects/goth_katana/shoot.dae");
    const std::string jumpPath = FileSystem::getPath("resources/objects/goth_katana/jump.dae");
    const std::string crouchPath = FileSystem::getPath("resources/objects/goth_katana/crouch.dae");


    const std::string katanaPath = FileSystem::getPath("resources/objects/katana/katana.dae");
    Model* katana = fileExists(katanaPath) ? new Model(katanaPath) : nullptr;

    const std::string lightingOrbPath = FileSystem::getPath("resources/objects/bullets/blue_orb/blue_orb.obj");
    Model* lightingOrb = fileExists(lightingOrbPath) ? new Model(lightingOrbPath) : nullptr;

    const std::string forestPath = FileSystem::getPath("resources/objects/winter_forest/winter_forest.dae");
    Model* forest = fileExists(forestPath) ? new Model(forestPath) : nullptr;

    const std::string stonePath = FileSystem::getPath("resources/objects/winter_forest/black_energy.dae");
    Model* stoneModel = fileExists(stonePath) ? new Model(stonePath) : nullptr;

    stbi_set_flip_vertically_on_load(true);
    unsigned int bg = loadTexture(FileSystem::getPath("resources/textures/grey_wall.jpg").c_str());


    // Verify files
    if (!fileExists(modelPath)) {
        std::cerr << "Missing model file: " << modelPath << "\n";
        glfwTerminate();
        return -1;
    }

    Model ourModel(modelPath);
    //PrintAllBoneNames(ourModel);

    // Try loading each animation safely
    Animation* idleAnimation = nullptr;
    Animation* shootMagicAnimation = nullptr;
    Animation* jumpAnimation = nullptr;
    Animation* crouchAnimation = nullptr;

    if (fileExists(idlePath) && canLoadAnimation(idlePath))
        idleAnimation = new Animation(idlePath, &ourModel);
    if (fileExists(shootMagicPath) && canLoadAnimation(shootMagicPath))
        shootMagicAnimation = new Animation(shootMagicPath, &ourModel);
    if (fileExists(jumpPath) && canLoadAnimation(jumpPath))
        jumpAnimation = new Animation(jumpPath, &ourModel);
    if (fileExists(crouchPath) && canLoadAnimation(crouchPath))
        crouchAnimation = new Animation(crouchPath, &ourModel);

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
    std::vector<Stone> stones;
    int maxX = 200; // Max range for obstacles

    std::random_device rd;
    std::mt19937 generator(rd());
    std::bernoulli_distribution spawnDist(1.0); // 50% chance to spawn stone
    std::bernoulli_distribution scaleDist(0.5); // 50% chance to pick 1.0, else 0.2

    for (int x = 25; x <= maxX; x += 5) {
        if (spawnDist(generator)) {
            Stone s;
            s.x = float(x) + ((rand() % 100) / 100.0f - 0.5f); // optional small random X offset ±0.5
            s.scale = scaleDist(generator) ? 1.0f : 0.2f;
            stones.push_back(s);
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

    float frozenSlashTime = 0.0f;
    float frozenJumpTime = 0.0f;
    float frozenCrouchTime = 0.0f;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        //running and stop running
        if (charState) {
            scenePosX += speed * deltaTime;
        }
        camera.Position.x = scenePosX + 1.0f; // start at x=2, move with scene

        // --- Player position & jump logic ---
        float baseY = -0.4f;       // standing bottom
        float jumpHeight = 1.0f;   // max jump height
        float playerHeight = 1.0f; // full height of player model

        static float jumpEndY = 0.0f;

        float posY = baseY;

        // Update posY according to jump state
        if (charState == IDLE_JUMP && jumpAnimation) {
            posY += blendAmount * jumpHeight;
        }
        else if (charState == JUMP && jumpAnimation) {
            float ticksPerSecond = jumpAnimation->GetTicksPerSecond();
            float currentFrame = animator.m_CurrentTime * ticksPerSecond;

            // progress 0 → 1
            float progress = glm::clamp(currentFrame / 0.73f, 0.0f, 1.0f);

            // sine wave jump
            posY += sin(progress * glm::pi<float>()) * jumpHeight;

            // store end Y when finished
            if (currentFrame >= 0.73f) jumpEndY = posY;
        }
        else if (charState == JUMP_IDLE && jumpAnimation) {
            posY = jumpEndY;
        }

        // --- Player collision check ---
        float playerX = scenePosX;
        float playerY = posY; // bottom of player

        for (auto& stone : stones) {
            float stoneX = stone.x;
            float stoneY = 0.0f;                  // ground level
            float stoneWidth = stone.scale + 0.1f;  // half-width scaled
            float stoneHeight = 0.25f * stone.scale;  // height scaled

            // X collision
            bool collideX = fabsf(stoneX - playerX) < stoneWidth;

            // Y collision: check vertical overlap
            bool collideY = (playerY < stoneY + stoneHeight) && (playerY + playerHeight > stoneY);

            if (collideX && collideY) {
                std::cout << "Player died! Collided with stone at X = " << stoneX << std::endl;
                glfwSetWindowShouldClose(window, true);
                break;
            }
        }




        processInput(window);


        float duration = shootMagicAnimation->GetDuration();
        //printf("Slash Duration: %f\n", slashAnimation->GetDuration());

        // Animation state logic
        switch (charState) {
        case IDLE:
            // --- Magic trigger ---
            if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS && shootMagicAnimation) {
                blendAmount = 0.0f;
                animator.PlayAnimation(idleAnimation, shootMagicAnimation, idleAnimation,
                    animator.m_CurrentTime, 0.0f, animator.m_lowerTime, blendAmount);
                charState = IDLE_MAGIC;
            }

            // --- Jump trigger ---
            if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && jumpAnimation) {
                blendAmount = 0.0f;
                animator.PlayAnimation(idleAnimation, jumpAnimation, idleAnimation,
                    animator.m_CurrentTime, 0.0f, animator.m_lowerTime, blendAmount);
                charState = IDLE_JUMP;
            }

            // --- Crouch trigger ---
            if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS && crouchAnimation) {
                blendAmount = 0.0f;
                animator.PlayAnimation(idleAnimation, crouchAnimation, idleAnimation,
                    animator.m_CurrentTime, 0.0f, animator.m_lowerTime, blendAmount);
                charState = IDLE_CROUCH;
            }
            break;

            // -------------------------------
            // MAGIC sequence (same as before)
        case IDLE_MAGIC:
            if (shootMagicAnimation) {
                if (blendAmount <= 0.1f) {
                    // spawn orb
                    Orb orb;
                    orb.x = scenePosX + 0.5f;
                    orb.y = 0.2f;
                    orb.z = 0.0f;
                    orb.speed = 7.0f;
                    orb.alive = true;
                    orbs.push_back(orb);
                }

                blendAmount += 0.1f;
                blendAmount = fmin(blendAmount, 1.0f);
                animator.PlayAnimation(idleAnimation, shootMagicAnimation, idleAnimation,
                    animator.m_CurrentTime, animator.m_CurrentTime2, animator.m_lowerTime, blendAmount);
                if (blendAmount >= 1.0f) {
                    animator.PlayAnimation(shootMagicAnimation, NULL, idleAnimation,
                        animator.m_CurrentTime2, 0.0f, animator.m_lowerTime, 0.0f);
                    charState = MAGIC;
                }
            }
            break;

        case MAGIC:
            if (shootMagicAnimation) {
                animator.PlayAnimation(shootMagicAnimation, idleAnimation, idleAnimation,
                    animator.m_CurrentTime, 0.0f, animator.m_lowerTime, 0.0f);
                float ticksPerSecond = shootMagicAnimation->GetTicksPerSecond();
                float currentFrame = animator.m_CurrentTime * ticksPerSecond;

                if (currentFrame >= 0.73f) {
                    charState = MAGIC_IDLE;
                    blendAmount = 0.0f;
                    frozenSlashTime = animator.m_CurrentTime;
                }
                else if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
                    charState = IDLE_MAGIC;
                    blendAmount = 0.0f;
                }
                else if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && jumpAnimation) {
                    blendAmount = 0.0f;
                    animator.PlayAnimation(idleAnimation, jumpAnimation, idleAnimation,
                        animator.m_CurrentTime, 0.0f, animator.m_lowerTime, blendAmount);
                    charState = IDLE_JUMP;
                }
            }
            break;

        case MAGIC_IDLE:
            if (shootMagicAnimation) {
                blendAmount += 0.1f;
                blendAmount = fmin(blendAmount, 1.0f);
                animator.PlayAnimation(shootMagicAnimation, idleAnimation, idleAnimation,
                    frozenSlashTime, animator.m_CurrentTime2, animator.m_lowerTime, blendAmount);
                if (blendAmount >= 1.0f) {
                    animator.PlayAnimation(idleAnimation, NULL, idleAnimation,
                        animator.m_CurrentTime2, 0.0f, animator.m_lowerTime, 0.0f);
                    charState = IDLE;
                }
                else if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
                    charState = IDLE_MAGIC;
                    blendAmount = 0.0f;
                }
                else if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS && jumpAnimation) {
                    blendAmount = 0.0f;
                    animator.PlayAnimation(idleAnimation, jumpAnimation, idleAnimation,
                    animator.m_CurrentTime, 0.0f, animator.m_lowerTime, blendAmount);
                    charState = IDLE_JUMP;
                }
            }
            break;


        // -------------------------------
        // JUMP sequence
        case IDLE_JUMP:
            if (jumpAnimation) {
                blendAmount += 0.1f;
                blendAmount = fmin(blendAmount, 1.0f);
                animator.PlayAnimation(idleAnimation, jumpAnimation, idleAnimation,
                    animator.m_CurrentTime, animator.m_CurrentTime2, animator.m_lowerTime, blendAmount);
                if (blendAmount >= 1.0f) {
                    animator.PlayAnimation(jumpAnimation, NULL, idleAnimation,
                        animator.m_CurrentTime2, 0.0f, animator.m_lowerTime, 0.0f);
                    charState = JUMP;
                }
            }
            break;

        case JUMP:
            if (jumpAnimation) {
                animator.PlayAnimation(jumpAnimation, idleAnimation, idleAnimation,
                    animator.m_CurrentTime, 0.0f, animator.m_lowerTime, 0.0f);
                float ticksPerSecond = jumpAnimation->GetTicksPerSecond();
                float currentFrame = animator.m_CurrentTime * ticksPerSecond;

                if (currentFrame >= 0.73f) {
                    charState = JUMP_IDLE;
                    blendAmount = 0.0f;
                    frozenJumpTime = animator.m_CurrentTime;
                }
            }
            break;

        case JUMP_IDLE:
            if (jumpAnimation) {
                blendAmount += 0.1f;
                blendAmount = fmin(blendAmount, 1.0f);
                animator.PlayAnimation(jumpAnimation, idleAnimation, idleAnimation,
                    frozenJumpTime, animator.m_CurrentTime2, animator.m_lowerTime, blendAmount);
                if (blendAmount >= 1.0f) {
                    animator.PlayAnimation(idleAnimation, NULL, idleAnimation,
                        animator.m_CurrentTime2, 0.0f, animator.m_lowerTime, 0.0f);
                    charState = IDLE;
                }
            }
            break;
        //////////
        case IDLE_CROUCH:
            if (crouchAnimation) {
                blendAmount += 0.1f;
                blendAmount = fmin(blendAmount, 1.0f);
                animator.PlayAnimation(idleAnimation, crouchAnimation, idleAnimation,
                    animator.m_CurrentTime, animator.m_CurrentTime2, animator.m_lowerTime, blendAmount);
                if (blendAmount >= 1.0f) {
                    animator.PlayAnimation(crouchAnimation, NULL, idleAnimation,
                        animator.m_CurrentTime2, 0.0f, animator.m_lowerTime, 0.0f);
                    charState = CROUCH;
                }
            }
            break;
        case CROUCH:
            if (crouchAnimation) {
                animator.PlayAnimation(crouchAnimation, idleAnimation, idleAnimation,
                    animator.m_CurrentTime, 0.0f, animator.m_lowerTime, 0.0f);

                // If player releases down key, start transition to idle
                if (glfwGetKey(window, GLFW_KEY_DOWN) != GLFW_PRESS) {
                    charState = CROUCH_IDLE;
                    blendAmount = 0.0f;
                    frozenCrouchTime = animator.m_CurrentTime;
                }
            }
            break;

        case CROUCH_IDLE:
            if (crouchAnimation) {
                blendAmount += 0.1f;
                blendAmount = fmin(blendAmount, 1.0f);
                animator.PlayAnimation(crouchAnimation, idleAnimation, idleAnimation,
                    frozenCrouchTime, animator.m_CurrentTime2, animator.m_lowerTime, blendAmount);

                // Transition back to IDLE once blend is done
                if (blendAmount >= 1.0f) {
                    animator.PlayAnimation(idleAnimation, NULL, idleAnimation,
                        animator.m_CurrentTime2, 0.0f, animator.m_lowerTime, 0.0f);
                    charState = IDLE;
                }

                // Optional: if player presses down again during CROUCH_IDLE, restart crouch
                else if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) {
                    charState = CROUCH;
                }
            }
            break;

        }





        // Update animator
        if (charState == IDLE_MAGIC || charState == MAGIC || charState == MAGIC_IDLE)
        {
            animator.UpdateAnimation(deltaTime, legBones);
        }
        else
        {
            animator.UpdateAnimation(deltaTime, {});
        }

        // ----- UPDATE ORB MOVEMENT -----
        for (auto& orb : orbs) {
            if (!orb.alive) continue;

            orb.x += orb.speed * deltaTime;

            // destroy if too far
            if (orb.x > scenePosX + 3.5f)
                orb.alive = false;
        }

        // ----- ORB HIT DETECTION -----
        for (auto& orb : orbs) {
            if (!orb.alive) continue;

            for (size_t i = 0; i < stones.size(); ) {
                Stone& stone = stones[i];
                float stoneX = stone.x;
                float stoneY = 0.0f;                  // ground level
                float stoneWidth = 0.5f * stone.scale;  // half-width scaled
                float stoneHeight = stone.scale;  // height scaled

                float orbX = orb.x;
                float orbY = orb.y;
                float orbRadius = 0.1f; // adjust as needed

                // X overlap
                bool collideX = fabsf(stoneX - orbX) < (stoneWidth + orbRadius);

                // Y overlap
                bool collideY = (orbY < stoneY + stoneHeight) && (orbY + orbRadius > stoneY);

                if (collideX && collideY) {
                    stones.erase(stones.begin() + i); // remove stone
                    orb.alive = false;                // destroy orb
                }
                else {
                    ++i;
                }
            }
        }


        // --------------------------------

        // remove dead orbs
        orbs.erase(
            std::remove_if(orbs.begin(), orbs.end(),
                [](const Orb& o) { return !o.alive; }),
            orbs.end()
        );
        // -------------------------------

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

        //draw model
        glm::mat4 model = glm::mat4(1.0f);
        

        model = glm::translate(model, glm::vec3(scenePosX, posY, 0.0f));
        model = glm::rotate(model, glm::radians(90.0f), glm::vec3(0, 1, 0));
        model = glm::scale(model, glm::vec3(0.5f));
        ourShader.setMat4("model", model);

        ourModel.Draw(ourShader);

        //if (katana && charState == MAGIC) { // Only draw katana while slashing
        //    glm::mat4 boneMat = GetBoneMatrix(ourModel, animator, handBone);
        //    glm::mat4 katanaMat = model * boneMat;

        //    // Apply attachment transform
        //    katanaMat = glm::translate(katanaMat, katanaOffsetHand);
        //    katanaMat = glm::rotate(katanaMat, glm::radians(katanaRotHand.x), glm::vec3(1, 0, 0));
        //    katanaMat = glm::rotate(katanaMat, glm::radians(katanaRotHand.y), glm::vec3(0, 1, 0));
        //    katanaMat = glm::rotate(katanaMat, glm::radians(katanaRotHand.z), glm::vec3(0, 0, 1));
        //    katanaMat = glm::scale(katanaMat, katanaScale);

        //    ourShader.setMat4("model", katanaMat);
        //    for (size_t i = 0; i < katana->meshes.size(); ++i)
        //        katana->meshes[i].Draw(ourShader);
        //}




        for (auto& stone : stones) {
            if (stone.x > 0) {
                glm::mat4 stoneModelMat = glm::mat4(1.0f);

                // Translate to stone position
                stoneModelMat = glm::translate(stoneModelMat, glm::vec3(stone.x, -0.2f, 0.0f));

                // Scale according to random scale
                stoneModelMat = glm::scale(stoneModelMat, glm::vec3(stone.scale));

                // Set shader and draw
                ourShader.setMat4("model", stoneModelMat);

                // Use your mesh pointer, not the struct
                stoneModel->Draw(ourShader);  // <-- or whatever your Mesh* is
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

        orbShader.use();
        orbShader.setMat4("view", view);
        orbShader.setMat4("projection", projection);

        DrawOrbs(orbs, orbShader, lightingOrb, view, projection, camera.Position);

        //////////////////PICS
        // bind the background shader
        picShader.use();
        picShader.setMat4("view", view);
        picShader.setMat4("projection", projection);
        DrawBackgroundPic(picShader, quadVAO, bg, textureWidth, textureHeight,
            scenePosX, view, projection);


        glfwSwapBuffers(window);
        glfwPollEvents();
    }

    delete idleAnimation;
    delete shootMagicAnimation;
    glfwTerminate();
    return 0;
}

// Input handling
void processInput(GLFWwindow* window) {
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);
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
