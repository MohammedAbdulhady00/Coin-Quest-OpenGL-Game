#include "shader.hpp"

#include <cassert>
#include <iostream>
#include <fstream>
#include <string>
#include <filesystem>

// include the stb library
// allowing us to use include in glsl files
#define STB_INCLUDE_LINE_GLSL
#define STB_INCLUDE_IMPLEMENTATION
#include <stb/stb_include.h>

// Forward definition for error checking functions
/**
 * checks if there is an error in the given shader. You should use it to check if there is a
 *  compilation error and print it so that you can know what is wrong with
 *  the shader. The returned string will be empty if there is no errors.
 * @param shader: the shader to check for errors
 */
std::string checkForShaderCompilationErrors(GLuint shader);

/**
 * checks if there is an error in the given program.
 * You should use it to check if there is a
 * linking error and print it so that you can know what is wrong with the
 * program. The returned string will be empty if there is no errors.
 * @param program: the program to check for errors.
 */
std::string checkForLinkingErrors(GLuint program);

bool our::ShaderProgram::attach(const std::string &filename, GLenum type) const
{
    // first, we use C++17 filesystem library to get the directory (parent) path of the file.
    // the parent path will be sent to stb_include to search for files referenced by any "#include" preprocessor command.
    auto file_path = std::filesystem::path(filename);
    auto file_path_string = file_path.string();
    auto parent_path_string = file_path.parent_path().string();
    auto path_to_includes = &(parent_path_string[0]);
    char stb_error[256];

    // Read the file as a string and resolve any "#include"s recursively
    auto source = stb_include_file(&(file_path_string[0]), nullptr, path_to_includes, stb_error);

    // Check if any loading errors happened
    if (source == nullptr)
    {
        std::cerr << "ERROR: " << stb_error << std::endl;
        return false;
    }

    // // Here, we open the file and read a string from it containing the GLSL code of our shader
    // std::ifstream file(filename);
    // if (!file)
    // {
    //     std::cerr << "ERROR: Couldn't open shader file: " << filename << std::endl;
    //     return false;
    // }
    // std::string sourceString = std::string(std::istreambuf_iterator<char>(file), std::istreambuf_iterator<char>());
    // const char *sourceCStr = sourceString.c_str();
    // file.close();

    // // TODO: Complete this function

    // create a shader object with the given type
    GLuint shader = glCreateShader(type);
    // attach the source code to the shader object
    glShaderSource(shader, 1, &source, nullptr);
    // compile the shader
    glCompileShader(shader);

    // check for compilation errors
    std::string error = checkForShaderCompilationErrors(shader);
    if (!error.empty())
    {
        std::cerr << "ERROR: Couldn't compile shader: " << filename << std::endl;
        std::cerr << error << std::endl;
        return false;
    }

    // attach the shader to the program
    glAttachShader(program, shader);
    // delete the shader object
    glDeleteShader(shader);
    // We return true if the compilation succeeded
    return true;
}

bool our::ShaderProgram::link() const
{
    // // TODO: Complete this function
    // link the program
    glLinkProgram(program);

    // check for linking errors
    std::string error = checkForLinkingErrors(program);
    if (!error.empty())
    {
        std::cerr << "ERROR: Couldn't link shader program" << std::endl;
        std::cerr << error << std::endl;
        return false;
    }
    return true;
}

////////////////////////////////////////////////////////////////////
// Function to check for compilation and linking error in shaders //
////////////////////////////////////////////////////////////////////

std::string checkForShaderCompilationErrors(GLuint shader)
{
    // Check and return any error in the compilation process
    GLint status;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &status);
    if (!status)
    {
        GLint length;
        glGetShaderiv(shader, GL_INFO_LOG_LENGTH, &length);
        char *logStr = new char[length];
        glGetShaderInfoLog(shader, length, nullptr, logStr);
        std::string errorLog(logStr);
        delete[] logStr;
        return errorLog;
    }
    return std::string();
}

std::string checkForLinkingErrors(GLuint program)
{
    // Check and return any error in the linking process
    GLint status;
    glGetProgramiv(program, GL_LINK_STATUS, &status);
    if (!status)
    {
        GLint length;
        glGetProgramiv(program, GL_INFO_LOG_LENGTH, &length);
        char *logStr = new char[length];
        glGetProgramInfoLog(program, length, nullptr, logStr);
        std::string error(logStr);
        delete[] logStr;
        return error;
    }
    return std::string();
}