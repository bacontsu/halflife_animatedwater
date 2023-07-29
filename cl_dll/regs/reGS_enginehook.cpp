// reGS_enginehook.cpp: hw.dll hooking
// Used libraries: SPTLib/MinHook

#include "hud.h"
#include "minhook/MinHook.h"
#include "sptlib/patterns.hpp"
#include "sptlib/MemUtils.h"
#include "sptlib/Utils.hpp"
#include "reGS_enginehook.h"
#include "reGS_patterns.hpp"

#include "com_model.h"
#include "SDL2/SDL_opengl.h"


Utils utils = Utils::Utils(NULL, NULL, NULL);

typedef void (*_EmitWaterPolys)(msurface_t* fa, int direction);
_EmitWaterPolys ORIG_EmitWaterPolys = NULL;
void EmitWaterPolys(msurface_t* fa, int direction);

void EmitWaterPolys(msurface_t* fa, int direction)
{
	//return ORIG_EmitWaterPolys(fa, direction);
	
	// idk why it doesnt work now
	glpoly_t* p;
	float* v;
	int i;
	float s, t, os, ot;
	float tempVert[3];

	for (p = fa->polys; p; p = p->next)
	{
		glBegin(GL_POLYGON);

		if (direction)
			v = p->verts[p->numverts - 1];
		else
			v = p->verts[0];

		for (i = 0; i < p->numverts; i++)
		{
			os = v[3];
			ot = v[4];

			tempVert[0] = v[0];
			tempVert[1] = v[1];
			tempVert[2] = v[2];
			//tempVert[2] += s;

			//gEngfuncs.Con_Printf("os:%f ot:%f", os, ot);

			s = os;
			s *= (1.0f / 64);

			t = ot;
			t *= (1.0f / 64);

			glTexCoord2f(s, t);
			glVertex3fv(tempVert);

			if (direction)
				v -= VERTEXSIZE;
			else
				v += VERTEXSIZE;
		}

		glEnd();
	}
}


void HWHook()
{
	void* handle;
	void* base;
	size_t size;

	if (!MemUtils::GetModuleInfo(L"hw.dll", &handle, &base, &size))
	{
		gEngfuncs.Con_DPrintf("HWHook: can't get module info about hw.dll! Stopping hooking...\n");
		return;
	}

	utils = Utils::Utils(handle, base, size);

	/* Hooking all necessary funcs */
	Hook(EmitWaterPolys);

	MH_EnableHook(MH_ALL_HOOKS);
}