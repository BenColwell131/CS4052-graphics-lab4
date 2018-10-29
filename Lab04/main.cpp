// Windows includes (For Time, IO, etc.)
#include <windows.h>
#include <mmsystem.h>
#include <iostream>
#include <string>
#include <stdio.h>
#include <math.h>
#include <vector> // STL dynamic memory.

// OpenGL includes
#include <GL/glew.h>
#include <GL/freeglut.h>


// GLM includes
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// Assimp includes
#include <assimp/cimport.h> // scene importer
#include <assimp/scene.h> // collects data
#include <assimp/postprocess.h> // various extra operations

// Project includes
//#include "maths_funcs.h"



/*----------------------------------------------------------------------------
MESH TO LOAD
----------------------------------------------------------------------------*/
// this mesh is a dae file format but you should be able to use any other format too, obj is typically what is used
// put the mesh in your project directory, or provide a filepath for it here
//#define MESH_NAME "monkeyhead_smooth.dae"
#define PUMPKIN_MESH "Halloween_Pumpkin.dae"
#define SKULL_MESH "skull.dae"
/*----------------------------------------------------------------------------
----------------------------------------------------------------------------*/

#pragma region SimpleTypes
typedef struct
{
	size_t mPointCount = 0;
	std::vector<glm::vec3> mVertices;
	std::vector<glm::vec3> mNormals;
	std::vector<glm::vec2> mTextureCoords;
} ModelData;
#pragma endregion SimpleTypes

using namespace std;

// Constants
const float ASPECT_RATIO = 0.75f;

const float WINDOW_WIDTH = 800.0f;
const float WINDOW_HEIGHT = WINDOW_WIDTH * ASPECT_RATIO;

// Global variables
GLuint shaderProgramID;
ModelData mesh_data[2];
unsigned int mesh_vaos[2];

unsigned int vp_vbo[2];
unsigned int vn_vbo[2];

GLuint loc1[2], loc2[2], loc3[2];
GLfloat rotate_y = 0.0f;

glm::mat4 model;


#pragma region MESH LOADING
/*----------------------------------------------------------------------------
MESH LOADING FUNCTION
----------------------------------------------------------------------------*/

ModelData load_mesh(const char* file_name) {
	ModelData modelData;

	/* Use assimp to read the model file, forcing it to be read as    */
	/* triangles. The second flag (aiProcess_PreTransformVertices) is */
	/* relevant if there are multiple meshes in the model file that   */
	/* are offset from the origin. This is pre-transform them so      */
	/* they're in the right position.                                 */
	const aiScene* scene = aiImportFile(
		file_name, 
		aiProcess_Triangulate | aiProcess_PreTransformVertices
	); 

	if (!scene) {
		fprintf(stderr, "ERROR: reading mesh %s\n", file_name);
		return modelData;
	}

	printf("  %i materials\n", scene->mNumMaterials);
	printf("  %i meshes\n", scene->mNumMeshes);
	printf("  %i textures\n", scene->mNumTextures);

	for (unsigned int m_i = 0; m_i < scene->mNumMeshes; m_i++) {
		const aiMesh* mesh = scene->mMeshes[m_i];
		printf("    %i vertices in mesh\n", mesh->mNumVertices);
		modelData.mPointCount += mesh->mNumVertices;
		for (unsigned int v_i = 0; v_i < mesh->mNumVertices; v_i++) {
			if (mesh->HasPositions()) {
				const aiVector3D* vp = &(mesh->mVertices[v_i]);
				modelData.mVertices.push_back(glm::vec3(vp->x, vp->y, vp->z));
			}
			if (mesh->HasNormals()) {
				const aiVector3D* vn = &(mesh->mNormals[v_i]);
				modelData.mNormals.push_back(glm::vec3(vn->x, vn->y, vn->z));
			}
			if (mesh->HasTextureCoords(0)) {
				const aiVector3D* vt = &(mesh->mTextureCoords[0][v_i]);
				modelData.mTextureCoords.push_back(glm::vec2(vt->x, vt->y));
			}
			if (mesh->HasTangentsAndBitangents()) {
				/* You can extract tangents and bitangents here              */
				/* Note that you might need to make Assimp generate this     */
				/* data for you. Take a look at the flags that aiImportFile  */
				/* can take.                                                 */
			}
		}
	}

	aiReleaseImport(scene);
	return modelData;
}

#pragma endregion MESH LOADING


// Shader Functions- click on + to expand
#pragma region SHADER_FUNCTIONS
char* readShaderSource(const char* shaderFile) {
	FILE* fp;
	fopen_s(&fp, shaderFile, "rb");

	if (fp == NULL) { return NULL; }

	fseek(fp, 0L, SEEK_END);
	long size = ftell(fp);

	fseek(fp, 0L, SEEK_SET);
	char* buf = new char[size + 1];
	fread(buf, 1, size, fp);
	buf[size] = '\0';

	fclose(fp);

	return buf;
}


static void AddShader(GLuint ShaderProgram, const char* pShaderText, GLenum ShaderType)
{
	// create a shader object
	GLuint ShaderObj = glCreateShader(ShaderType);

	if (ShaderObj == 0) {
		std::cerr << "Error creating shader..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}
	const char* pShaderSource = readShaderSource(pShaderText);

	// Bind the source code to the shader, this happens before compilation
	glShaderSource(ShaderObj, 1, (const GLchar**)&pShaderSource, NULL);
	
	// Compile the shader and check for errors
	glCompileShader(ShaderObj);
	GLint success;
	
	// Check for shader related errors using glGetShaderiv
	glGetShaderiv(ShaderObj, GL_COMPILE_STATUS, &success);
	if (!success) {
		GLchar InfoLog[1024] = { '\0' };
		glGetShaderInfoLog(ShaderObj, 1024, NULL, InfoLog);
		std::cerr << "Error compiling "
			<< (ShaderType == GL_VERTEX_SHADER ? "vertex" : "fragment")
			<< " shader program: " << InfoLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Attach the compiled shader object to the program object
	glAttachShader(ShaderProgram, ShaderObj);
}

GLuint CompileShaders()
{
	//Start the process of setting up our shaders by creating a program ID
	//Note: we will link all the shaders together into this ID
	shaderProgramID = glCreateProgram();
	if (shaderProgramID == 0) {
		std::cerr << "Error creating shader program..." << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Create two shader objects, one for the vertex, and one for the fragment shader
	AddShader(shaderProgramID, "simpleVertexShader.txt", GL_VERTEX_SHADER);
	AddShader(shaderProgramID, "simpleFragmentShader.txt", GL_FRAGMENT_SHADER);

	GLint Success = 0;
	GLchar ErrorLog[1024] = { '\0' };

	// After compiling all shader objects and attaching them to the program, we can finally link it
	glLinkProgram(shaderProgramID);
	
	// Check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_LINK_STATUS, &Success);
	if (Success == 0) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Error linking shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Program has been successfully linked but needs to be validated to check whether the program can execute given the current pipeline state
	glValidateProgram(shaderProgramID);

	// Check for program related errors using glGetProgramiv
	glGetProgramiv(shaderProgramID, GL_VALIDATE_STATUS, &Success);
	if (!Success) {
		glGetProgramInfoLog(shaderProgramID, sizeof(ErrorLog), NULL, ErrorLog);
		std::cerr << "Invalid shader program: " << ErrorLog << std::endl;
		std::cerr << "Press enter/return to exit..." << std::endl;
		std::cin.get();
		exit(1);
	}

	// Finally, use the linked shader program
	// Note: this program will stay in effect for all draw calls until you replace it with another or explicitly disable its use
	glUseProgram(shaderProgramID);
	return shaderProgramID;
}
#pragma endregion SHADER_FUNCTIONS

// VBO Functions - click on + to expand
#pragma region VBO_FUNCTIONS

ModelData generateObjectBufferMesh(const char* file_name, int meshIndex) {
	/*----------------------------------------------------------------------------
	LOAD MESH HERE AND COPY INTO BUFFERS
	----------------------------------------------------------------------------*/

	// Note: you may get an error "vector subscript out of range" if you are using this code for a mesh that doesnt have positions and normals
	// Might be an idea to do a check for that before generating and binding the buffer.

	ModelData model = load_mesh(file_name);
	
	// Find attributes
	loc1[meshIndex] = glGetAttribLocation(shaderProgramID, "vertex_position");
	loc2[meshIndex] = glGetAttribLocation(shaderProgramID, "vertex_normal");
	loc3[meshIndex] = glGetAttribLocation(shaderProgramID, "vertex_texture");

	// Setup vertex positions VBO
	vp_vbo[meshIndex] = 0;
	glGenBuffers(1, &vp_vbo[meshIndex]);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo[meshIndex]);
	glBufferData(GL_ARRAY_BUFFER, mesh_data[meshIndex].mPointCount * sizeof(glm::vec3), &mesh_data[meshIndex].mVertices[0], GL_STATIC_DRAW);

	// Setup vertex normals VBO
	vn_vbo[meshIndex] = 0;
	glGenBuffers(1, &vn_vbo[meshIndex]);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo[meshIndex]);
	glBufferData(GL_ARRAY_BUFFER, mesh_data[meshIndex].mPointCount * sizeof(glm::vec3), &mesh_data[meshIndex].mNormals[0], GL_STATIC_DRAW);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	unsigned int vt_vbo = 0;
	//	glGenBuffers (1, &vt_vbo);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glBufferData (GL_ARRAY_BUFFER, monkey_head_data.mTextureCoords * sizeof (vec2), &monkey_head_data.mTextureCoords[0], GL_STATIC_DRAW);

	glBindVertexArray(mesh_vaos[meshIndex]);

	glEnableVertexAttribArray(loc1[meshIndex]);
	glBindBuffer(GL_ARRAY_BUFFER, vp_vbo[meshIndex]);
	glVertexAttribPointer(loc1[meshIndex], 3, GL_FLOAT, GL_FALSE, 0, NULL);
	glEnableVertexAttribArray(loc2[meshIndex]);
	glBindBuffer(GL_ARRAY_BUFFER, vn_vbo[meshIndex]);
	glVertexAttribPointer(loc2[meshIndex], 3, GL_FLOAT, GL_FALSE, 0, NULL);

	//	This is for texture coordinates which you don't currently need, so I have commented it out
	//	glEnableVertexAttribArray (loc3);
	//	glBindBuffer (GL_ARRAY_BUFFER, vt_vbo);
	//	glVertexAttribPointer (loc3, 2, GL_FLOAT, GL_FALSE, 0, NULL);
	return model;
}
#pragma endregion VBO_FUNCTIONS


void display() {

	// tell GL to only draw onto a pixel if the shape is closer to the viewer
	glEnable(GL_DEPTH_TEST); // enable depth-testing
	glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
	glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
	glUseProgram(shaderProgramID);

	// Bind pumpkin mesh vao
	glBindVertexArray(mesh_vaos[0]);
	cout << "binding vao " << mesh_vaos[0] << endl;

	//Declare your uniform variables that will be used in your shader
	int matrix_location = glGetUniformLocation(shaderProgramID, "model");
	int view_mat_location = glGetUniformLocation(shaderProgramID, "view");
	int proj_mat_location = glGetUniformLocation(shaderProgramID, "proj");

	glm::mat4 view;
	glm::mat4 persp_proj;
	
	// Root of the Hierarchy
	// Pumpkin #1 (root)
	view = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, 0.0f, -10.0f));
	persp_proj = glm::perspective(45.0f, (float)WINDOW_WIDTH / (float)WINDOW_HEIGHT, 0.1f, 1000.0f);
	// Root model is already initialised in init() - This is to cater for keyboard control

	// update uniforms & draw
	glUniformMatrix4fv(view_mat_location, 1, GL_FALSE, glm::value_ptr(view));
	glUniformMatrix4fv(proj_mat_location, 1, GL_FALSE, glm::value_ptr(persp_proj));
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(model));

	glDrawArrays(GL_TRIANGLES, 0, mesh_data[0].mPointCount);

	// Pumpkin #1.1
	// Child of pumpkin #1
	glm::mat4 modelChild1;
	modelChild1 = glm::rotate(modelChild1, -rotate_y, glm::vec3(0.0f, 1.0f, 0.0f));
	modelChild1 = glm::translate(modelChild1, glm::vec3(0.8f, 0.0f, 0.0f));
	modelChild1 = glm::scale(modelChild1, glm::vec3(0.5f, 0.5f, 0.5f));
	
	// Apply the root matrix to the child matrix
	glm::mat4 global1 = model * modelChild1;

	// Update the appropriate uniform and draw the mesh again
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(global1));
	glDrawArrays(GL_TRIANGLES, 0, mesh_data[0].mPointCount);

	// Pumpkin #1.2
	// Child of pumpkin #1
	glm::mat4 modelChild2;
	modelChild2 = glm::rotate(modelChild2, 90.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	modelChild2 = glm::rotate(modelChild2, -rotate_y, glm::vec3(0.0f, 1.0f, 0.0f));
	modelChild2 = glm::translate(modelChild2, glm::vec3(0.8f, 0.0f, 0.0f));
	modelChild2 = glm::scale(modelChild2, glm::vec3(0.5f, 0.5f, 0.5f));
	
	// Apply the root matrix to the child matrix
	glm::mat4 global2 = model * modelChild2;

	// Update the appropriate uniform and draw the mesh again
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(global2));
	glDrawArrays(GL_TRIANGLES, 0, mesh_data[0].mPointCount);

	// Pumpkin #1.3
	// Child of pumpkin #1
	glm::mat4 modelChild3;
	modelChild3 = glm::rotate(modelChild3, 180.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	modelChild3 = glm::rotate(modelChild3, -rotate_y, glm::vec3(0.0f, 1.0f, 0.0f));
	modelChild3 = glm::translate(modelChild3, glm::vec3(0.8f, 0.0f, 0.0f));
	modelChild3 = glm::scale(modelChild3, glm::vec3(0.5f, 0.5f, 0.5f));

	// Apply the root matrix to the child matrix
	glm::mat4 global3 = model * modelChild3;

	// Update the appropriate uniform and draw the mesh again
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(global3));
	glDrawArrays(GL_TRIANGLES, 0, mesh_data[0].mPointCount);

	// Pumpkin #1.4
	// Child of pumpkin #1
	glm::mat4 modelChild4;
	modelChild4 = glm::rotate(modelChild4, 270.0f, glm::vec3(0.0f, 1.0f, 0.0f));
	modelChild4 = glm::rotate(modelChild4, -rotate_y, glm::vec3(0.0f, 1.0f, 0.0f));
	modelChild4 = glm::translate(modelChild4, glm::vec3(0.8f, 0.0f, 0.0f));
	modelChild4 = glm::scale(modelChild4, glm::vec3(0.5f, 0.5f, 0.5f));

	// Apply the root matrix to the child matrix
	glm::mat4 global4 = model * modelChild4;

	// Update the appropriate uniform and draw the mesh again
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(global4));
	glDrawArrays(GL_TRIANGLES, 0, mesh_data[0].mPointCount);

	// Pumpkin #1.5
	// Child of pumpkin #1
	glm::mat4 modelChild5;
	modelChild5 = glm::rotate(modelChild5, -rotate_y, glm::vec3(0.0f, 1.0f, 0.0f));
	modelChild5 = glm::translate(modelChild5, glm::vec3(0.0f, 0.4f, 0.0f));

	// Apply the root matrix to the child matrix
	glm::mat4 global5 = model * modelChild5;

	// Update the appropriate uniform and draw the mesh again
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(global5));
	glDrawArrays(GL_TRIANGLES, 0, mesh_data[0].mPointCount);


	// Bind skull mesh vao
	glBindVertexArray(mesh_vaos[1]);
	cout << "binding vao " << mesh_vaos[1] << endl;


	// Skull #1.5.1
	// Child of pumpkin #1.5 - grandchild of pumpkin #1
	glm::mat4 modelChild6;
	modelChild6 = glm::translate(modelChild6, glm::vec3(0.0f, 0.4f, 0.0f));
	modelChild6 = glm::scale(modelChild6, glm::vec3(0.7f, 0.7f, 0.7f));

	// Apply the root matrix and the parent matrix to the child matrix
	glm::mat4 global6 = model * modelChild5 * modelChild6;

	// Update the appropriate uniform and draw the mesh again
	glUniformMatrix4fv(matrix_location, 1, GL_FALSE, glm::value_ptr(global6));
	glDrawArrays(GL_TRIANGLES, 0, mesh_data[1].mPointCount);

	glutSwapBuffers();
}


void updateScene() {

	static DWORD last_time = 0;
	DWORD curr_time = timeGetTime();
	if (last_time == 0)
		last_time = curr_time;
	float delta = (curr_time - last_time) * 0.001f;
	last_time = curr_time;

	// Rotate the model slowly around the y axis at 20 degrees per second
	rotate_y += 20.0f * delta;
	rotate_y = fmodf(rotate_y, 360.0f);

	// Draw the next frame
	glutPostRedisplay();
}


void init()
{
	// Set up the shaders
	shaderProgramID = CompileShaders();

	// Load pumpkin mesh
	ModelData pumpkin = generateObjectBufferMesh(PUMPKIN_MESH, 0);
	mesh_data[0] = pumpkin;

	// Load skull mesh
	ModelData skull = generateObjectBufferMesh(SKULL_MESH, 1);
	mesh_data[1] = skull;

	// Initialise root model
	model = glm::rotate(glm::mat4(1.0f), 0.0f, glm::vec3(0.0f, 0.0f, 1.0f));
	model = glm::translate(model, glm::vec3(0.0f, -1.0f, 0.0f));
	model = glm::scale(model, glm::vec3(3.0f, 3.0f, 3.0f));

}

// Placeholder code for the keypress
void keypress(unsigned char key, int x, int y) {
	switch (key) {
	
	case 'a':
		// Negative X Translation
		cout << "Case a: - X Translation" << endl;
		model = glm::translate(model, glm::vec3(-0.1f, 0.0f, 0.0f));
		break;
	
	case 'd':
		// Positive X Translation
		cout << "Case d: + X Translation" << endl;
		model = glm::translate(model, glm::vec3(0.1f, 0.0f, 0.0f));
		break;
	
	case 'w':
		// Positive Y Translation
		cout << "Case w: + Y Translation" << endl;
		model = glm::translate(model, glm::vec3(0.0f, 0.1f, 0.0f));
		break;
	
	case 's':
		// Negative Y Translation
		cout << "Case s: - Y Translation" << endl;
		model = glm::translate(model, glm::vec3(0.0f, -0.1f, 0.0f));
		break;
	
	case 'q':
		// Negative Z Translation
		cout << "Case q: - Z Translation" << endl;
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, -0.1f));
		break;
	
	case 'e':
		// Positive Z Translation
		cout << "Case e: + Z Translation" << endl;
		model = glm::translate(model, glm::vec3(0.0f, 0.0f, 0.1f));
		break;
	
	default:
		cout << "Default case" << endl;
		break;
	}

	//Redraw scene instantly rather than waiting for display().
	glutPostRedisplay();
}

int main(int argc, char** argv) {

	// Set up the window
	glutInit(&argc, argv);
	glutInitDisplayMode(GLUT_DOUBLE | GLUT_RGB);
	glutInitWindowSize((float)WINDOW_WIDTH, (float)WINDOW_HEIGHT);
	glutCreateWindow("Hello Triangle");

	// Tell glut where the display function is
	glutDisplayFunc(display);
	glutIdleFunc(updateScene);
	glutKeyboardFunc(keypress);

	// A call to glewInit() must be done after glut is initialized!
	GLenum res = glewInit();
	// Check for any errors
	if (res != GLEW_OK) {
		fprintf(stderr, "Error: '%s'\n", glewGetErrorString(res));
		return 1;
	}
	// Set up your objects and shaders
	init();
	// Begin infinite event loop
	glutMainLoop();
	return 0;
}
