#ifdef HAVE_ARB_shader_draw_parameters
#define SETUP_SHADER_PARMS SetShaderParameters();
void SetShaderParameters() {
  in_drawID = drawID;
  in_baseInstance = baseInstance;
}
#else
#define SETUP_SHADER_PARMS //Unavailable
#endif
