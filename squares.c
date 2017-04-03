#include <GLES2/gl2.h>
#include <stdio.h>
#include "eglut.h"

static GLuint position_l,
              projection_l,
              model_l,
              color_l;

static const GLfloat square[] = {
     0.0f,  1.0f,
    -1.0f,  1.0f,
    -1.0f,  0.0f,
     0.0f,  0.0f
};

GLfloat projection[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 1.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

GLfloat model[] = {
    1.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 1.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 0.0f,
    0.0f, 0.0f, 0.0f, 1.0f
};

void draw_square(GLfloat x,
                 GLfloat y,
                 GLfloat s,
                 GLfloat *color) {
    model[12] = x;
    model[13] = y;
    model[0] = s;
    model[5] = s;

    glUniformMatrix4fv(projection_l, 1, GL_FALSE, projection);
    glUniformMatrix4fv(model_l, 1, GL_FALSE, model);
    glUniform4fv(color_l, 1, color);

    glVertexAttribPointer(position_l, 2, GL_FLOAT, GL_FALSE, 0, square);
    glEnableVertexAttribArray(position_l);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    model[12] = 0;
    model[13] = 0;
    model[0] = 1;
    model[5] = 1;
    
    return;
}

void squares() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    draw_square(0, 0, 1, (GLfloat[]){0.0f, 1.0f, 1.0f, 1.0f});
    draw_square(1, 0, 1, (GLfloat[]){1.0f, 1.0f, 0.0f, 1.0f});
    draw_square(0, -1, 1, (GLfloat[]){1.0f, 0.0f, 1.0f, 1.0f});
    draw_square(1, -1, 1, (GLfloat[]){1.0f, 1.0f, 1.0f, 1.0f});

    eglutPostRedisplay();
}

int main(int argc, char **argv) {
    eglutInitWindowSize(512, 512);
    eglutInitAPIMask(EGLUT_OPENGL_ES2_BIT);
    eglutInit(argc, argv);

    eglutCreateWindow("Squares");

    eglutDisplayFunc(squares);

    /* OpenGL */

    glEnable(GL_CULL_FACE);
    glEnable(GL_DEPTH_TEST);

    GLuint s_v, s_f, p;
    char msg[512];

    static const char *src_v = "uniform mat4 projection;\n"
                               "uniform mat4 model;\n"
                               "uniform vec4 color_u;\n"
                               "attribute vec2 position;\n"
                               "varying vec4 color;\n"
                               "void main() {"
                                   "color = color_u;\n"
                                   "gl_Position = model * vec4(position, 0, 1) * projection;"
                               "}";
    static const char *src_f = "precision mediump float;\n"
                               "varying vec4 color;\n"
                               "void main() {"
                                   "gl_FragColor = color;"
                               "}";

    s_v = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(s_v, 1, &src_v, NULL);
    glCompileShader(s_v);
    glGetShaderInfoLog(s_v, sizeof msg, NULL, msg);
    printf("vertex shader info: %s\n", msg);

    s_f = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(s_f, 1, &src_f, NULL);
    glCompileShader(s_f);
    glGetShaderInfoLog(s_f, sizeof msg, NULL, msg);
    printf("fragment shader info: %s\n", msg);

    p = glCreateProgram();
    glAttachShader(p, s_v);
    glAttachShader(p, s_f);
    glLinkProgram(p);
    glUseProgram(p);

    projection_l = glGetUniformLocation(p, "projection");
    model_l = glGetUniformLocation(p, "model");
    color_l = glGetUniformLocation(p, "color_u");
    position_l = glGetAttribLocation(p, "position");

    eglutMainLoop();

    return 0;
}