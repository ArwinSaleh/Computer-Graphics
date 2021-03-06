
#include <GL/glew.h>

#include <sstream>
#include <cstdlib>
#include <algorithm>
#include <chrono>

#include <labhelper.h>
#include <imgui.h>
#include <imgui_impl_sdl_gl3.h>
#include <Model.h>

#include <glm/glm.hpp>
#include <glm/gtx/transform.hpp>
using namespace glm;
using namespace labhelper;

using std::min;
using std::max;

int old_w = 1280;
int old_h = 720;

struct PerspectiveParams
{
	float fov;
	int w;
	int h;
	float near;
	float far;
};

PerspectiveParams pp = { 45.0f, 1280, 720, 0.1f, 300.0f };

// The window we'll be rendering to
SDL_Window* g_window = nullptr;

GLuint shaderProgram;

// Mouse input
ivec2 g_prevMouseCoords = { -1, -1 };
bool g_isMouseDragging = false;

float currentTime = 0.0f;
float deltaTime = 0.0f;
bool showUI = false;
bool drivingForward = false;
bool drivingBackward = false;

// Models
Model* cityModel = nullptr;
Model* carModel = nullptr;
Model* groundModel = nullptr;
mat4 carModelMatrix(1.0f);
mat4 carModelMatrix2(1.0f);

vec3 worldUp = vec3(0.0f, 1.0f, 0.0f);

// Camera parameters
vec3 cameraPosition(15.0f, 15.0f, 15.0f);
vec3 cameraDirection(-1.0f, -1.0f, -1.0f);
mat4 T(1.0f), R(1.0f);
mat4 T2(1.0f), R2(1.0f);	// TASK 3

void loadModels()
{
	///////////////////////////////////////////////////////////////////////////
	// Load models (both vertex buffers and textures).
	///////////////////////////////////////////////////////////////////////////
	cityModel = loadModelFromOBJ("../scenes/city.obj");
	carModel = loadModelFromOBJ("../scenes/car.obj");
	groundModel = loadModelFromOBJ("../scenes/ground_plane.obj");
}

void drawGround(mat4 mvpMatrix)
{
	mvpMatrix[3] += vec4(0, 0.0005, 0, 0);
	int loc = glGetUniformLocation(shaderProgram, "modelViewProjectionMatrix");
	glUniformMatrix4fv(loc, 1, false, &mvpMatrix[0].x);
	render(groundModel);
}

void display()
{
	// Set up
	int w, h;
	SDL_GetWindowSize(g_window, &w, &h);

	if (pp.w != old_w || pp.h != old_h)
	{
		SDL_SetWindowSize(g_window, pp.w, pp.h);
		w = pp.w;
		h = pp.h;
		old_w = pp.w;
		old_h = pp.h;
	}

	glViewport(0, 0, w, h);                             // Set viewport
	glClearColor(0.2f, 0.2f, 0.8f, 1.0f);               // Set clear color
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT); // Clears the color buffer and the z-buffer
	glEnable(GL_DEPTH_TEST);                            // enable Z-buffering
	glDisable(GL_CULL_FACE);                            // disables not showing back faces of triangles

	// Set the shader program to use for this draw call
	glUseProgram(shaderProgram);

	// Set up model matrices
	mat4 cityModelMatrix(1.0f);

	// Set up the view matrix
	// The view matrix defines where the viewer is looking
	// Initially fixed, but will be replaced in the tutorial.
	/*
	mat4 constantViewMatrix = mat4(0.707106769f, -0.408248276f, 1.00000000f, 0.000000000f, 0.000000000f,
		0.816496551f, 1.00000000f, 0.000000000f, -0.707106769f, -0.408248276f,
		1.00000000f, 0.000000000f, 0.000000000f, 0.000000000f, -30.0000000f,
		1.00000000f);
	mat4 viewMatrix = constantViewMatrix;
	*/

	// use camera direction as -z axis and compute the x (cameraRight) and y (cameraUp) base vectors
	vec3 cameraRight = normalize(cross(cameraDirection, worldUp));
	vec3 cameraUp = normalize(cross(cameraRight, cameraDirection));

	mat3 cameraBaseVectorsWorldSpace(cameraRight, cameraUp, -cameraDirection);

	mat4 cameraRotation = mat4(transpose(cameraBaseVectorsWorldSpace));
	mat4 viewMatrix = cameraRotation * translate(-cameraPosition);
	//mat4 viewMatrix = carModelMatrix * translate(-cameraPosition);


	// Setup the projection matrix
	if (w != old_w || h != old_h)
	{
		pp.h = h;
		pp.w = w;
		old_w = w;
		old_h = h;
	}
	mat4 projectionMatrix = perspective(radians(pp.fov), float(pp.w) / float(pp.h), pp.near, pp.far);

	// Concatenate the three matrices and pass the final transform to the vertex shader

	// City
	mat4 modelViewProjectionMatrix = projectionMatrix * viewMatrix * cityModelMatrix;
	int loc = glGetUniformLocation(shaderProgram, "modelViewProjectionMatrix");
	glUniformMatrix4fv(loc, 1, false, &modelViewProjectionMatrix[0].x);
	render(cityModel);

	// Ground
	// TASK 5: Uncomment this
	drawGround(modelViewProjectionMatrix);

	// check keyboard state (which keys are still pressed)
	const uint8_t* state = SDL_GetKeyboardState(nullptr);


	/////////////////////////////////////////////////////////////////////////
	////////////////////////////////TASK 2///////////////////////////////////
	/////////////////////////////////////////////////////////////////////////

	const float rotateSpeed = 2.f;
	if (state[SDL_SCANCODE_LEFT] && (drivingBackward || drivingForward)) {
		R[0] -= rotateSpeed * deltaTime * R[2];
	}
	if (state[SDL_SCANCODE_RIGHT] && (drivingBackward || drivingForward)) {
		R[0] += rotateSpeed * deltaTime * R[2];
	}

	// Make R orthonormal again
	R[0] = normalize(R[0]);
	R[2] = vec4(cross(vec3(R[0]), vec3(R[1])), 0.0f);

	/////////////////////////////////////////////////////////////////////////
	///////////////////////////////END TASK 2////////////////////////////////
	/////////////////////////////////////////////////////////////////////////


	/////////////////////////////////////////////////////////////////////////
	////////////////////////////////TASK 1///////////////////////////////////
	/////////////////////////////////////////////////////////////////////////

	// implement controls based on key states
	const float speed = 10.f;
	if (state[SDL_SCANCODE_UP]) {
		T[3] += speed * deltaTime * R * vec4(0.0f, 0.0f, 1.0f, 0.0f);	// R * V for task 2
		drivingForward = true;
	}
	else
	{
		drivingForward = false;
	}

	if (state[SDL_SCANCODE_DOWN]) {
		T[3] -= speed * deltaTime * R * vec4(0.0f, 0.0f, 1.0f, 0.0f);	// R * V for task 2
		drivingBackward = true;
	}
	else
	{
		drivingBackward = false;
	}
	/*
	if (state[SDL_SCANCODE_LEFT]) {
		T[3] += speed * deltaTime * vec4(1.0f, 0.0f, 0.0f, 0.0f);	// Removed for task 2
	}
	if (state[SDL_SCANCODE_RIGHT]) {
		T[3] -= speed * deltaTime * vec4(1.0f, 0.0f, 0.0f, 0.0f);	// Removed for task 2
	}
	*/
	/////////////////////////////////////////////////////////////////////////
	///////////////////////////////END TASK 1////////////////////////////////
	/////////////////////////////////////////////////////////////////////////


	carModelMatrix = T * R;

	// car
	modelViewProjectionMatrix = projectionMatrix * viewMatrix * carModelMatrix;
	glUniformMatrix4fv(loc, 1, false, &modelViewProjectionMatrix[0].x);
	render(carModel);

	/////////////////////////////////////////////////////////////////////////
	////////////////////////////////TASK 3///////////////////////////////////
	/////////////////////////////////////////////////////////////////////////

	R2 = rotate(rotateSpeed * currentTime, vec3(0.0f, 1.0f, 0.0f));
	T2 = translate(5.0f * vec3(-1.0f, 0.0f, 0.0f));

	carModelMatrix2 = R2 * T2 * scale(vec3(2.0f));

	// car
	modelViewProjectionMatrix = projectionMatrix * viewMatrix * carModelMatrix2;
	glUniformMatrix4fv(loc, 1, false, &modelViewProjectionMatrix[0].x);
	render(carModel);

	/////////////////////////////////////////////////////////////////////////
	///////////////////////////////END TASK 3////////////////////////////////
	/////////////////////////////////////////////////////////////////////////


	glUseProgram(0);
}

void gui()
{
	// Inform imgui of new frame
	ImGui_ImplSdlGL3_NewFrame(g_window);

	// ----------------- Set variables --------------------------
	ImGui::SliderFloat("Field Of View", &pp.fov, 1.0f, 180.0f, "%.0f");
	ImGui::SliderInt("Width", &pp.w, 256, 1920);
	ImGui::SliderInt("Height", &pp.h, 256, 1080);
	ImGui::Text("Aspect Ratio: %.2f", float(pp.w) / float(pp.h));
	ImGui::SliderFloat("Near Plane", &pp.near, 0.1f, 300.0f, "%.2f", 2.f);
	ImGui::SliderFloat("Far Plane", &pp.far, 0.1f, 300.0f, "%.2f", 2.f);
	if (ImGui::Button("Reset"))
	{
		pp.fov = 45.0f;
		pp.w = 1280;
		pp.h = 720;
		pp.near = 0.1f;
		pp.far = 300.0f;
	}
	ImGui::Text("Application average %.3f ms/frame (%.1f FPS)", 1000.0f / ImGui::GetIO().Framerate,
		ImGui::GetIO().Framerate);
	// ----------------------------------------------------------

	// Render the GUI.
	ImGui::Render();
}

int main(int argc, char* argv[])
{
	g_window = labhelper::init_window_SDL("OpenGL Lab 3");

	// Load shader program
	shaderProgram = labhelper::loadShaderProgram("../lab3-camera/simple.vert", "../lab3-camera/simple.frag");

	// Load models
	loadModels();

	// render-loop
	bool stopRendering = false;
	auto startTime = std::chrono::system_clock::now();

	while (!stopRendering)
	{
		// update currentTime
		std::chrono::duration<float> timeSinceStart = std::chrono::system_clock::now() - startTime;
		deltaTime = timeSinceStart.count() - currentTime;
		currentTime = timeSinceStart.count();

		// render to window
		display();

		// Render overlay GUI.
		if (showUI)
		{
			gui();
		}

		// Swap front and back buffer. This frame will now been displayed.
		SDL_GL_SwapWindow(g_window);

		// check new events (keyboard among other)
		SDL_Event event;
		while (SDL_PollEvent(&event))
		{
			// Allow ImGui to capture events.
			ImGui_ImplSdlGL3_ProcessEvent(&event);

			// More info at https://wiki.libsdl.org/SDL_Event
			if (event.type == SDL_QUIT || (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_ESCAPE))
			{
				stopRendering = true;
			}
			if (event.type == SDL_KEYUP && event.key.keysym.sym == SDLK_g)
			{
				showUI = !showUI;
			}
			else if (event.type == SDL_MOUSEBUTTONDOWN && event.button.button == SDL_BUTTON_LEFT
				&& (!showUI || !ImGui::GetIO().WantCaptureMouse))
			{
				g_isMouseDragging = true;
				int x;
				int y;
				SDL_GetMouseState(&x, &y);
				g_prevMouseCoords.x = x;
				g_prevMouseCoords.y = y;
			}

			if (!(SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT)))
			{
				g_isMouseDragging = false;
			}
			int delta_x = 0;
			int delta_y = 0;
			if (event.type == SDL_MOUSEMOTION && g_isMouseDragging)
			{
				// More info at https://wiki.libsdl.org/SDL_MouseMotionEvent
				delta_x = event.motion.x - g_prevMouseCoords.x;
				delta_y = event.motion.y - g_prevMouseCoords.y;
				if (event.button.button == SDL_BUTTON_LEFT)
				{
					printf("Mouse motion while left button down (%i, %i)\n", event.motion.x, event.motion.y);
				}
				g_prevMouseCoords.x = event.motion.x;
				g_prevMouseCoords.y = event.motion.y;
			}
		/////////////////////////////////////////////////////////////////////////
		////////////////////////////////TASK 4///////////////////////////////////
		/////////////////////////////////////////////////////////////////////////

			if (SDL_GetMouseState(NULL, NULL) & SDL_BUTTON(SDL_BUTTON_LEFT))
			{
				float rotationSpeed = 0.005f;
				mat4 yaw = rotate(rotationSpeed * -delta_x, worldUp);
				mat4 pitch = rotate(rotationSpeed * -delta_y, normalize(cross(cameraDirection, worldUp)));
				cameraDirection = vec3(pitch * yaw * vec4(cameraDirection, 0.0f));
			}
		}

		vec3 cameraPerpendicular = normalize(cross(cameraDirection, worldUp));
		const float depthCoeff = 0.1f;
		const float sideCoeff = 0.2f;
		// check keyboard state (which keys are still pressed)
		const uint8_t* state = SDL_GetKeyboardState(nullptr);

		if (state[SDL_SCANCODE_W])
		{
			cameraPosition += depthCoeff * cameraDirection;
		}
		if (state[SDL_SCANCODE_S])
		{
			cameraPosition -= depthCoeff * cameraDirection;
		}
		if (state[SDL_SCANCODE_D])
		{

			cameraPosition += sideCoeff * cameraPerpendicular;
		}
		if (state[SDL_SCANCODE_A])
		{
			cameraPosition -= sideCoeff * cameraPerpendicular;
		}
		if (state[SDL_SCANCODE_SPACE])
		{
			cameraPosition += sideCoeff * worldUp;
		}
		if (state[SDL_SCANCODE_C])
		{
			cameraPosition -= sideCoeff * worldUp;
		}

		/////////////////////////////////////////////////////////////////////////
		///////////////////////////////END TASK 4////////////////////////////////
		/////////////////////////////////////////////////////////////////////////
	}

	// Shut down everything. This includes the window and all other subsystems.
	labhelper::shutDown(g_window);
	return 0;
}
