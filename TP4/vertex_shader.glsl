#version 330 core

// Input vertex data, different for all executions of this shader.
layout(location = 0) in vec3 vertices_position_modelspace;
layout(location = 1) in vec2 vertexUV;

uniform mat4 MVP;

// Values that stay constant for the whole mesh.
out vec2 UV;
out float altitude;

void main(){

        // gl_Position = MVP * vec4(relief,1);
        gl_Position = MVP * vec4(vertices_position_modelspace,1);
        UV = vertexUV;

}

