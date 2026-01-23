/*
* (C) 2015 see Authors.txt
*
* This file is part of MPC-HC.
*
* MPC-HC is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License as published by
* the Free Software Foundation; either version 3 of the License, or
* (at your option) any later version.
*
* MPC-HC is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*
* You should have received a copy of the GNU General Public License
* along with this program.  If not, see <http://www.gnu.org/licenses/>.
*
*/

#include "stdafx.h"
#include "ColorConvTable.h"

/************************************
Formula:
Ey => [0,1]
Eu => [-0.5,0.5]
Ev => [-0.5,0.5]
Er => [0,1]
Eg => [0,1]
Eb => [0,1]

1  = Kr + Kg + Kb
Ey = Kr * Er + Kg * Eg + Kb * Eb;
Eu = (Eb - Ey) / (1 - Kb) / 2;
Ev = (Er - Ey) / (1 - Kr) / 2;

Quantization:
ANY = ANY * RANGE_SIZE + BASE

Range:
TV Level
Y => [16, 235]
U => [16, 240]
V => [16, 240]

R => [16, 235]
G => [16, 235]
B => [16, 235]
PC Level
Y => [0,255]
U => [0,255]
V => [0,255]

R => [0,255]
G => [0,255]
B => [0,255]
************************************/

//RGB to YUV
#define DEFINE_YUV_MATRIX(Kr,Kg,Kb) {                        \
        {   Kr            ,  Kg           ,   Kb            , 0},\
        {  -Kr /((1-Kb)*2), -Kg/((1-Kb)*2),(1-Kb)/((1-Kb)*2), 0},\
        {(1-Kr)/((1-Kr)*2), -Kg/((1-Kr)*2),  -Kb /((1-Kr)*2), 0} \
}

//YUV to RGB: INV stand for inverse
#define DEFINE_YUV_MATRIX_INV(Kr,Kg,Kb) {       \
        {   1,  0             ,  2*(1-Kr)      , 0},\
        {   1, -2*(1-Kb)*Kb/Kg, -2*(1-Kr)*Kr/Kg, 0},\
        {   1,  2*(1-Kb)      ,  0             , 0} \
}

const float MATRIX_BT_601[3][4] = DEFINE_YUV_MATRIX(0.299f, 0.587f, 0.114f);
const float MATRIX_BT_601_INV[3][4] = DEFINE_YUV_MATRIX_INV(0.299f, 0.587f, 0.114f);
const float MATRIX_BT_709[3][4] = DEFINE_YUV_MATRIX(0.2126f, 0.7152f, 0.0722f);
const float MATRIX_BT_709_INV[3][4] = DEFINE_YUV_MATRIX_INV(0.2126f, 0.7152f, 0.0722f);
const float MATRIX_BT_2020    [3][4] = DEFINE_YUV_MATRIX    (0.2627f, 0.678f , 0.0593f);
const float MATRIX_BT_2020_INV[3][4] = DEFINE_YUV_MATRIX_INV(0.2627f, 0.678f , 0.0593f);

const float YUV_PC[3][4] = {
    { 255, 0, 0, 0 },
    { 0, 255, 0, 128 },
    { 0, 0, 255, 128 }
};
const float YUV_PC_INV[3][4] = {
    { 1 / 255.0f, 0, 0, 0 },
    { 0, 1 / 255.0f, 0, -128 / 255.0f },
    { 0, 0, 1 / 255.0f, -128 / 255.0f }
};
const float YUV_TV[3][4] = {
    { 219, 0, 0, 16 },
    { 0, 224, 0, 128 },
    { 0, 0, 224, 128 }
};
const float YUV_TV_INV[3][4] = {
    { 1 / 219.0f, 0, 0, -16 / 219.0f },
    { 0, 1 / 224.0f, 0, -128 / 224.0f },
    { 0, 0, 1 / 224.0f, -128 / 224.0f }
};
const float RGB_PC[3][4] = {
    { 255, 0, 0, 0 },
    { 0, 255, 0, 0 },
    { 0, 0, 255, 0 }
};
const float RGB_PC_INV[3][4] = {
    { 1 / 255.0f, 0, 0, 0 },
    { 0, 1 / 255.0f, 0, 0 },
    { 0, 0, 1 / 255.0f, 0 }
};
const float RGB_TV[3][4] = {
    { 219, 0, 0, 16 },
    { 0, 219, 0, 16 },
    { 0, 0, 219, 16 }
};
const float RGB_TV_INV[3][4] = {
    { 1 / 219.0f, 0, 0, -16 / 219.0f },
    { 0, 1 / 219.0f, 0, -16 / 219.0f },
    { 0, 0, 1 / 219.0f, -16 / 219.0f }
};
const float IDENTITY[3][4] = {
    { 1, 0, 0, 0 },
    { 0, 1, 0, 0 },
    { 0, 0, 1, 0 }
};

inline int clip(int value, int upper_bound)
{
    value &= ~(value >> 31); //value = value > 0 ? value : 0
    return value ^ ((value ^ upper_bound) & ((upper_bound - value) >> 31)); //value = value < upper_bound ? value : upper_bound
}

#define E(M,i,j) M[i*4+j]

void MultiplyMatrix(float* lhs_in_out, const float* rhs)
{
    float tmp1;
    float tmp2;
    float tmp3;

    tmp1 = E(lhs_in_out, 0, 0);
    tmp2 = E(lhs_in_out, 0, 1);
    tmp3 = E(lhs_in_out, 0, 2);

    E(lhs_in_out, 0, 0) = tmp1 * E(rhs, 0, 0) + tmp2 * E(rhs, 1, 0) + tmp3 * E(rhs, 2, 0);
    E(lhs_in_out, 0, 1) = tmp1 * E(rhs, 0, 1) + tmp2 * E(rhs, 1, 1) + tmp3 * E(rhs, 2, 1);
    E(lhs_in_out, 0, 2) = tmp1 * E(rhs, 0, 2) + tmp2 * E(rhs, 1, 2) + tmp3 * E(rhs, 2, 2);
    E(lhs_in_out, 0, 3) = tmp1 * E(rhs, 0, 3) + tmp2 * E(rhs, 1, 3) + tmp3 * E(rhs, 2, 3) + E(lhs_in_out, 0, 3);

    tmp1 = E(lhs_in_out, 1, 0);
    tmp2 = E(lhs_in_out, 1, 1);
    tmp3 = E(lhs_in_out, 1, 2);

    E(lhs_in_out, 1, 0) = tmp1 * E(rhs, 0, 0) + tmp2 * E(rhs, 1, 0) + tmp3 * E(rhs, 2, 0);
    E(lhs_in_out, 1, 1) = tmp1 * E(rhs, 0, 1) + tmp2 * E(rhs, 1, 1) + tmp3 * E(rhs, 2, 1);
    E(lhs_in_out, 1, 2) = tmp1 * E(rhs, 0, 2) + tmp2 * E(rhs, 1, 2) + tmp3 * E(rhs, 2, 2);
    E(lhs_in_out, 1, 3) = tmp1 * E(rhs, 0, 3) + tmp2 * E(rhs, 1, 3) + tmp3 * E(rhs, 2, 3) + E(lhs_in_out, 1, 3);

    tmp1 = E(lhs_in_out, 2, 0);
    tmp2 = E(lhs_in_out, 2, 1);
    tmp3 = E(lhs_in_out, 2, 2);

    E(lhs_in_out, 2, 0) = tmp1 * E(rhs, 0, 0) + tmp2 * E(rhs, 1, 0) + tmp3 * E(rhs, 2, 0);
    E(lhs_in_out, 2, 1) = tmp1 * E(rhs, 0, 1) + tmp2 * E(rhs, 1, 1) + tmp3 * E(rhs, 2, 1);
    E(lhs_in_out, 2, 2) = tmp1 * E(rhs, 0, 2) + tmp2 * E(rhs, 1, 2) + tmp3 * E(rhs, 2, 2);
    E(lhs_in_out, 2, 3) = tmp1 * E(rhs, 0, 3) + tmp2 * E(rhs, 1, 3) + tmp3 * E(rhs, 2, 3) + E(lhs_in_out, 2, 3);
}

class ConvFunc;
static ConvFunc& ConvFuncInst();

class ConvMatrix
{
public:
    enum LevelType {
        LEVEL_TV,
        LEVEL_PC,
        LEVEL_COUNT
    };
    enum ColorType {
        COLOR_YUV_601,
        COLOR_YUV_709,
        COLOR_YUV_2020,
        COLOR_RGB,
        COLOR_COUNT
    };
public:
    ConvMatrix();
    virtual ~ConvMatrix();

    bool Init();
#if INCLUDE_COLOR_CONVERT
    void InitMatrix(int in_level, int in_type, int out_level, int out_type);
    DWORD Convert(int x1, int x2, int x3, int in_level, int in_type, int out_level, int out_type);
#endif
    static DWORD DoConvert(int x1, int x2, int x3, const int* matrix);

    DWORD Correct601to709(int r8, int g8, int b8, int output_rgb_level);
    void InitColorCorrectionMatrix();
private:
#if INCLUDE_COLOR_CONVERT
    const float* MATRIX_DE_QUAN[LEVEL_COUNT][COLOR_COUNT];
    const float* MATRIX_INV_TRANS[COLOR_COUNT];
    const float* MATRIX_TRANS[COLOR_COUNT];
#endif
    int COLOR_CORRECTION_MATRIX[3][4];
};

ConvMatrix::ConvMatrix()
{
}

ConvMatrix::~ConvMatrix()
{
}

#if INCLUDE_COLOR_CONVERT
void ConvMatrix::InitMatrix(int in_level, int in_type, int out_level, int out_type)
{
    MATRIX_DE_QUAN[in_level][in_type] = (in_type == COLOR_RGB) ? RGB_PC_INV : (in_level == LEVEL_TV ? YUV_TV_INV : YUV_PC_INV);
    MATRIX_INV_TRANS[in_type] = (in_type == COLOR_YUV_601 ? MATRIX_BT_601_INV : (in_type == COLOR_YUV_709 ? MATRIX_BT_709_INV : MATRIX_BT_2020_INV));
    MATRIX_TRANS[out_type] = (out_type == COLOR_YUV_601 ? MATRIX_BT_601 : (out_type == COLOR_YUV_709 ? MATRIX_BT_709 : MATRIX_BT_2020));
}

DWORD ConvMatrix::Convert(int x1, int x2, int x3, int in_level, int in_type, int out_level, int out_type)
{
    int matrix[3][4] = {0};
    memcpy(matrix, MATRIX_DE_QUAN[in_level][in_type], sizeof(matrix));

    if (in_type == COLOR_RGB) {
        in_type = COLOR_YUV_601;
        if (out_type == COLOR_RGB) {
            out_type = COLOR_YUV_601;
        }
    }

    MultiplyMatrix((float*)matrix, MATRIX_INV_TRANS[in_type]);
    MultiplyMatrix((float*)matrix, MATRIX_TRANS[out_type]);
    MultiplyMatrix((float*)matrix, (out_level == LEVEL_TV ? YUV_TV : YUV_PC));

    return DoConvert(x1, x2, x3, (int*)matrix);
}
#endif

DWORD ConvMatrix::DoConvert(int x1, int x2, int x3, const int* matrix)
{
    int x = clip(int(x1 * matrix[0] + x2 * matrix[1] + x3 * matrix[2] + (matrix[3] << 8) + 128) >> 8, 255);
    int y = clip(int(x1 * matrix[4] + x2 * matrix[5] + x3 * matrix[6] + (matrix[7] << 8) + 128) >> 8, 255);
    int z = clip(int(x1 * matrix[8] + x2 * matrix[9] + x3 * matrix[10] + (matrix[11] << 8) + 128) >> 8, 255);
    return (x << 16) | (y << 8) | z;
}

DWORD ConvMatrix::Correct601to709(int r8, int g8, int b8, int output_rgb_level)
{
    int matrix[3][4];
    memcpy(matrix, COLOR_CORRECTION_MATRIX, sizeof(matrix));
    if (output_rgb_level == LEVEL_TV) {
        MultiplyMatrix((float*)matrix, &RGB_TV[0][0]);
    }
    return DoConvert(r8, g8, b8, (int*)matrix);
}

ConvMatrix m_convMatrix;

class ConvFunc
{
public:
    ConvFunc(ColorConvTable::YuvMatrixType yuv_type, ColorConvTable::YuvRangeType range, bool bOutputTVRange, bool bVSFilterCorrection);
    bool InitConvFunc(ColorConvTable::YuvMatrixType yuv_type, ColorConvTable::YuvRangeType range);

    typedef DWORD(*R8G8B8ToYuvFunc)(int r8, int g8, int b8);
    typedef R8G8B8ToYuvFunc Y8U8V8ToRGBFunc;
    ColorConvTable::YuvMatrixType m_videoYuvType;
    ColorConvTable::YuvRangeType  m_videoRangeType;
    bool          m_bOutputTVRange;
    bool          m_bCorrect601to709;

    ConvMatrix    m_convMatrix; //for YUV to YUV or other complicated conversions
};

static ConvFunc& ConvFuncInst()
{
    static ConvFunc s(ColorConvTable::BT601, ColorConvTable::RANGE_TV, false, false);
    return s;
}

void ConvMatrix::InitColorCorrectionMatrix()
{
    float matrix[3][4] = {0};
    memcpy(matrix, RGB_PC_INV, sizeof(matrix));
    MultiplyMatrix((float*)matrix, &MATRIX_BT_601[0][0]);
    MultiplyMatrix((float*)matrix, (ConvFuncInst().m_videoRangeType == ColorConvTable::RANGE_TV ? &YUV_TV[0][0] : &YUV_PC[0][0]));
    MultiplyMatrix((float*)matrix, (ConvFuncInst().m_videoRangeType == ColorConvTable::RANGE_TV ? &YUV_TV_INV[0][0] : &YUV_PC_INV[0][0]));
    MultiplyMatrix((float*)matrix, &MATRIX_BT_709_INV[0][0]);
    MultiplyMatrix((float*)matrix, &RGB_PC[0][0]);

    memcpy(COLOR_CORRECTION_MATRIX, matrix, sizeof(COLOR_CORRECTION_MATRIX));
}

ConvFunc::ConvFunc(ColorConvTable::YuvMatrixType yuv_type, ColorConvTable::YuvRangeType range, bool bOutputTVRange, bool bCorrect601to709)
    : m_bOutputTVRange(bOutputTVRange)
    , m_bCorrect601to709(bCorrect601to709)
    , m_videoYuvType(yuv_type)
    , m_videoRangeType(range)
{
#if INCLUDE_COLOR_CONVERT
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_2020);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_2020);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_2020);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_2020);

    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_2020);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_2020);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_2020);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_2020);

    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_2020,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_2020,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_2020,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_601);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_2020,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_601);

    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_2020,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_2020,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_2020,
        ConvMatrix::LEVEL_TV, ConvMatrix::COLOR_YUV_709);
    m_convMatrix.InitMatrix(
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_2020,
        ConvMatrix::LEVEL_PC, ConvMatrix::COLOR_YUV_709);
#endif
}

//
// YUV to RGB conversion
//

#define RGB_LVL_TV 219
#define RGB_LVL_PC 255
#define YUV_LVL_TV 219
#define YUV_LVL_PC 255
#define FRACTION_SCALE (1<<16)

#define DEFINE_YUV2RGB_FUNC(FUNC_NAME, RGB_LVL, YUV_LVL, Kr, Kg, Kb) \
static DWORD FUNC_NAME(int y8, int u8, int v8) {                     \
    int y, u, v, r, g, b;                                            \
    y = y8 * RGB_LVL / YUV_LVL;                                      \
    u = u8 - 128;                                                    \
    v = v8 - 128;                                                    \
    r = clip(int(y + ((1.0 - Kr) / 0.5) * v), 255);                  \
    g = clip(int(y - ((1.0 - Kb) * Kb / Kg / 0.5) * u \
            - ((1.0 - Kr) * Kr / Kg / 0.5) * v), 255);               \
    b = clip(int(y + ((1.0 - Kb) / 0.5) * u), 255);                  \
    return (r << 16) | (g << 8) | b;                                 \
}

DEFINE_YUV2RGB_FUNC(YUV_TV_TO_RGB_PC_601, RGB_LVL_PC, YUV_LVL_TV, 0.299, 0.587, 0.114)
DEFINE_YUV2RGB_FUNC(YUV_PC_TO_RGB_PC_601, RGB_LVL_PC, YUV_LVL_PC, 0.299, 0.587, 0.114)
DEFINE_YUV2RGB_FUNC(YUV_TV_TO_RGB_PC_709, RGB_LVL_PC, YUV_LVL_TV, 0.2126, 0.7152, 0.0722)
DEFINE_YUV2RGB_FUNC(YUV_PC_TO_RGB_PC_709, RGB_LVL_PC, YUV_LVL_PC, 0.2126, 0.7152, 0.0722)
DEFINE_YUV2RGB_FUNC(YUV_TV_TO_RGB_PC_2020, RGB_LVL_PC, YUV_LVL_TV, 0.2627, 0.678, 0.0593)
DEFINE_YUV2RGB_FUNC(YUV_PC_TO_RGB_PC_2020, RGB_LVL_PC, YUV_LVL_PC, 0.2627, 0.678, 0.0593)

DEFINE_YUV2RGB_FUNC(YUV_TV_TO_RGB_TV_601, RGB_LVL_TV, YUV_LVL_TV, 0.299, 0.587, 0.114)
DEFINE_YUV2RGB_FUNC(YUV_PC_TO_RGB_TV_601, RGB_LVL_TV, YUV_LVL_PC, 0.299, 0.587, 0.114)
DEFINE_YUV2RGB_FUNC(YUV_TV_TO_RGB_TV_709, RGB_LVL_TV, YUV_LVL_TV, 0.2126, 0.7152, 0.0722)
DEFINE_YUV2RGB_FUNC(YUV_PC_TO_RGB_TV_709, RGB_LVL_TV, YUV_LVL_PC, 0.2126, 0.7152, 0.0722)
DEFINE_YUV2RGB_FUNC(YUV_TV_TO_RGB_TV_2020, RGB_LVL_TV, YUV_LVL_TV, 0.2627, 0.678, 0.0593)
DEFINE_YUV2RGB_FUNC(YUV_PC_TO_RGB_TV_2020, RGB_LVL_TV, YUV_LVL_PC, 0.2627, 0.678, 0.0593)

//
// ColorConvTable
//
void ColorConvTable::SetDefaultConvType(YuvMatrixType yuv_type, YuvRangeType range, bool bOutputTVRange, bool bCorrect601to709)
{
    ConvFuncInst().m_videoYuvType = yuv_type;
    ConvFuncInst().m_videoRangeType = range;
    ConvFuncInst().m_bOutputTVRange = bOutputTVRange;
    ConvFuncInst().m_bCorrect601to709 = bCorrect601to709;
}

DWORD ColorConvTable::RGB_PC_TO_TV(DWORD argb)
{
    const int MIN = 16;
    const int SCALE = int(219.0 / 255 * FRACTION_SCALE + 0.5);
    DWORD r = (argb & 0x00ff0000) >> 16;
    DWORD g = (argb & 0x0000ff00) >> 8;
    DWORD b = (argb & 0x000000ff);
    r = ((r * SCALE) >> 16) + MIN;
    g = ((g * SCALE) >> 16) + MIN;
    b = ((b * SCALE) >> 16) + MIN;
    return (argb & 0xff000000) | (r << 16) | (g << 8) | b;
}

DWORD ColorConvTable::A8Y8U8V8_TO_ARGB(int a8, int y8, int u8, int v8, YuvMatrixType in_type)
{
    const ConvFunc::Y8U8V8ToRGBFunc funcs[2][3][2] = {
        {
            { YUV_TV_TO_RGB_TV_601, YUV_TV_TO_RGB_PC_601 },
            { YUV_TV_TO_RGB_TV_709, YUV_TV_TO_RGB_PC_709 },
            {YUV_TV_TO_RGB_TV_2020, YUV_TV_TO_RGB_PC_2020}
        },
        {
            { YUV_PC_TO_RGB_TV_601, YUV_PC_TO_RGB_PC_601 },
            { YUV_PC_TO_RGB_TV_709, YUV_PC_TO_RGB_PC_709 },
            {YUV_PC_TO_RGB_TV_2020, YUV_PC_TO_RGB_PC_2020}
        }
    };
    return (a8 << 24) | funcs[ConvFuncInst().m_videoRangeType == RANGE_PC ? 1 : 0][in_type == BT709 ? 1 : (in_type == BT2020 ? 2 : 0)][ConvFuncInst().m_bOutputTVRange ? 0 : 1](y8, u8, v8);
}

DWORD ColorConvTable::ColorCorrection(DWORD argb)
{
    if (ConvFuncInst().m_bCorrect601to709) {
        int r = (argb & 0x00ff0000) >> 16;
        int g = (argb & 0x0000ff00) >> 8;
        int b = (argb & 0x000000ff);
        return (argb & 0xff000000) |
               ConvFuncInst().m_convMatrix.Correct601to709(r, g, b,
                                                           ConvFuncInst().m_bOutputTVRange ? ConvMatrix::LEVEL_TV : ConvMatrix::LEVEL_PC);
    } else if (ConvFuncInst().m_bOutputTVRange) {
        return RGB_PC_TO_TV(argb);
    }
    return argb;
}
