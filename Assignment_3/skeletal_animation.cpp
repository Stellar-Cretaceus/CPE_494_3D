#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <learnopengl/filesystem.h>
#include <learnopengl/shader_m.h>
#include <learnopengl/camera.h>
#include <learnopengl/animator.h>
#include <learnopengl/model_animation.h>



#include <iostream>


void framebuffer_size_callback(GLFWwindow* window, int width, int height);
void mouse_callback(GLFWwindow* window, double xpos, double ypos);
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset);
void processInput(GLFWwindow* window);
unsigned int loadTexture(const char* path);

// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;
const unsigned int textureWidth = 1920;
const unsigned int textureHeight = 1440;

// camera
Camera camera(glm::vec3(0.0f, 0.0f, 2.0f));
glm::vec3 modelPosition = glm::vec3(0.0f, -1.0f, 0.0f);
float modelYaw = 180.0f; // your model already rotated
float lastX = SCR_WIDTH / 2.0f;
float lastY = SCR_HEIGHT / 2.0f;
bool firstMouse = true;

// light
glm::vec3 lightPosition = glm::vec3(0.0f, 5.0f, 5.0f);

// bullets
float spawnInterval = 0.5f;  // 1 second
float spawnTimer = 0.0f;
float rotationAngle = 0.0f;  // rotation around Z axis (XY plane)
int bulletCount = 20;         // number of bullets per spawn
float radius = 10.0f;         // spawn radius

float radiusMax = 10.0f;      // maximum radius
float radiusMin = 0.0f;       // minimum radius
float radiusTimer = 0.0f;
bool radiusShrinking = true;  // true = shrink to 0, false = expand to 10
float radiusShrinkDuration = 6.0f;  // seconds for shrink or expand
float radiusExpandDuration = 6.0f;  // seconds for shrink or expand

// timing
float deltaTime = 0.0f;
float lastFrame = 0.0f;

//hitbox
float playerRadius = 0.2f;   // approximate player hitbox
float bulletRadius = 0.1f;   // your bullet scale



struct Bullet {
	glm::vec3 position;
	float speed;
	bool active;

	// rotation info
	float orbitRadius;       // distance from spawn center
	float orbitAngle;        // current angle in radians
	float orbitSpeed;        // radians/sec
	int orbitDirection;      // 1 = clockwise, -1 = counterclockwise
	glm::vec3 spawnCenter;   // center around which it rotates
};

std::vector<Bullet> bullets;





int main()
{
	// glfw: initialize and configure
	// ------------------------------
	glfwInit();
	glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
	glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
	glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
	glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

	// glfw window creation
	// --------------------
	GLFWwindow* window = glfwCreateWindow(SCR_WIDTH, SCR_HEIGHT, "LearnOpenGL", NULL, NULL);
	if (window == NULL)
	{
		std::cout << "Failed to create GLFW window" << std::endl;
		glfwTerminate();
		return -1;
	}
	glfwMakeContextCurrent(window);
	glfwSetFramebufferSizeCallback(window, framebuffer_size_callback);

	// tell GLFW to capture our mouse
	glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

	// glad: load all OpenGL function pointers
	// ---------------------------------------
	if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
	{
		std::cout << "Failed to initialize GLAD" << std::endl;
		return -1;
	}

	// tell stb_image.h to flip loaded texture's on the y-axis (before loading model).
	stbi_set_flip_vertically_on_load(true);

	// configure global opengl state
	// -----------------------------
	glEnable(GL_DEPTH_TEST);

	// build and compile shaders
	// -------------------------
	Shader ourShader("anim_model.vs", "anim_model.fs");
	Shader bulletShader("orbShader.vs", "orbShader.fs");
	Shader backgroundShader("backgroundShader.vs", "backgroundShader.fs");
	Shader picShader("bg_light.vs", "bg_light.fs");

	
	// load models
	// -----------
	Model ourModel(FileSystem::getPath("resources/objects/goth_katana/goth_katana.dae"));
	Animation danceAnimation(FileSystem::getPath("resources/objects//goth_katana/goth_katana.dae"),&ourModel);
	Animator animator(&danceAnimation);

	Model bulletModel(FileSystem::getPath("resources/objects/bullets/blue_orb/blue_orb.obj"));

	Model background(FileSystem::getPath("resources/objects/vaporwave_bg/vaporwave_bg.obj"));

	stbi_set_flip_vertically_on_load(true);
	unsigned int bg = loadTexture(FileSystem::getPath("resources/textures/space.jpg").c_str());

	// Spawn initial bullets once
	// Spawn bullets
	if (bullets.empty()) {
		int bulletCount = 20;          // how many bullets
		float radius = 10.0f;          // radius of the circle

		for (int i = 0; i < bulletCount; i++) {
			float angle = 2.0f * glm::pi<float>() * i / bulletCount + rotationAngle;
			float xOffset = cos(angle) * radius;
			float yOffset = sin(angle) * radius;

			Bullet b;
			b.spawnCenter = modelPosition + glm::vec3(0.0f, 0.0f, -50.0f);
			b.position = b.spawnCenter + glm::vec3(xOffset, yOffset, 0.0f);
			b.speed = 8.0f;
			b.active = true;
			b.orbitRadius = radius;
			b.orbitAngle = angle;

			// alternate clockwise/counterclockwise
			b.orbitDirection = (i % 2 == 0) ? 1 : -1;
			b.orbitSpeed = glm::radians(10.0f); // 90 deg/sec, tweak as needed

			bullets.push_back(b);
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






	// draw in wireframe
	//glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);

	// render loop
	// -----------
	while (!glfwWindowShouldClose(window))
	{
		// per-frame time logic
		// --------------------
		float currentFrame = glfwGetTime();
		deltaTime = currentFrame - lastFrame;
		lastFrame = currentFrame;

		// input
		// -----
		processInput(window);
		animator.UpdateAnimation(deltaTime);


		camera.Position = modelPosition + glm::vec3(0.0f, 1.0f, 5.0f);
		camera.Front = glm::normalize(modelPosition - camera.Position);
		camera.Up = glm::vec3(0.0f, 1.0f, 0.0f); // world up

		// render
		// ------
		glClearColor(0.05f, 0.05f, 0.05f, 1.0f);
		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);


		//////////////////////////// BACKGROUND
		// --- render background first ---
		backgroundShader.use();
		backgroundShader.setVec3("viewPos", camera.Position);
		backgroundShader.setFloat("emissionStrength", 1.0f);

		glm::mat4 projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		glm::mat4 view = camera.GetViewMatrix();
		glm::mat4 bgModel = glm::mat4(1.0f);
		bgModel = glm::translate(bgModel, glm::vec3(0.0f, -10.0f, -10.0f));
		bgModel = glm::scale(bgModel, glm::vec3(0.1f)); // shrink a lot
		backgroundShader.setMat4("model", bgModel);
		backgroundShader.setMat4("view", camera.GetViewMatrix());
		backgroundShader.setMat4("projection", projection);
		for (unsigned int i = 0; i < background.meshes.size(); i++)
		{
			backgroundShader.setVec3("objectColor", glm::vec3(0.1f, 0.1f, 0.1f)); // dark gray
			backgroundShader.setFloat("emissionStrength", 1.0f);
			background.meshes[i].Draw(backgroundShader);
		}




		//////////////////////////////

		bulletShader.use();
		bulletShader.setVec3("objectColor", glm::vec3(0.2f, 0.5f, 1.0f)); // base blue
		bulletShader.setVec3("lightColor", glm::vec3(1.0f));
		bulletShader.setVec3("lightPos", lightPosition);
		bulletShader.setVec3("viewPos", camera.Position);

		// Glow settings (BLUE glow)
		bulletShader.setVec3("emissionColor", glm::vec3(0.2f, 0.5f, 1.0f)); // blue
		bulletShader.setFloat("emissionStrength", 1.2f); // bump this for more glow


		// view/projection transformations
		projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		view = camera.GetViewMatrix();
		bulletShader.setMat4("projection", projection);
		bulletShader.setMat4("view", view);

		////////////////////////// BULLETS ////////////////////////////////
		// --- Update radius linearly with different durations ---
		radiusTimer += deltaTime;

		// pick duration based on shrinking or expanding
		float currentDuration = radiusShrinking ? radiusShrinkDuration : radiusExpandDuration;
		float t = radiusTimer / currentDuration; // 0 1 over currentDuration

		if (radiusShrinking) {
			radius = glm::mix(radiusMax, radiusMin, t); // shrink 10 0
		}
		else {
			radius = glm::mix(radiusMin, radiusMax, t); // expand 0 10
		}

		// reset timer and reverse direction when done
		if (radiusTimer >= currentDuration) {
			radiusTimer = 0.0f;
			radiusShrinking = !radiusShrinking;
		}


		spawnTimer += deltaTime;
		rotationAngle += glm::radians(45.0f) * deltaTime; // rotate 45 degrees per second
		if (rotationAngle > glm::two_pi<float>()) rotationAngle -= glm::two_pi<float>();

		// --- Spawn bullets if timer reached ---
		if (spawnTimer >= spawnInterval) {
			spawnTimer = 0.0f;

			for (int i = 0; i < bulletCount; i++) {
				float angle = 2.0f * glm::pi<float>() * i / bulletCount + rotationAngle;
				float xOffset = cos(angle) * radius;
				float yOffset = sin(angle) * radius;

				Bullet b;
				b.spawnCenter = modelPosition + glm::vec3(0.0f, 0.0f, -50.0f); // <--- important
				b.position = b.spawnCenter + glm::vec3(xOffset, yOffset, 0.0f); // use spawnCenter
				b.speed = 8.0f;
				b.active = true;

				b.orbitRadius = radius;
				b.orbitAngle = angle;

				b.orbitDirection = (i % 2 == 0) ? 1 : -1; // alternate CW/CCW
				b.orbitSpeed = glm::radians(10.0f);

				bullets.push_back(b);
			}
		}



		// --- Update bullet positions ---
		for (auto& b : bullets) {
			if (!b.active) continue;

			// move forward along Z
			b.position.z += b.speed * deltaTime;

			// orbit in XY plane
			b.orbitAngle += b.orbitSpeed * deltaTime * b.orbitDirection;
			b.position.x = b.spawnCenter.x + cos(b.orbitAngle) * b.orbitRadius;
			b.position.y = b.spawnCenter.y + sin(b.orbitAngle) * b.orbitRadius;

			if (b.position.z > modelPosition.z + 20.0f) // bullets moved past player
				b.active = false;

			// --- Collision check ---
			float distance = glm::length(b.position - modelPosition);
			if (distance < (playerRadius + bulletRadius)) {
				std::cout << "You died! Collided with a bullet.\n";
				glfwTerminate(); // close program immediately
				exit(0);         // ensure termination

			}
		}

		/////
		// draw bullets
		for (auto& b : bullets) {
			if (!b.active) continue;

			glm::mat4 bulletMat = glm::mat4(1.0f);
			bulletMat = glm::translate(bulletMat, b.position);
			bulletMat = glm::scale(bulletMat, glm::vec3(0.1f));

			bulletShader.setMat4("model", bulletMat);
			bulletModel.Draw(bulletShader);
		}

		///////////////
		// 
	
		// don't forget to enable shader before setting uniforms
		ourShader.use();

		// view/projection transformations
		projection = glm::perspective(glm::radians(camera.Zoom), (float)SCR_WIDTH / (float)SCR_HEIGHT, 0.1f, 100.0f);
		view = camera.GetViewMatrix();
		ourShader.setMat4("projection", projection);
		ourShader.setMat4("view", view);


		auto transforms = animator.GetFinalBoneMatrices();
		for (int i = 0; i < transforms.size(); ++i)
			ourShader.setMat4("finalBonesMatrices[" + std::to_string(i) + "]", transforms[i]);
		
		// render the loaded model
		glm::mat4 model = glm::mat4(1.0f);
		model = glm::translate(model, modelPosition);
		// First rotate model 90 about Y axis (so its side faces camera)
		model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));
		model = glm::rotate(model, glm::radians(-30.0f), glm::vec3(1.0f, 0.0f, 0.0f));
		model = glm::scale(model, glm::vec3(.5f, .5f, .5f));	// it's a bit too big for our scene, so scale it down
		ourShader.setMat4("model", model);
		ourModel.Draw(ourShader);


		//////////////////PICS
		// bind the background shader
		picShader.use();
		picShader.setMat4("view", view);
		picShader.setMat4("projection", projection);

		model = glm::mat4(1.0f);

		float scaleX = 100.0f; // base width in world units
		float scaleY = 100.0f; // base height in world units

		float aspect = (float)textureWidth / (float)textureHeight;
		model = glm::translate(glm::mat4(1.0f), modelPosition + glm::vec3(0.0f, 0.0f, -70.0f));
		model = glm::scale(model, glm::vec3(aspect * scaleX, scaleY, 4.0f));
		picShader.setMat4("model", model);

		glBindVertexArray(quadVAO);

		glActiveTexture(GL_TEXTURE0);
		glBindTexture(GL_TEXTURE_2D, bg);
		picShader.setInt("screenTexture", 0);
		glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);





		// glfw: swap buffers and poll IO events (keys pressed/released, mouse moved etc.)
		// -------------------------------------------------------------------------------
		glfwSwapBuffers(window);
		glfwPollEvents();
	}

	// glfw: terminate, clearing all previously allocated GLFW resources.
	// ------------------------------------------------------------------
	glfwTerminate();
	return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow* window)
{
	const float speed = 3.0f * deltaTime;

	// --- Movement (XZ) ---
	if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
		modelPosition.z -= speed;
	if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
		modelPosition.z += speed;
	if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
		modelPosition.x -= speed;
	if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
		modelPosition.x += speed;

	// --- Up/Down movement (Y axis) ---
	if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
		modelPosition.y += speed;
	if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
		modelPosition.y -= speed;

	// --- Auto forward movement (always pushing player forward) ---
	const float autoSpeed = 2.0f * deltaTime;   // tweak this value
	modelPosition.z -= autoSpeed;               // push forward (negative Z)


	// --- ESC to close game ---
	if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
		glfwSetWindowShouldClose(window, true);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
	// make sure the viewport matches the new window dimensions; note that width and 
	// height will be significantly larger than specified on retina displays.
	glViewport(0, 0, width, height);
}

// glfw: whenever the mouse scroll wheel scrolls, this callback is called
// ----------------------------------------------------------------------
void scroll_callback(GLFWwindow* window, double xoffset, double yoffset)
{
	camera.ProcessMouseScroll(yoffset);
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
