#include "corange.h"

#include "particles.h"

static camera* cam;
static light* sun;

static model* floor;
static material* floor_mat;
static renderable* r_floor;

static int mouse_x;
static int mouse_y;
static int mouse_down;
static int mouse_right_down;

GLuint billboard_positions;
GLuint billboard_uvs;
const int max_particles = 1000;

static shader_program* particle_program;
static texture* particle_texture;

void metaballs_init() {
  
  load_folder("/kernels/");
  
  particles_init();
  
  load_folder("/resources/floor/");
  load_folder("/resources/particles/");
  load_folder("/resources/shaders/");
  
  particle_program = asset_get("/resources/shaders/particles.prog");
  
  particle_texture = asset_get("/resources/particles/white.dds");
  
  floor = asset_get("/resources/floor/floor.obj");
  floor_mat = asset_get("/resources/floor/floor.mat");
  
  r_floor = renderable_new("floor");
  renderable_add_model(r_floor, floor);
  renderable_set_material(r_floor, floor_mat);
  
  viewport_set_vsync(1);
  
  cam = camera_new( v3(20.0, 10.0, 0.0) , v3(0.0, 5.0, 0.0) );
  sun = light_new_type( v3(30,43,-26), light_type_spot );
  
  sun->ambient_color = v3(0.749, 0.855, 0.902);
  sun->diffuse_color = v3(1.0, 0.875, 0.573);
  
  shadow_mapper_init(sun);  
  
  forward_renderer_init();
  forward_renderer_set_camera(cam);
  forward_renderer_set_light(sun);
  forward_renderer_set_shadow_texture( shadow_mapper_depth_texture() );
  
  vector3* temp_pos = malloc(sizeof(vector3) * 4 * max_particles);
  int i = 0;
  while (i < 4 * max_particles) {
    temp_pos[i]   = v3(-1, -1, 0);
    temp_pos[i+1] = v3(-1,  1, 0);
    temp_pos[i+2] = v3( 1,  1, 0);
    temp_pos[i+3] = v3( 1, -1, 0);
    i += 4;
  }
  
  glGenBuffers(1, &billboard_positions);
  glBindBuffer(GL_ARRAY_BUFFER, billboard_positions);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vector3) * 4 * max_particles, temp_pos, GL_DYNAMIC_COPY);
  
  free(temp_pos);
  
  vector2* temp_uvs = malloc(sizeof(vector2) * 4 * max_particles);
  i = 0;
  while (i < 4 * max_particles) {
    temp_uvs[i]   = v2(0, 0);
    temp_uvs[i+1] = v2(0, 1);
    temp_uvs[i+2] = v2(1, 1);
    temp_uvs[i+3] = v2(1, 0);
    i += 4;
  }
  
  glGenBuffers(1, &billboard_uvs);
  glBindBuffer(GL_ARRAY_BUFFER, billboard_uvs);
  glBufferData(GL_ARRAY_BUFFER, sizeof(vector2) * 4 * max_particles, temp_uvs, GL_DYNAMIC_COPY);
  
  free(temp_uvs);
}

void metaballs_update() {

  Uint8 keystate = SDL_GetMouseState(NULL, NULL);
  if(keystate & SDL_BUTTON(1)){
    float a1 = -(float)mouse_x * frame_time() * 0.25;
    float a2 = (float)mouse_y * frame_time() * 0.25;
    
    cam->position = m33_mul_v3(m33_rotation_y( a1 ), cam->position );
    
    vector3 rotation_axis = v3_normalize(v3_cross( v3_sub(cam->position, cam->target) , v3(0,1,0) ));
    
    cam->position = m33_mul_v3(m33_rotation_axis_angle(rotation_axis, a2 ), cam->position );
  }
  
  if(keystate & SDL_BUTTON(3)){
    sun->position.x += (float)mouse_y / 2;
    sun->position.z -= (float)mouse_x / 2;
  }

  mouse_x = 0;
  mouse_y = 0;

  particles_update(0.005);
  
}

static float proj_matrix[16];
static float view_matrix[16];

void metaballs_render() {

  shadow_mapper_begin();
  shadow_mapper_render_renderable(r_floor);
  shadow_mapper_end();

  forward_renderer_begin();
  
  glClearColor(1.0f, 0.769f, 0.0f, 0.0f);
  glClear(GL_COLOR_BUFFER_BIT|GL_DEPTH_BUFFER_BIT);
  
  forward_renderer_render_renderable(r_floor);
  
    glUseProgram(*particle_program);
    
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE, GL_ONE);
    
    glUniform1i(glGetUniformLocation(*particle_program, "particle_texture"), 0);
    glActiveTexture(GL_TEXTURE0 + 0);
    glBindTexture(GL_TEXTURE_2D, *particle_texture);
    
    m44_to_array(camera_proj_matrix(cam, viewport_ratio()), proj_matrix);
    GLint proj_matrix_u = glGetUniformLocation(*particle_program, "proj_matrix");
    glUniformMatrix4fv(proj_matrix_u, 1, 0, proj_matrix);
    
    m44_to_array(camera_view_matrix(cam), view_matrix);
    GLint view_matrix_u = glGetUniformLocation(*particle_program, "view_matrix");
    glUniformMatrix4fv(view_matrix_u, 1, 0, view_matrix);
    
    GLuint particle_position = glGetAttribLocation(*particle_program, "particle_position");
    glEnableVertexAttribArray(particle_position);
    
    glBindBuffer(GL_ARRAY_BUFFER, particle_positions_buffer());
    glVertexAttribPointer(particle_position, 4, GL_FLOAT, GL_TRUE, 0, (void*)0);
    
    glBindBuffer(GL_ARRAY_BUFFER, billboard_positions);
    glVertexPointer(3, GL_FLOAT, 0, (void*)0);
    glEnableClientState(GL_VERTEX_ARRAY);
    
    glBindBuffer(GL_ARRAY_BUFFER, billboard_uvs);
    glTexCoordPointer(2, GL_FLOAT, 0, (void*)0);
    glEnableClientState(GL_TEXTURE_COORD_ARRAY);
    
    glDrawArrays(GL_QUADS, 0, particles_count() * 4);
  
    glDisableVertexAttribArray(particle_position);
  
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    
    glEnable(GL_DEPTH_TEST);
    glDisable(GL_BLEND);
    
    glUseProgram(0);
  
  forward_renderer_end();

}

void metaballs_event(SDL_Event event) {

  switch(event.type){

  case SDL_MOUSEBUTTONDOWN:

    if (event.button.button == SDL_BUTTON_WHEELUP) {
      cam->position = v3_sub(cam->position, v3_normalize(cam->position));
    }
    if (event.button.button == SDL_BUTTON_WHEELDOWN) {
      cam->position = v3_add(cam->position, v3_normalize(cam->position));
    }
    
  break;
  
  case SDL_MOUSEMOTION:
    mouse_x = event.motion.xrel;
    mouse_y = event.motion.yrel;
  break;
  }

}

void metaballs_finish() {

  camera_delete(cam);
  light_delete(sun);
  
  forward_renderer_finish();
  
  shadow_mapper_finish();
  
  particles_finish();

  glDeleteBuffers(1, &billboard_positions);
  glDeleteBuffers(1, &billboard_uvs);
  
}