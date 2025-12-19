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
#include <ctime>

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
    MENU, MENU_IDLE,
    IDLE,
    MAGIC, IDLE_MAGIC, MAGIC_IDLE,
    JUMP, IDLE_JUMP, JUMP_IDLE,
    CROUCH, IDLE_CROUCH, CROUCH_IDLE
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

//random
std::random_device rd;
std::mt19937 generator(rd());
std::bernoulli_distribution spawnDist(1.0); // 50% chance to spawn stone
std::bernoulli_distribution scaleDist(0.5); // 50% chance to pick 1.0, else 0.2

//items
enum ItemType { NONE, SPEAR, SHIELD };

struct Item {
    float x;
    ItemType type;
    bool collected = false;
};

Item currentItem;

// Spear Mode Flags
bool isSpearMode = false;
float spearTimer = 0.0f;
float autoFireTimer = 0.0f;
const float AUTO_FIRE_RATE = 0.3f; // Shoots every 0.3 seconds

// Shield Mode Flags
bool isShieldMode = false;
float shieldTimer = 0.0f;



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

void DrawModel(Model* model, Shader& shader, glm::vec3 coordinates, glm::vec3 scales, int index)
{
    glm::mat4 universalModel = glm::mat4(1.0f);
    universalModel = glm::translate(universalModel, coordinates);
    if (index > 0) {
        float angle = 137.508f * (float)index;

        universalModel = glm::rotate(universalModel, glm::radians(angle), glm::vec3(1, 0, 0)); // Rotate around Y (up)
    }
    universalModel = glm::rotate(universalModel, glm::radians(90.0f), glm::vec3(1, 0, 0));
    universalModel = glm::scale(universalModel, scales);

    shader.setMat4("model", universalModel);
    model->Draw(shader);
}

void DrawEnvironment(Model* model, Shader& ourShader, float spacing, float offset, float posY, float posZ, glm::vec3 scale, int index)
{
    float playerX = scenePosX;
    const int viewDistance = 20;

    int minRange = (int)floor(playerX) - viewDistance;
    int maxRange = (int)ceil(playerX) + viewDistance;

    int startX = (int)floor((float)minRange / spacing) * spacing;

    ourShader.use();

    for (int x = startX; x <= maxRange; x += (int)spacing) {
        float finalX = (float)x + offset;
        glm::vec3 pos = glm::vec3(finalX, posY, posZ);
        DrawModel(model, ourShader, pos, scale, index);
    }
}

void SpawnOrb()
{
    // spawn orb
    Orb orb;
    orb.x = scenePosX + 0.5f;
    orb.y = 0.2f;
    orb.z = 0.0f;
    orb.speed = 7.0f;
    orb.alive = true;
    orbs.push_back(orb);
}

void UpdateOrb()
{
    // ----- UPDATE ORB MOVEMENT -----
    for (auto& orb : orbs) {
        if (!orb.alive) continue;

        orb.x += orb.speed * deltaTime;

        // destroy if too far
        if (orb.x > scenePosX + 3.5f)
            orb.alive = false;
    }
}

void RemoveDeadOrb()
{
    orbs.erase(
        std::remove_if(orbs.begin(), orbs.end(),
            [](const Orb& o) { return !o.alive; }),
        orbs.end()
    );
}

void DetectOrbCollide(std::vector<Orb>& orbs, std::vector<Stone>& stones) {
    for (auto& orb : orbs) {
        if (!orb.alive) continue;

        for (size_t i = 0; i < stones.size(); ) {
            Stone& stone = stones[i];

            // Boundary constants (consider moving these to the Stone/Orb class)
            float stoneWidth = 0.5f * stone.scale;
            float stoneHeight = stone.scale;
            float orbRadius = 0.1f;
            float groundLevel = 0.0f;

            // Collision Logic
            bool collideX = fabsf(stone.x - orb.x) < (stoneWidth + orbRadius);
            bool collideY = (orb.y < groundLevel + stoneHeight) && (orb.y + orbRadius > groundLevel);

            if (collideX && collideY) {
                // Erase-remove pattern logic
                stones.erase(stones.begin() + i);
                orb.alive = false;

                // Once an orb hits one stone, it's dead. 
                // Stop checking other stones for this specific orb.
                break;
            }
            else {
                ++i;
            }
        }
    }
}

void DrawSprite(Shader& shader, unsigned int vao, unsigned int textureID,
    glm::vec3 position, glm::vec2 scale, float rotation,
    const glm::mat4& view, const glm::mat4& projection, bool followPlayer)
{
    shader.use();
    glm::mat4 model = glm::mat4(1.0f);

    if (followPlayer) {
        model = glm::translate(model, glm::vec3(scenePosX + position.x, position.y, position.z));
    }
    else {
        model = glm::translate(model, position);
    }

    model = glm::rotate(model, glm::radians(rotation), glm::vec3(0.0f, 0.0f, 1.0f));
    // Apply the aspect-corrected scale
    model = glm::scale(model, glm::vec3(scale.x, scale.y, 1.0f));

    shader.setMat4("view", view);
    shader.setMat4("projection", projection);
    shader.setMat4("model", model);

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, textureID);
    glBindVertexArray(vao);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

void StoneGeneration()
{
    stones.clear(); // Add this line to remove previous stones
    for (int x = 20; x <= 100; x += 5) {
        if (spawnDist(generator)) {
            Stone s;
            s.x = float(x) + ((rand() % 100) / 100.0f - 0.5f); // optional small random X offset ±0.5
            s.scale = scaleDist(generator) ? 1.0f : 0.2f;
            stones.push_back(s);
        }
    }
}

void ItemGeneration()
{
    // Reset the single item instance
    int itemChance = rand() % 100;
    if (itemChance < 33) {
        currentItem.x = 52.0f;
        currentItem.type = (rand() % 2 == 0) ? SPEAR : SHIELD;
        currentItem.collected = false;
    }
    else {
        currentItem.type = NONE; // No item spawned this lap
    }
}

bool CheckCollision(float pX, float pY, float pW, float pH,
    float oX, float oY, float oW, float oH) {
    // pX/pY = Player Center, pW/pH = Half-Width/Half-Height
    // oX/oY = Object Center, oW/oH = Half-Width/Half-Height

    bool collideX = fabsf(oX - pX) < (pW + oW);
    bool collideY = (pY < oY + oH) && (pY + pH > oY);

    return collideX && collideY;
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

    GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "Solnar Child", NULL, NULL);
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
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    Shader ourShader("anim_model.vs", "anim_model.fs");
    Shader picShader("bg_light.vs", "bg_light.fs");
    Shader orbShader("orbShader.vs", "orbShader.fs");

    // Resource paths
    const std::string modelPath = "resources/objects/angel/angel.dae";
    const std::string idlePath = "resources/objects/goth_katana/run.dae";
    const std::string shootMagicPath = "resources/objects/goth_katana/shoot.dae";
    const std::string jumpPath = "resources/objects/goth_katana/jump.dae";


    const std::string lightingOrbPath = "resources/objects/bullets/blue_orb/blue_orb.obj";
    Model* lightingOrb = fileExists(lightingOrbPath) ? new Model(lightingOrbPath) : nullptr;

    const std::string pillarPath = "resources/objects/church_bg/pillar.dae";
    Model* pillar = fileExists(pillarPath) ? new Model(pillarPath) : nullptr;

    const std::string stonePath = "resources/objects/church_bg/stones/stone_1/stone_1.dae";
    Model* stoneModel = fileExists(stonePath) ? new Model(stonePath) : nullptr;

    const std::string hydrengeaPath = "resources/objects/church_bg/hydrengea/hydrengea.dae";
    Model* hydrengeaModel = fileExists(hydrengeaPath) ? new Model(hydrengeaPath) : nullptr;

    const std::string floorPath = "resources/objects/church_bg/floor/tile_floor.dae";
    Model* floorModel = fileExists(floorPath) ? new Model(floorPath) : nullptr;

    const std::string spearPath = "resources/objects/items/sun_spear.dae";
    Model* spearModel = fileExists(spearPath) ? new Model(spearPath) : nullptr;

    const std::string shieldPath = "resources/objects/items/moon_shield.dae";
    Model* shieldModel = fileExists(shieldPath) ? new Model(shieldPath) : nullptr;

    stbi_set_flip_vertically_on_load(true);
    unsigned int bg = loadTexture("resources/textures/red_church.jpg");
    unsigned int logo = loadTexture("resources/textures/logo.png");
    int logoWidth = 2100;
    int logoHeight = 470;

    unsigned int instruction = loadTexture("resources/textures/instruction.png");
    int instructionWidth = 1136;
    int instructionHeight = 107;

    unsigned int moonMagic = loadTexture("resources/textures/moon_magic.png");
    unsigned int sunMagic = loadTexture("resources/textures/sun_magic.png");
    int magicWidth = 2048;
    int magicHeight = 2048;

    unsigned int overlayVine = loadTexture("resources/textures/overlay.png");
    unsigned int overlayMoon = loadTexture("resources/textures/overlay_moon.png");
    unsigned int overlaySun = loadTexture("resources/textures/overlay_sun.png");


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

    Animator animator(idleAnimation);

    float blendAmount = 0.0f;
    float blendRate = 0.055f;


    srand(static_cast<unsigned int>(time(NULL)));
    generator.seed(static_cast<unsigned int>(time(NULL)));
    //std::cout << "Game Seed: " << time(NULL) << std::endl;

    StoneGeneration();
    ItemGeneration();

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

    AnimState charState = MENU;

    // Render loop
    while (!glfwWindowShouldClose(window)) {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // --- Movement and State Logic ---
        if (charState == MENU) {
            // Model stays at x = 0
            scenePosX = 10.0f;
            camera.Position.x = 15.0f; // Camera fixed at 10

            // Press Z to transition
            if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS) {
                charState = MENU_IDLE;
            }
        }
        else if (charState == MENU_IDLE) {
            // Model starts moving
            scenePosX += speed * deltaTime;

            // Camera stays frozen at 10 until model reaches it
            if (scenePosX >= 14.0f) {
                charState = IDLE;
            }
                camera.Position.x = 15.0;

            
        }
        else {
            // Normal IDLE/Gameplay logic
            scenePosX += speed * deltaTime;
            camera.Position.x = scenePosX + 1.0f; // Camera follows model

            // Handle the warp-back logic from the previous step if desired
            if (scenePosX >= 90.0f) {
                scenePosX = 10.0f;
                StoneGeneration(); 
                ItemGeneration();
            }
        }

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
            if (CheckCollision(playerX, playerY, 0.2f, 1.0f, 
                stone.x, 0.0f, stone.scale, 0.25f * stone.scale)) {

                // If we have a shield, we are immune
                if (isShieldMode) {
                    continue;
                }

                charState = MENU;
                blendAmount = 0.0f;     // Reset the animation blending
                jumpEndY = 0.0f;        // Clear stored jump height
                isSpearMode = false;  // Turn off spear mode on death
                spearTimer = 0.0f;    // Reset timer
                animator.PlayAnimation(idleAnimation, NULL, idleAnimation, 0.0f, 0.0f, 0.0f, 0.0f);
                break;
            }
        }

        if (currentItem.type != NONE && !currentItem.collected) {
            if (CheckCollision(playerX, playerY, 0.2f, 1.0f,
                currentItem.x, -0.4f, 0.5f, 0.5f)) { // Item bounds
                currentItem.collected = true;
                if (currentItem.type == SPEAR) {
                    isSpearMode = true;
                    spearTimer = 10.0f;
                }
                else if (currentItem.type == SHIELD) { // Add this block
                    isShieldMode = true;
                    shieldTimer = 10.0f;
                }
            }
        }


        processInput(window);

        // --- Power-up Timer & Auto-Fire Logic ---
        if (isSpearMode) {
            spearTimer -= deltaTime;
            autoFireTimer += deltaTime;

            // Automatic Shooting every 0.3 seconds
            if (autoFireTimer >= AUTO_FIRE_RATE) {
                SpawnOrb();
                autoFireTimer = 0.0f;
            }

            // Power-up expires
            if (spearTimer <= 0.0f) {
                isSpearMode = false;
                spearTimer = 0.0f;
            }
        }

        // --- Shield Timer logic ---
        if (isShieldMode) {
            shieldTimer -= deltaTime;
            if (shieldTimer <= 0.0f) {
                isShieldMode = false;
                shieldTimer = 0.0f;
            }
        }


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
            break;

            // -------------------------------
            // MAGIC sequence (same as before)
        case IDLE_MAGIC:
            if (shootMagicAnimation) {
                if (blendAmount <= 0.05f) {
                    // spawn orb
                    SpawnOrb();
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
        case MENU:
            break;
        case MENU_IDLE:
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

        UpdateOrb();
        DetectOrbCollide(orbs, stones);
        RemoveDeadOrb();



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


        for (auto& stone : stones) {
            if (stone.x > 0) {
                glm::mat4 stoneModelMat = glm::mat4(1.0f);
                stoneModelMat = glm::translate(stoneModelMat, glm::vec3(stone.x, -0.4f, -0.2f));
                stoneModelMat = glm::rotate(stoneModelMat, glm::radians(90.0f), glm::vec3(0, 1, 0));
                stoneModelMat = glm::scale(stoneModelMat, glm::vec3(stone.scale));
                ourShader.setMat4("model", stoneModelMat);
                stoneModel->Draw(ourShader); 
            }
        }

        // --- Draw Item (Spear or Shield) ---
        if (currentItem.type != NONE && !currentItem.collected) {
            Model* modelToDraw = nullptr;
            if (currentItem.type == SPEAR) modelToDraw = spearModel;
            else if (currentItem.type == SHIELD) modelToDraw = shieldModel;

            if (modelToDraw) {
                // Position it at the ground level (same as stones)
                // Adjust the scale (0.3f) as needed for your model size
                DrawModel(modelToDraw, ourShader,
                    glm::vec3(currentItem.x, 0.5f, 0.0f),
                    glm::vec3(0.3f), 0);
            }
        }

        DrawModel(floorModel, ourShader, glm::vec3(0.0f, -0.7f, 0.5f), glm::vec3(1.0f, 1.0f, 1.0f), 0);
        DrawEnvironment(pillar, ourShader, 10.0f, 0.0f , 1.0f, -1.0f, glm::vec3(1.5f, 1.5f, 1.5f), 0);
        for (int i = 1; i < 10; i++)
        {
            DrawEnvironment(hydrengeaModel, ourShader, 10.0f, float(i), -0.2f, -1.0f, glm::vec3(1.0f, 0.5f, 0.5f), i);
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
        if (charState == MENU)
        {
            DrawSprite(picShader, quadVAO, logo, glm::vec3(camera.Position.x, 1.25f, -0.2f)
                , glm::vec2(1.0f * ((float)logoWidth / (float)logoHeight), 1.0f), 0.0f, view, projection, false);

            DrawSprite(picShader, quadVAO, instruction, glm::vec3(camera.Position.x, 0.5f, -0.5f)
                , glm::vec2(0.25f * ((float)instructionWidth / (float)instructionHeight), 0.25f), 0.0f, view, projection, false);
        }

        // --- Draw Magic Effects ---
        float magicAspect = (float)magicWidth / (float)magicHeight;
        glm::vec2 magicScale = glm::vec2(1.5f * magicAspect, 1.5f); // Scale size as needed

        // Moon Shield Effect (Centered on Player)
        if (isShieldMode) {
            float shieldPulse = (shieldTimer / 10.0f);
            glm::vec2 dynamicShieldScale = glm::vec2(1.5f * magicAspect * shieldPulse, 1.5f * shieldPulse);

            DrawSprite(picShader, quadVAO, moonMagic,
                glm::vec3(0.0f, posY + 0.5f, 0.1f),
                dynamicShieldScale, currentFrame * 100.0f, view, projection, true);
        }

        // Sun Spear Effect (Ahead of Player)
        if (isSpearMode) {
            float spearPulse = glm::clamp(spearTimer / 3.0f, 0.0f, 1.0f);
            glm::vec2 dynamicSpearScale = glm::vec2(1.5f * magicAspect * spearPulse, 1.5f * spearPulse);

            DrawSprite(picShader, quadVAO, sunMagic,
                glm::vec3(0.5f, posY + 0.5f, 0.1f),
                dynamicSpearScale, 0.0f, view, projection, true);
        }

        // --- Draw Mode Overlays ---
        float screenAspect = (float)SCR_WIDTH / (float)SCR_HEIGHT;
        glm::vec2 overlayScale = glm::vec2(2.5f * screenAspect, 2.5f);

        if (isShieldMode) {
            // Draw Moon Overlay when Shield is active
            DrawSprite(picShader, quadVAO, overlayMoon,
                glm::vec3(camera.Position.x, 0.5f, -0.01f),
                overlayScale, 0.0f, view, projection, false);
        }
        else if (isSpearMode) {
            // Draw Sun Overlay when Spear is active
            DrawSprite(picShader, quadVAO, overlaySun,
                glm::vec3(camera.Position.x, 0.5f, -0.01f),
                overlayScale, 0.0f, view, projection, false);
        }
        else {
            // Optional: Draw your original vines when no mode is active
            DrawSprite(picShader, quadVAO, overlayVine,
                glm::vec3(camera.Position.x, 0.5f, -0.01f),
                overlayScale, 0.0f, view, projection, false);
        }



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