// Layout dependent
$input a_position
$input a_color0
#ifdef USE_TEX0
$input a_texcoord0
#endif

#if COLOR == 1
$output v_custom0
#endif
#if COLOR == 2
$output v_custom1
#endif


// Layout dependent


/*
 * Copyright 2011-2020 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */

#include "../common/common.sh"

void main()
{
	gl_Position = mul(u_modelViewProj, vec4(a_position, 1.0) );

#if COLOR == 1
	v_custom0 = vec4(1.0,0.0,0.0,0.0);
#endif
#if COLOR == 2
	#ifdef USE_TEX0
		v_custom1 = vec4(0.0,1.0,0.0,0.0);		
	#else
		v_custom1 = vec4(0.0,0.0,1.0,0.0);
	#endif	
#endif
}
