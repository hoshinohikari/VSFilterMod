/*
 * Copyright (C) 2014, 2016 see Authors.txt
 *
 * This file is part of VSFilterMod.
 *
 * VSFilterMod is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2, or (at your option)
 * any later version.
 *
 * VSFilterMod is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 */

#pragma once

struct SubPicQueueSettings
{
    int nSize;
    int nMaxResX;
    int nMaxResY;
    bool bDisableSubtitleAnimation;
    int nRenderAtWhenAnimationIsDisabled;
    int nAnimationRate;
    bool bAllowDroppingSubpic;

    SubPicQueueSettings()
        : nSize(10)
        , nMaxResX(2560)
        , nMaxResY(1440)
        , bDisableSubtitleAnimation(false)
        , nRenderAtWhenAnimationIsDisabled(50)
        , nAnimationRate(100)
        , bAllowDroppingSubpic(true)
    {
    }

    SubPicQueueSettings(int nSize, int nMaxResX, int nMaxResY,
                        bool bDisableSubtitleAnimation, int nRenderAtWhenAnimationIsDisabled, int nAnimationRate,
                        bool bAllowDroppingSubpic)
        : nSize(nSize)
        , nMaxResX(nMaxResX)
        , nMaxResY(nMaxResY)
        , bDisableSubtitleAnimation(bDisableSubtitleAnimation)
        , nRenderAtWhenAnimationIsDisabled(nRenderAtWhenAnimationIsDisabled)
        , nAnimationRate(nAnimationRate)
        , bAllowDroppingSubpic(bAllowDroppingSubpic)
    {
    }
};
