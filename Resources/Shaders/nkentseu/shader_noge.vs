#version 450

#include "shader.h"

layout(location = 0) out vec3 uvw;
layout(location = 1) out vec3 v_worldNormal;
layout(location = 2) out vec4 v_worldPos;
layout(location = 3) out flat uint matIdx;