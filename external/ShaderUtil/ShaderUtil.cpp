#include "hud.h"
#include "ShaderUtil.h"
#include "PlatformHeaders.h"
#include <GL/glew.h>
#include <iostream>
#include <fstream>


unsigned int ShaderUtil::GetCompiledShader(unsigned int shader_type, const std::string& shader_source)
{
	unsigned int shader_id = glCreateShader(shader_type);

	const char* c_source = shader_source.c_str();
	glShaderSource(shader_id, 1, &c_source, nullptr);
	glCompileShader(shader_id);

	GLint result;
	glGetShaderiv(shader_id, GL_COMPILE_STATUS, &result);

	if (result == GL_FALSE)
	{
		int length;
		glGetShaderiv(shader_id, GL_INFO_LOG_LENGTH, &length);

		GLchar* strInfoLog = new GLchar[length + 1];
		glGetShaderInfoLog(shader_id, length, &length, strInfoLog);

		//fprintf(stderr, "Compilation error in shader: %s\n", strInfoLog);
		gEngfuncs.Con_DPrintf("[GLEW] Compilation error in shader: %s\n[GLEW] ERROR: %s\n", shader_source.c_str(), strInfoLog);

		delete[] strInfoLog;
	}
	else
		gEngfuncs.Con_DPrintf("[GLEW] Compiling %s shader success!\n", shader_source.c_str());

	return shader_id;
}

bool ShaderUtil::Load(const std::string& vertexShaderFile, const std::string& fragmentShaderFile)
{

	mProgramId = glCreateProgram();

	unsigned int vs = GetCompiledShader(GL_VERTEX_SHADER, vertexShaderFile);
	unsigned int fs = GetCompiledShader(GL_FRAGMENT_SHADER, fragmentShaderFile);

	glAttachShader(mProgramId, vs);
	glAttachShader(mProgramId, fs);

	glLinkProgram(mProgramId);
	glValidateProgram(mProgramId);

	glDeleteShader(vs);
	glDeleteShader(fs);

	return true;
}

void ShaderUtil::Use()
{
	glUseProgram(mProgramId);
}

void ShaderUtil::Delete()
{
	glDeleteProgram(mProgramId);
}

