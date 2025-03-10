#version 430 core

layout (location = 0) in uint data;
uniform mat4 u_view;
uniform mat4 u_projection;

out vec4 frag_viewspace;
out vec3 frag_pos;
out vec3 frag_normal;
out float frag_ao;
flat out uint frag_type;

uniform vec3 NORMALS[6] = {
  vec3( 0, 1, 0 ),
  vec3(0, -1, 0 ),
  vec3( 1, 0, 0 ),
  vec3( -1, 0, 0 ),
  vec3( 0, 0, 1 ),
  vec3( 0, 0, -1 )
};

void main() {
  float x = float(data&63u);
  float y = float((data >> 6)&63u);
  float z = float((data >> 12)&63u);
  uint type  = (data >> 18)&255u;
  uint norm  = (data >> 26)&7u;
  uint ao = (data >> 29)&3u;
  frag_ao = clamp(float(ao) / 3.0, 0.5, 1.0);

  frag_pos = vec3(x, y, z) - vec3(0.5);
  frag_viewspace = u_view * vec4(frag_pos, 1);
  frag_normal = NORMALS[norm];
  frag_type = type;
  
  gl_Position = u_projection * frag_viewspace;
} 