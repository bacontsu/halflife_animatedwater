/***
 *
 *	Copyright (c) 2022, Bacontsu. All rights reserved
 *	Copyright (c) 1996-2002, Valve LLC. All rights reserved.
 *
 *	This product contains software technology licensed from Id
 *	Software, Inc. ("Id Technology").  Id Technology (c) 1996 Id Software, Inc.
 *	All Rights Reserved.
 *
 *   Use, distribution, and modification of this source code and/or resulting
 *   object code is restricted to non-commercial enhancements to products from
 *   Valve LLC.  All other use, distribution, or modification is prohibited
 *   without written permission from Valve LLC.
 *
 ****/
//
// waterrenderer.cpp
//
// in this file, we alter the rendering of normal water plane and modifies the texture
//


#include <filesystem>
#include "stdio.h"
#include "stdlib.h"

#include "hud.h"
#include "cl_util.h"
#include "parsemsg.h"
#include <string.h>
#include "cl_entity.h"
#include "com_model.h"

#include "PlatformHeaders.h"
#include "SDL2/SDL.h"
#include <GL/glew.h>
#include <gl/GL.h>
#include "ShaderUtil/ShaderUtil.h"


#ifndef GL_TEXTURE_RECTANGLE_NV
#define GL_TEXTURE_RECTANGLE_NV 0x84F5
#endif // !GL_TEXTURE_RECTANGLE_NV

std::string str_tolower(std::string s)
{
	std::transform(s.begin(), s.end(), s.begin(),
		// static_cast<int(*)(int)>(std::tolower)         // wrong
		// [](int c){ return std::tolower(c); }           // wrong
		// [](char c){ return std::tolower(c); }          // wrong
		[](unsigned char c)
		{ return std::tolower(c); } // correct
	);
	return s;
}

cvar_t* m_pCvarDrawAnimatedWater = NULL;

struct CWaterSurface
{
	texture_t* pointer;
	GLuint default_tex;
	int tex_height;
	int tex_width;
};

//==============================
//		TEXTURE LOADER STRUCTS
//
//==============================
struct cl_texture_t
{
	std::string szName;

	GLuint iIndex;

	int iBpp;
	unsigned int iWidth;
	unsigned int iHeight;
};


typedef struct
{
	char identification[4]; // should be WAD2/WAD3
	int numlumps;
	int infotableofs;
} wadinfo_t;

typedef struct
{
	int filepos;
	int disksize;
	int size;
	char type;
	char compression;
	char pad1, pad2;
	char name[16];
} lumpinfo_t;

struct wadfile_t
{
	byte* wadfile;
	wadinfo_t* info;

	lumpinfo_t* lumps;
	int numlumps;
};

// Detect litte/big endian
byte bLittleEndian[2] = {1, 0};
unsigned short ByteToUShort(byte* byte)
{
	if (*(short*)bLittleEndian == 1)
		return (byte[0] + (byte[1] << 8));
	else
		return (byte[1] + (byte[0] << 8));
}

unsigned int ByteToUInt(byte* byte)
{
	unsigned int iValue = byte[0];
	iValue += (byte[1] << 8);
	iValue += (byte[2] << 16);
	iValue += (byte[3] << 24);

	return iValue;
}
std::vector<wadfile_t> g_WADS;

std::vector<CWaterSurface> waterBuffer;
std::vector<cl_texture_t> waterTextures;


int stage = 0;
float nextFrame = 0;
GLuint screenHandler;
GLenum glew;
ShaderUtil shaderUtil;

int CWaterRenderer::Init()
{

	// create a load of blank pixels to create textures with
	unsigned char* pBlankTex = new unsigned char[ScreenWidth * ScreenHeight * 3];
	memset(pBlankTex, 0, ScreenWidth * ScreenHeight * 3);

	// Create the SCREEN-HOLDING TEXTURE
	glGenTextures(1, &screenHandler);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, screenHandler);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGB8, ScreenWidth, ScreenHeight, 0, GL_RGB8, GL_UNSIGNED_BYTE, pBlankTex);

	// free the memory
	delete[] pBlankTex;

	LoadWADFiles();
	LoadWADTextures();
	FreeWADFiles();

	m_pCvarDrawAnimatedWater = CVAR_CREATE("r_animate_water", "1", FCVAR_ARCHIVE);

	glew = glewInit();
	if (GLEW_OK != glew)
	{
		/* Problem: glewInit failed, something is seriously wrong. */
		// fprintf(stderr, "Error: %s\n", glewGetErrorString(err));
		gEngfuncs.Con_DPrintf("[GLEW] Error: %s\n", glewGetErrorString(glew));
	}
	else
	{
		gEngfuncs.Con_DPrintf("[GLEW] Initialize success!\n");
		shaderUtil.Load(std::string(gEngfuncs.pfnGetGameDirectory() + (std::string) "/shaders/vs.shader"), std::string(gEngfuncs.pfnGetGameDirectory() + (std::string) "/shaders/fs.shader"));
	}

	return 1;
}

int CWaterRenderer::VidInit()
{
	waterBuffer.clear();
	stage = 0;
	nextFrame = 0;

	return 1;
}

/*
====================
LoadWADFiles

====================
*/
void CWaterRenderer::LoadWADFiles(void)
{
	const char* szFile[] = {"liquids.wad", "halflife.wad"};


	for (int i = 0; i < ARRAYSIZE(szFile); i++)
	{
		int iSize = 0;
		byte* pFile = gEngfuncs.COM_LoadFile(szFile[i], 5, &iSize);
		if (!pFile)
			return;

		wadinfo_t* pInfo = (wadinfo_t*)pFile;
		if (strncmp("WAD3", pInfo->identification, 4))
		{
			gEngfuncs.COM_FreeFile(pFile);
			return;
		}

		g_WADS.emplace_back();

		wadfile_t* pWADFile = &g_WADS.back();

		pWADFile->wadfile = pFile;
		pWADFile->info = (wadinfo_t*)pWADFile->wadfile;

		pWADFile->lumps = new lumpinfo_t[pWADFile->info->numlumps];
		memcpy(pWADFile->lumps, (pWADFile->wadfile + pWADFile->info->infotableofs), sizeof(lumpinfo_t) * pWADFile->info->numlumps);
		pWADFile->numlumps = pWADFile->info->numlumps;
	}
}

/*
====================
FreeWADFiles

====================
*/
void CWaterRenderer::FreeWADFiles(void)
{
	for (size_t i = 0; i < g_WADS.size(); i++)
	{
		delete[] g_WADS[i].lumps;
		gEngfuncs.COM_FreeFile(g_WADS[i].wadfile);
	}
}

/*
====================
LoadPallettedTexture

====================
*/
void LoadPallettedTexture(byte* data, byte* pal, cl_texture_t* pTexture)
{
	int row1[1024], row2[1024], col1[1024], col2[1024];
	byte *pix1, *pix2, *pix3, *pix4;
	byte alpha1, alpha2, alpha3, alpha4;

	// convert texture to power of 2
	unsigned int outwidth;
	for (outwidth = 1; outwidth < pTexture->iWidth; outwidth <<= 1)
		;
	if (outwidth > 1024)
		outwidth = 1024;

	unsigned int outheight;
	for (outheight = 1; outheight < pTexture->iHeight; outheight <<= 1)
		;
	if (outheight > 1024)
		outheight = 1024;

	byte *out, *tex;
	tex = out = new byte[outwidth * outheight * 4];

	for (unsigned int i = 0; i < outwidth; i++)
	{
		col1[i] = (int)((i + 0.25) * (pTexture->iWidth / (float)outwidth));
		col2[i] = (int)((i + 0.75) * (pTexture->iWidth / (float)outwidth));
	}

	for (unsigned int i = 0; i < outheight; i++)
	{
		row1[i] = (int)((i + 0.25) * (pTexture->iHeight / (float)outheight)) * pTexture->iWidth;
		row2[i] = (int)((i + 0.75) * (pTexture->iHeight / (float)outheight)) * pTexture->iWidth;
	}

	for (unsigned int i = 0; i < outheight; i++)
	{
		for (unsigned int j = 0; j < outwidth; j++, out += 4)
		{
			pix1 = &pal[data[row1[i] + col1[j]] * 3];
			pix2 = &pal[data[row1[i] + col2[j]] * 3];
			pix3 = &pal[data[row2[i] + col1[j]] * 3];
			pix4 = &pal[data[row2[i] + col2[j]] * 3];
			alpha1 = 0xFF;
			alpha2 = 0xFF;
			alpha3 = 0xFF;
			alpha4 = 0xFF;

			if (pTexture->szName[0] == '{')
			{
				if (data[row1[i] + col1[j]] == 0xFF)
				{
					pix1[0] = 0;
					pix1[1] = 0;
					pix1[2] = 0;
					alpha1 = 0;
				}

				if (data[row1[i] + col2[j]] == 0xFF)
				{
					pix2[0] = 0;
					pix2[1] = 0;
					pix2[2] = 0;
					alpha2 = 0;
				}

				if (data[row2[i] + col1[j]] == 0xFF)
				{
					pix3[0] = 0;
					pix3[1] = 0;
					pix3[2] = 0;
					alpha3 = 0;
				}

				if (data[row2[i] + col2[j]] == 0xFF)
				{
					pix4[0] = 0;
					pix4[1] = 0;
					pix4[2] = 0;
					alpha4 = 0;
				}
			}

			out[0] = (pix1[0] + pix2[0] + pix3[0] + pix4[0]) >> 2;
			out[1] = (pix1[1] + pix2[1] + pix3[1] + pix4[1]) >> 2;
			out[2] = (pix1[2] + pix2[2] + pix3[2] + pix4[2]) >> 2;
			out[3] = (alpha1 + alpha2 + alpha3 + alpha4) >> 2;
		}
	}

	glGenTextures(1, &pTexture->iIndex);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, pTexture->iIndex);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
	//glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_GENERATE_MIPMAP, GL_TRUE);
	// Sets the wrap parameters in both directions
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_S, GL_REPEAT);
	glTexParameteri(GL_TEXTURE_RECTANGLE_NV, GL_TEXTURE_WRAP_T, GL_REPEAT);
	glTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, outwidth, outheight, 0, GL_RGBA, GL_UNSIGNED_BYTE, tex);

	pTexture->iWidth = outwidth;
	pTexture->iHeight = outheight;

	delete[] tex;
}

/*
====================
LoadWADTexture

====================
*/
void CWaterRenderer::LoadWADTextures()
{
	std::string szName;
	for (size_t i = 0; i < g_WADS.size(); i++)
	{
		byte* pFile = g_WADS[i].wadfile;
		wadinfo_t* pInfo = g_WADS[i].info;
		for (int j = 0; j < pInfo->numlumps; j++)
		{
			cl_texture_t cl_texture{};

			lumpinfo_t* pLump = &g_WADS[i].lumps[j];
			if (pLump->type != 0 && !(pLump->type & 0x43))
				continue;

			szName = str_tolower(pLump->name);

			if ((!strncmp(szName.c_str(), "water", 5) || szName[0] == '!'))
			{
				cl_texture.szName = szName;

				cl_texture.iWidth = ByteToUInt(pFile + pLump->filepos + 16);
				cl_texture.iHeight = ByteToUInt(pFile + pLump->filepos + 20);
				cl_texture.iBpp = 4;

				// Get offsets
				int iIndexOffset = ByteToUInt(pFile + pLump->filepos + 24);
				int iMip3Offset = ByteToUInt(pFile + pLump->filepos + 36);

				byte* pPalette;
				if (pLump->type & 0x43)
					pPalette = pFile + pLump->filepos + iMip3Offset + ((cl_texture.iWidth / 8) * (cl_texture.iHeight / 8)) + 2;
				else
					pPalette = pFile + pLump->filepos + iIndexOffset + (cl_texture.iWidth * cl_texture.iHeight) + 2;

				byte* pPixels = pFile + pLump->filepos + iIndexOffset;
				LoadPallettedTexture(pPixels, pPalette, &cl_texture);

				waterTextures.push_back(cl_texture);
			}
		}
	}
}


int CWaterRenderer::Draw()
{
	// Initialization, search all entities with "!" texture flag
	if (stage == 0)
	{
		// Scan for all existing entities, insert to waterSurface list
		for (int i = 0; i < 512; i++)
		{
			if (auto ent = gEngfuncs.GetEntityByIndex(i))
			{
				if (ent && ent->model && ent->model->textures)
				{
					// Scan for all textures on that (brush entity)
					for (int j = 0; j < ent->model->numtextures; j++)
					{
						if (ent->model->textures[j] && (!strncmp(ent->model->textures[j]->name, "water", 5) || ent->model->textures[j]->name[0] == '!'))
						{
							// Register this pointer to the buffer
							CWaterSurface local;
							local.pointer = ent->model->textures[j];
							waterBuffer.push_back(local);
						}
					}
				}
			}
		}

		stage = 1;
	}
	else if (stage == 1)
	{
		// Load all the textures here, then Get max_frame value for each buffer
		for (size_t i = 0; i < waterBuffer.size(); i++)
		{
			for (size_t j = 0; j < waterTextures.size(); j++)
			{
				if (!stricmp(waterTextures[j].szName.c_str(), waterBuffer[i].pointer->name))
				{
					// Assign the data(s) onto the buffer
					waterBuffer[i].default_tex = waterTextures[j].iIndex;		// waterTextures[j].iIndex;
					waterBuffer[i].tex_width = waterBuffer[i].pointer->width;	// waterTextures[j].iWidth;
					waterBuffer[i].tex_height = waterBuffer[i].pointer->height; // waterTextures[j].iHeight;
					break;
				}
			}
		}

		stage = 2;
	}
	return true;
}

void CWaterRenderer::DrawQuad(int width, int height, int ofsX, int ofsY)
{
	glTexCoord2f(ofsX, ofsY);
	glVertex3f(0, 1, -1);
	glTexCoord2f(ofsX, height + ofsY);
	glVertex3f(0, 0, -1);
	glTexCoord2f(width + ofsX, height + ofsY);
	glVertex3f(1, 0, -1);
	glTexCoord2f(width + ofsX, ofsY);
	glVertex3f(1, 1, -1);
}

void CWaterRenderer::AnimateWater()
{
	if (m_pCvarDrawAnimatedWater->value <= 0)
		return;

	if (stage != 2)
		return;

	// enable some OpenGL stuff
	glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
	glEnable(GL_BLEND);

	glEnable(GL_TEXTURE_RECTANGLE_NV);
	glColor3f(1, 1, 1);
	glDisable(GL_DEPTH_TEST);
	glMatrixMode(GL_MODELVIEW);
	glPushMatrix();
	glLoadIdentity();
	glMatrixMode(GL_PROJECTION);
	glPushMatrix();
	glLoadIdentity();
	glOrtho(0, 1, 1, 0, 0.1, 100);

	// save screen
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, screenHandler);
	glCopyTexImage2D(GL_TEXTURE_RECTANGLE_NV, 0, GL_RGBA8, 0, 0, ScreenWidth, ScreenHeight, 0);

	// scroll the texture
	static float offset = 0.0f;
	if (nextFrame < gEngfuncs.GetAbsoluteTime())
	{
		offset += 0.005f;
		if (offset > 1)
			offset = 0;
		nextFrame = gEngfuncs.GetAbsoluteTime() + 0.01f;
	}
	// renderpasses
	for (size_t i = 0; i < waterBuffer.size(); i++)
	{
		if (waterBuffer[i].default_tex != 0)
		{
			glViewport(0, 0, waterBuffer[i].tex_width, waterBuffer[i].tex_height);
			glBindTexture(GL_TEXTURE_RECTANGLE_NV, waterBuffer[i].default_tex);
			glColor4f(1, 1, 1, 1);

			shaderUtil.Use();

			// ===== this layer stays still =====
			glBegin(GL_QUADS);
			glTexCoord2f(waterBuffer[i].tex_width, 0);
			glVertex3f(0, 1, -1);
			glTexCoord2f(waterBuffer[i].tex_width, waterBuffer[i].tex_height);
			glVertex3f(0, 0, -1);
			glTexCoord2f(0, waterBuffer[i].tex_height);
			glVertex3f(1, 0, -1);
			glTexCoord2f(0, 0);
			glVertex3f(1, 1, -1);
			glEnd();

			glUseProgram(0);

			/*
			// ===== this layer scrolls horizontally =====
			glPushMatrix();
			glColor4f(1, 1, 1, 0.5f);
			glTranslatef(offset, 0.0f, 0.0f);
			glBegin(GL_QUADS);
			DrawQuad(waterBuffer[i].tex_width, waterBuffer[i].tex_height, 0, 0);
			glEnd();
			glPopMatrix();

			glPushMatrix();
			glColor4f(1, 1, 1, 0.5f);
			glTranslatef(offset - 1, 0.0f, 0.0f);
			glBegin(GL_QUADS);
			DrawQuad(waterBuffer[i].tex_width, waterBuffer[i].tex_height, 0, 0);
			glEnd();
			glPopMatrix();

			// ===== this layer scrolls vertically =====
			glPushMatrix();
			glColor4f(1, 1, 1, 0.5f);
			glTranslatef(0.0f, offset, 0.0f);
			glBegin(GL_QUADS);
			glTexCoord2f(waterBuffer[i].tex_width, 0);
			glVertex3f(0, 1, -1);
			glTexCoord2f(0, 0);
			glVertex3f(0, 0, -1);
			glTexCoord2f(0, waterBuffer[i].tex_height);
			glVertex3f(1, 0, -1);
			glTexCoord2f(waterBuffer[i].tex_width, waterBuffer[i].tex_height);
			glVertex3f(1, 1, -1);
			glEnd();
			glPopMatrix();

			glPushMatrix();
			glColor4f(1, 1, 1, 0.5f);
			glTranslatef(0.0f, offset - 1, 0.0f);
			glBegin(GL_QUADS);
			glTexCoord2f(waterBuffer[i].tex_width, 0);
			glVertex3f(0, 1, -1);
			glTexCoord2f(0, 0);
			glVertex3f(0, 0, -1);
			glTexCoord2f(0, waterBuffer[i].tex_height);
			glVertex3f(1, 0, -1);
			glTexCoord2f(waterBuffer[i].tex_width, waterBuffer[i].tex_height);
			glVertex3f(1, 1, -1);
			glEnd();
			glPopMatrix();
			*/
			
			// update water texture
			glBindTexture(GL_TEXTURE_2D, waterBuffer[i].pointer->gl_texturenum);
			glCopyTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, 0, 0, waterBuffer[i].tex_width, waterBuffer[i].tex_height, 0);
		}
	}
	// restore screen content
	glViewport(0, 0, ScreenWidth, ScreenHeight);
	glBindTexture(GL_TEXTURE_RECTANGLE_NV, screenHandler);
	glColor4f(1, 1, 1, 1);

	//glBegin(GL_QUADS);
	//DrawQuad(ScreenWidth, ScreenHeight, 0, 0);
	//glEnd();
	//glBegin(GL_QUADS);
	//DrawQuad(ScreenWidth, ScreenHeight, 0, 0);
	//glEnd();

	// reset state
	glMatrixMode(GL_PROJECTION);
	glPopMatrix();
	glMatrixMode(GL_MODELVIEW);
	glPopMatrix();
	glDisable(GL_TEXTURE_RECTANGLE_NV);
	glEnable(GL_DEPTH_TEST);
	glDisable(GL_BLEND);
}