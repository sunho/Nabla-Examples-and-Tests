#ifndef _NBL_GLSL_WORKGROUP_SIZE_
#define _NBL_GLSL_WORKGROUP_SIZE_ 256
#endif
layout(local_size_x=_NBL_GLSL_WORKGROUP_SIZE_, local_size_y=1, local_size_z=1) in;
 
#define _NBL_GLSL_EXT_FFT_GET_PARAMETERS_DEFINED_
#define _NBL_GLSL_EXT_FFT_SET_DATA_DEFINED_
#define _NBL_GLSL_EXT_FFT_GET_PADDED_DATA_DEFINED_
#include "nbl/builtin/glsl/ext/FFT/fft.glsl"

layout(push_constant) uniform PushConstants
{
	nbl_glsl_ext_FFT_Parameters_t params;
} pc;

 nbl_glsl_ext_FFT_Parameters_t nbl_glsl_ext_FFT_getParameters()
 {
	 return pc.params;
 }
