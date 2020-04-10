#if COLOR == 1
$input v_custom0
#endif

#if COLOR == 2
$input v_custom1
#endif

/*
 * Copyright 2011-2020 Branimir Karadzic. All rights reserved.
 * License: https://github.com/bkaradzic/bgfx#license-bsd-2-clause
 */
 
#include "../common/common.sh"

void main()
{
#if COLOR == 1
	gl_FragColor = v_custom0;
#endif
#if COLOR == 2
	gl_FragColor = v_custom1;
#endif
#ifndef COLOR
	gl_FragColor = vec4(1.0,0.0,1.0,1.0);
#endif
}
