// Include standard headers
#include <glm/trigonometric.hpp>
#include <stdio.h>
#include <stdlib.h>
#include <vector>
#include <memory>
#include <iostream>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

// Include GLEW
#include <GL/glew.h>

// Include GLFW
#include <GLFW/glfw3.h>
GLFWwindow* window;

// Include GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <iostream>

using namespace glm;

#include <common/shader.hpp>
#include <common/objloader.hpp>
#include <common/vboindexer.hpp>



// settings
const unsigned int SCR_WIDTH = 800;
const unsigned int SCR_HEIGHT = 600;

// camera
glm::vec3 camera_position   = glm::vec3(0.0f, 2.0f, 10.0f);
glm::vec3 camera_target = glm::vec3(0.0f, 0.0f, -1.0f);
glm::vec3 camera_up    = glm::vec3(0.0f, 1.0f,  0.0f);

// glm::vec3 orbital_camera_position = glm::vec3(0.0f, 10.0f, 10.0f);

// timing
float deltaTime = 0.0f;	// time between current frame and last frame
float lastFrame = 0.0f;

int sommets = 32;
const int MIN_SOMMETS = 4;
const int MAX_SOMMETS = 248;
bool scaleT = false;

// rotation
float angle = 0.;
float angle_perspective = 45.;
float zoom = 1.;
bool orbital = false;
float rotation_speed = 0.5f;

GLuint programID;
GLuint MatrixID;
glm::mat4 MVP;

bool debugFilaire = false;

/*******************************************************************************/

// Chargeur de texture
GLuint loadTexture(const char* filename) {
    GLuint textureID;
    glGenTextures(1, &textureID);

    int width, height, nrChannels;
    unsigned char *data = stbi_load(filename, &width, &height, &nrChannels, 0);
    if (data) {
        glBindTexture(GL_TEXTURE_2D, textureID);

        GLenum format = GL_RGB;
        if (nrChannels == 1) format = GL_RED;
        else if (nrChannels == 3) format = GL_RGB;
        else if (nrChannels == 4) format = GL_RGBA;
        glTexImage2D(GL_TEXTURE_2D, 0, format, width, height, 0, format, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);

        // Paramètres de texture
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    } else {
        std::cerr << "Failed to load texture: " << filename << std::endl;
    }
    stbi_image_free(data);
    return textureID;
}

void calculateUVSphere(std::vector<glm::vec3>& vertices,std::vector<glm::vec2>& uvs){
    uvs.clear();
    for(const auto& vertex : vertices){
        float theta = atan2(vertex.z, vertex.x);
        float phi = acos(vertex.y / glm::length(vertex));

        float u = (theta + M_PI) / (2.0f * M_PI);
        float v = phi / M_PI;

        uvs.push_back(glm::vec2(u, v));
    }
}

void plan(std::vector<glm::vec3> &vertices,std::vector<glm::vec2> &uvs,std::vector<unsigned short> &indices){
    float taille = 10.0f;
    float m = taille / 2.0f;
    float pas = taille / (float)sommets;

    for(int i = 0; i <= sommets; i++){
        for(int j = 0; j <= sommets; j++){
            float x = -m + j * pas;
            float z = -m + i * pas;

            vertices.emplace_back(glm::vec3(x,0.0f,z));

            float u = (float)j / (float)(sommets-1);
            float v = (float)i / (float)(sommets-1);

            uvs.emplace_back(glm::vec2(u,v));
        }
    }

    for(int i = 0; i < sommets-1; i++){
        for(int j = 0; j < sommets-1; j++){
            int topleft = i * (sommets+1) + j;
            int topright = topleft + 1;
            int bottomleft = (i+1) * (sommets+1) + j;
            int bottomright = bottomleft + 1;

            indices.push_back(topleft);
            indices.push_back(bottomleft);
            indices.push_back(topright);

            indices.push_back(topright);
            indices.push_back(bottomleft);
            indices.push_back(bottomright);
        }
    }
}

float getTerrainHeight(float x, float z, const std::vector<glm::vec3>& vertices, const std::vector<unsigned short>& indices, const unsigned char* heightmapData, int heightmapWidth, int heightmapHeight) {
    float taille = 10.0f;
    float m = taille / 2.0f;
    float pas = taille / (float)sommets;

    int i = (z + m) / pas;
    int j = (x + m) / pas;

    if (i < 0 || i >= sommets - 1 || j < 0 || j >= sommets - 1) {
        return 0.0f;
    }

    int topleft = i * (sommets + 1) + j;
    int topright = topleft + 1;
    int bottomleft = (i + 1) * (sommets + 1) + j;
    int bottomright = bottomleft + 1;

    glm::vec3 v0 = vertices[topleft];
    glm::vec3 v1, v2;

    if ((x - v0.x) + (z - v0.z) < (bottomright - topleft)) {
        v1 = vertices[bottomleft];
        v2 = vertices[topright];
    } else {
        v1 = vertices[bottomright];
        v2 = vertices[topright];
    }

    // Barycentriques
    glm::vec3 v0v1 = v1 - v0;
    glm::vec3 v0v2 = v2 - v0;
    glm::vec3 p = glm::vec3(x, 0.0f, z);
    glm::vec3 v0p = p - v0;

    float d00 = glm::dot(v0v1,v0v1);
    float d01 = glm::dot(v0v1,v0v2);
    float d11 = glm::dot(v0v2,v0v2);
    float d20 = glm::dot(v0p,v0v1);
    float d21 = glm::dot(v0p,v0v2);

    float denom = d00*d11 - d01*d01;
    float v = (d11*d20 - d01*d21) / denom;
    float w = (d00*d21 - d01*d20) / denom;
    float u = 1.0f - v - w;

    // Interpolation de la hauteur avec la heightmap
    float height0 = heightmapData[((int)(v0.z + m) * heightmapWidth + (int)(v0.x + m))] / 255.0f;
    float height1 = heightmapData[((int)(v1.z + m) * heightmapWidth + (int)(v1.x + m))] / 255.0f;
    float height2 = heightmapData[((int)(v2.z + m) * heightmapWidth + (int)(v2.x + m))] / 255.0f;

    return u*(v0.y + height0*1.9) + v*(v1.y + height1*1.9) + w *(v2.y + height2*1.9);
}




/*******************************************************************************/

class Transform{
public:
    glm::vec3 position;
    glm::vec3 rotation;
    glm::vec3 scale;

    Transform() : position(0.0f),rotation(0.0f),scale(1.0f){}
    glm::mat4 getMatrice() const {
        glm::mat4 mat = glm::mat4(1.0f);
        mat = glm::translate(mat, position);
        mat = glm::rotate(mat, rotation.x, glm::vec3(1, 0, 0));
        mat = glm::rotate(mat, rotation.y, glm::vec3(0, 1, 0));
        mat = glm::rotate(mat, rotation.z, glm::vec3(0, 0, 1));
        mat = glm::scale(mat, scale);
        return mat;
    }
};

class SNode{
public:
    Transform transform;
    std::vector<std::shared_ptr<SNode>> feuilles;
    GLuint vao,vbo,ibo,textureID;
    size_t indexCPT;
    glm::vec3 color;
    int type_objet;
    glm::vec3 rayon;

    std::vector<glm::vec3> vertices;
    std::vector<glm::vec2> uvs;
    std::vector<unsigned short> indices;

    SNode(int obj,const char* texturePath) {
        type_objet = obj;
        buffers();
        textureID = loadTexture(texturePath);
    }

    // Ancienne version sans texture (couleur)
    SNode(int obj, glm::vec3 nodeColor) : color(nodeColor) {
        type_objet = obj;
        buffers();
    }

    SNode(){}

    void buffers() {
        vertices.clear();
        uvs.clear();
        indices.clear();
        std::vector<std::vector<unsigned short>> triangles;
        
        switch(type_objet) {
            case 1:
                plan(vertices,uvs,indices);
                break;
            default:
                loadOFF("modeles/sphere2.off",vertices,indices,triangles);
                rayon = vertices[0];
                calculateUVSphere(vertices, uvs);
                break;
        }

        glGenVertexArrays(1, &vao);
        glBindVertexArray(vao);

        glGenBuffers(1, &vbo);
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(glm::vec3), &vertices[0], GL_STATIC_DRAW);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);
        glEnableVertexAttribArray(0);

        GLuint uvVBO;
        glGenBuffers(1, &uvVBO);
        glBindBuffer(GL_ARRAY_BUFFER, uvVBO);
        glBufferData(GL_ARRAY_BUFFER, uvs.size() * sizeof(glm::vec2), &uvs[0], GL_STATIC_DRAW);
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, sizeof(glm::vec2), (void*)0);
        glEnableVertexAttribArray(1);

        glGenBuffers(1, &ibo);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ibo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, indices.size() * sizeof(unsigned short), &indices[0], GL_STATIC_DRAW);
        glBindVertexArray(0);
        indexCPT = indices.size();
    }

    void addFeuille(std::shared_ptr<SNode> feuille){
        feuilles.push_back(feuille);
    }

    virtual void update(float deltaTime){
        for(auto &feuille: feuilles){
            feuille->update(deltaTime);
        }
    }

    virtual void draw(GLuint shaderProgram,const glm::mat4 &brancheMatrix,const glm::vec3 &branchePos){
        glm::mat4 ModelMatrix = brancheMatrix*transform.getMatrice();
        
        glUseProgram(shaderProgram);

        glm::mat4 ViewMatrix = glm::lookAt(
            camera_position,
            camera_position+camera_target,
            camera_up
        );
        glm::mat4 ProjectionMatrix = glm::perspective(glm::radians(angle_perspective),(float)SCR_WIDTH / (float)SCR_HEIGHT,0.1f,100.0f);

        MVP = ProjectionMatrix*ViewMatrix*ModelMatrix;
        glUniformMatrix4fv(MatrixID,1,GL_FALSE,&MVP[0][0]);

        GLuint isTerrainID = glGetUniformLocation(shaderProgram, "isTerrain");
        if (type_objet == 1) {
            glUniform1i(isTerrainID, 1);
        } else {
            glUniform1i(isTerrainID, 0);
        }

        // Ancienne version sans texture (couleur)
        GLuint colorLocation = glGetUniformLocation(shaderProgram, "objColor");
        glUniform3fv(colorLocation, 1, &color[0]);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, textureID);
        GLuint textureLocation = glGetUniformLocation(shaderProgram, "texture1");
        if(textureLocation == -1){
            std::cerr << "Erreur : Uniform texture1 introuvable" << std::endl;
        }
        glUniform1i(textureLocation, 0);

        glBindVertexArray(vao);
        glEnableVertexAttribArray(0);
        glBindBuffer(GL_ARRAY_BUFFER,vbo);
        glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 0, (void*)0);
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER,ibo);

        glDrawElements(
            GL_TRIANGLES,
            indexCPT,
            GL_UNSIGNED_SHORT,
            (void*)0
        );
        glBindVertexArray(0);

        glDisableVertexAttribArray(0);
        glBindVertexArray(0);

        for(auto &feuille: feuilles){
            glm::vec3 worldPos = branchePos + transform.position;
            feuille->draw(shaderProgram,ModelMatrix,worldPos);
        }
    }

};

class Scene{
public:
    std::shared_ptr<SNode> racine;

    Scene(){racine = std::make_shared<SNode>();}

    void update(float deltaTime){
        racine->update(deltaTime);
    }

    void draw(GLuint shaderProgram){
        racine->draw(shaderProgram,glm::mat4(1.0f),glm::vec3(0.0f));
    }
};

/*******************************************************************************/

void processInput(GLFWwindow *window,std::shared_ptr<SNode> soleil);

int main( void ){
    // Initialise GLFW
    if( !glfwInit() )
    {
        fprintf( stderr, "Failed to initialize GLFW\n" );
        getchar();
        return -1;
    }

    glfwWindowHint(GLFW_SAMPLES, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE); // To make MacOS happy; should not be needed
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    // Open a window and create its OpenGL context
    window = glfwCreateWindow( 1024, 768, "TP4 - GLFW", NULL, NULL);
    if( window == NULL ){
        fprintf( stderr, "Failed to open GLFW window. If you have an Intel GPU, they are not 3.3 compatible. Try the 2.1 version of the tutorials.\n" );
        getchar();
        glfwTerminate();
        return -1;
    }
    glfwMakeContextCurrent(window);

    // Initialize GLEW
    glewExperimental = true; // Needed for core profile
    if (glewInit() != GLEW_OK) {
        fprintf(stderr, "Failed to initialize GLEW\n");
        getchar();
        glfwTerminate();
        return -1;
    }

    // Ensure we can capture the escape key being pressed below
    glfwSetInputMode(window, GLFW_STICKY_KEYS, GL_TRUE);
    // Hide the mouse and enable unlimited mouvement
    //  glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);

    // Set the mouse at the center of the screen
    glfwPollEvents();
    glfwSetCursorPos(window, 1024/2, 768/2);

    // Dark blue background
    glClearColor(0.8f, 0.8f, 0.8f, 0.0f);
    // glClearColor(0.0f, 0.0f, 0.0f, 0.0f);

    // Enable depth test
    glEnable(GL_DEPTH_TEST);
    // Accept fragment if it closer to the camera than the former one
    glDepthFunc(GL_LESS);

    // Cull triangles which normal is not towards the camera
    //glEnable(GL_CULL_FACE);

    GLuint VertexArrayID;
    glGenVertexArrays(1, &VertexArrayID);
    glBindVertexArray(VertexArrayID);

    // Create and compile our GLSL program from the shaders
    programID = LoadShaders( "vertex_shader.glsl", "fragment_shader.glsl" );

    /****************************************/
    // Get a handle for our "Model View Projection" matrices uniforms
    MatrixID = glGetUniformLocation(programID,"MVP");

    /****************************************/

    // Get a handle for our "LightPosition" uniform
    glUseProgram(programID);
    GLuint LightID = glGetUniformLocation(programID, "LightPosition_worldspace");

    glActiveTexture(GL_TEXTURE3);
    GLuint heightmapTexture = loadTexture("textures/heightmap-1024x1024.png");
    glBindTexture(GL_TEXTURE_2D,heightmapTexture);
    GLuint heightmapID = glGetUniformLocation(programID,"heightmap");
    glUniform1i(heightmapID, 3);

    GLuint heightScaleID = glGetUniformLocation(programID,"heightScale");
    glUniform1f(heightScaleID,1.0f);

    int heightmapWidth, heightmapHeight, nrChannels;
    unsigned char *heightmapData = stbi_load("textures/heightmap-1024x1024.png", &heightmapWidth, &heightmapHeight, &nrChannels, 1);

    std::cout << "width : " << heightmapWidth << " | height : " << heightmapHeight << std::endl;

    std::shared_ptr<Scene> scene = std::make_shared<Scene>();

    std::shared_ptr<SNode> soleil = std::make_shared<SNode>(0,"textures/s2.png");
    std::shared_ptr<SNode> plan = std::make_shared<SNode>(1,"textures/grass.png");

    soleil->transform.scale = glm::vec3(0.25f);

    // float val_rayon = (soleil->transform.position - (soleil->rayon)*0.25f).length(); 
    // std::cout << "pos soleil : (" << soleil->transform.position.x << "," << soleil->transform.position.y << "," << soleil->transform.position.z << ")" << std::endl;
    // std::cout << "pos rayon : (" << soleil->rayon.x << "," << soleil->rayon.y << "," << soleil->rayon.z << ")" << std::endl;
    // std::cout << "rayon : " << val_rayon << std::endl;

    soleil->transform.position = glm::vec3(0.0f,1.0f,0.0f);

    scene->racine->addFeuille(soleil);
    scene->racine->addFeuille(plan);

    float time = 0.0f;

    // For speed computation
    double lastTime = glfwGetTime();
    int nbFrames = 0;

    std::vector<glm::vec3> terrainVertices = plan->vertices;
    std::vector<unsigned short> terrainIndices = plan->indices;

    do{
        // Measure speed
        // per-frame time logic
        // --------------------
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        // input
        // -----
        processInput(window,soleil);
        float terrainHeight = getTerrainHeight(
            soleil->transform.position.x,
            soleil->transform.position.z,
            terrainVertices, 
            terrainIndices,
            heightmapData,
            heightmapWidth,
            heightmapHeight
        );
        

        // Empêcher le soleil de traverser le sol
        if (soleil->transform.position.y < terrainHeight) {
            soleil->transform.position.y = terrainHeight;
        }

        if (debugFilaire) {
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);  // Vue filaire
        } else {
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);  // Vue normale
        }

        // Clear the screen
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Use our shader
        glUseProgram(programID);

        scene->draw(programID);



        scene->update(deltaTime);

        // std::cout << "Current pos : (" << soleil->transform.position.x << "," << soleil->transform.position.y << "," << soleil->transform.position.z << ")" << std::endl;

        // Swap buffers
        glfwSwapBuffers(window);
        glfwPollEvents();
        
        time += deltaTime;

    } // Check if the ESC key was pressed or the window was closed
    while( glfwGetKey(window, GLFW_KEY_ESCAPE ) != GLFW_PRESS &&
           glfwWindowShouldClose(window) == 0 );

    glDeleteProgram(programID);
    glDeleteVertexArrays(1,&VertexArrayID);

    // Close OpenGL window and terminate GLFW
    glfwTerminate();

    return 0;
}

// process all input: query GLFW whether relevant keys are pressed/released this frame and react accordingly
// ---------------------------------------------------------------------------------------------------------
void processInput(GLFWwindow *window, std::shared_ptr<SNode> soleil){
    if (glfwGetKey(window, GLFW_KEY_ESCAPE) == GLFW_PRESS)
        glfwSetWindowShouldClose(window, true);

    float cameraSpeed = 5.0 * deltaTime;
    if(glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
        camera_position += cameraSpeed * camera_target;
    if(glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
        camera_position -= cameraSpeed * camera_target;
    if(glfwGetKey(window,GLFW_KEY_A) == GLFW_PRESS)
        camera_position -= glm::normalize(glm::cross(camera_target,camera_up))*cameraSpeed;
    if(glfwGetKey(window,GLFW_KEY_W) == GLFW_PRESS)
        camera_position += cameraSpeed * camera_up;
    if(glfwGetKey(window,GLFW_KEY_D) == GLFW_PRESS)
        camera_position += glm::normalize(glm::cross(camera_target,camera_up))*cameraSpeed;
    if(glfwGetKey(window,GLFW_KEY_S) == GLFW_PRESS)
        camera_position -= cameraSpeed * camera_up;
    if(glfwGetKey(window,GLFW_KEY_V) == GLFW_PRESS)
        debugFilaire = !debugFilaire;

    /****************************************/

    if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS) 
        soleil->transform.position -= glm::vec3(0.0f, 0.0f, 0.05f);
    if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS) 
        soleil->transform.position += glm::vec3(0.0f, 0.0f, 0.05f);
    if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS) 
        soleil->transform.position -= glm::vec3(0.05f, 0.0f, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS) 
        soleil->transform.position += glm::vec3(0.05f, 0.0f, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_C) == GLFW_PRESS) 
        soleil->transform.position -= glm::vec3(0.0f, 0.05f, 0.0f);
    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS) 
        soleil->transform.position += glm::vec3(0.0f, 0.05f, 0.0f);
}

// glfw: whenever the window size changed (by OS or user resize) this callback function executes
// ---------------------------------------------------------------------------------------------
void framebuffer_size_callback(GLFWwindow* window, int width, int height)
{
    // make sure the viewport matches the new window dimensions; note that width and
    // height will be significantly larger than specified on retina displays.
    glViewport(0, 0, width, height);
}
