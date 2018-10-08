/*

PICCANTE
The hottest HDR imaging library!
http://vcg.isti.cnr.it/piccante

Copyright (C) 2014
Visual Computing Laboratory - ISTI CNR
http://vcg.isti.cnr.it
First author: Francesco Banterle

This Source Code Form is subject to the terms of the Mozilla Public
License, v. 2.0. If a copy of the MPL was not distributed with this
file, You can obtain one at http://mozilla.org/MPL/2.0/.

*/

#ifndef PIC_FILTERING_FILTER_HPP
#define PIC_FILTERING_FILTER_HPP

#ifndef PIC_DISABLE_THREAD
#include <thread>
#endif

#include <functional>

#include "../image_vec.hpp"
#include "../util/tile_list.hpp"
#include "../util/string.hpp"

namespace pic {

//NOTE: This depends on the architecture!
#define TILE_SIZE 64

struct FilterFData
{
    int x, y, z;
    float *out;

    Image *dst;
    ImageVec src;
};

/**
 * @brief The Filter class
 */
class Filter
{
protected:
    float scale;
    std::vector< float > param_f;

    /**
     * @brief f
     * @param data
     */
    virtual void f(FilterFData *data)
    {

    }

    /**
     * @brief ProcessBBox
     * @param dst
     * @param src
     * @param box
     */
    virtual void ProcessBBox(Image *dst, ImageVec src, BBox *box)
    {
        FilterFData f_data;
        f_data.src = src;
        f_data.dst = dst;

        for(int k = box->z0; k < box->z1; k++) {
            f_data.z = k;

            for(int j = box->y0; j < box->y1; j++) {
                f_data.y = j;

                for(int i = box->x0; i < box->x1; i++) {
                    f_data.x = i;
                    f_data.out = (*dst)(i, j);

                    f(&f_data);
                }
            }
        }
    }

    /**
     * @brief setupAux
     * @param imgIn
     * @param imgOut
     * @return
     */
    virtual Image *setupAux(ImageVec imgIn, Image *imgOut);

public:
    bool cachedOnly;
    std::vector<Filter *> filters;

    /**
     * @brief Filter
     */
    Filter()
    {
        cachedOnly = false;
        scale = 1.0f;
    }

    ~Filter()
    {
    }

    /**
     * @brief changePass changes the pass direction.
     * @param pass
     * @param tPass
     */
    virtual void changePass(int pass, int tPass) {}

    /**
     * @brief signature returns the signature for the filter.
     * @return
     */
    virtual std::string signature()
    {
        return "FLT";
    }

    /**
     * @brief checkHalfSize
     * @param size
     * @return
     */
    int checkHalfSize(int size){
        if(size > 1)
        {
            return size >> 1;
        } else {
            return 1;
        }
    }

    /**
     * @brief getOutPutName
     * @param nameIn
     * @return
     */
    std::string getOutPutName(std::string nameIn);

    /**
     * @brief CachedProcess
     * @param imgIn
     * @param imgOut
     * @param nameIn
     * @return
     */
    Image *cachedProcess(ImageVec imgIn, Image *imgOut,
                            std::string nameIn);

    /**
     * @brief OutputSize
     * @param imgIn
     * @param width
     * @param height
     * @param channels
     * @param frames
     */
    virtual void OutputSize(Image *imgIn, int &width, int &height, int &channels, int &frames)
    {
        width       = imgIn->width;
        height      = imgIn->height;
        channels    = imgIn->channels;
        frames      = imgIn->frames;
    }

    /**
     * @brief setFloatParameters sets float parameters.
     * @param param_f
     */
    void setFloatParameters(std::vector< float > param_f)
    {
        this->param_f.insert(this->param_f.begin(), param_f.begin(), param_f.end());
    }

    /**
     * @brief Process
     * @param imgIn
     * @param imgOut
     * @return
     */
    virtual Image *Process(ImageVec imgIn, Image *imgOut);

    /**
     * @brief ProcessPAux
     * @param imgIn
     * @param imgOut
     * @param tiles
     */
    virtual void ProcessPAux(ImageVec imgIn, Image *imgOut,
                             TileList *tiles);

    /**
     * @brief ProcessP
     * @param imgIn
     * @param imgOut
     * @return
     */
    virtual Image *ProcessP(ImageVec imgIn, Image *imgOut);
};

PIC_INLINE Image *Filter::setupAux(ImageVec imgIn, Image *imgOut)
{
    if(imgOut == NULL) {
        imgOut = imgIn[0]->allocateSimilarOne();
    } else {
        if(!imgIn[0]->isSimilarType(imgOut)) {
            if(!imgOut->isValid()) {
                delete imgOut;
                imgOut = imgIn[0]->allocateSimilarOne();
            } else {
                imgOut = imgIn[0]->allocateSimilarOne();
            }
        }
    }

    return imgOut;
}

PIC_INLINE std::string Filter::getOutPutName(std::string nameIn)
{
    std::string outputName = nameIn;

    size_t found = outputName.find(".");

    if(found != std::string::npos) {
        outputName.erase(outputName.end() - 4, outputName.end());
    }

    outputName += "_filtered_";
    outputName += signature().c_str();
    outputName += ".pfm";
    return outputName;
}

PIC_INLINE Image *Filter::cachedProcess(ImageVec imgIn, Image *imgOut,
        std::string nameIn)
{
    std::string outputName = getOutPutName(nameIn);

    //check if it is chaced
    Image *imgOut2 = new Image(outputName);

    printf("%s\n", outputName.c_str());

    if(imgOut2->data == NULL) {
        if(!cachedOnly) {
            imgOut = ProcessP(imgIn, imgOut);
            imgOut->Write(outputName);
            return imgOut;
        } else {
            return NULL;
        }
    } else {
        if(imgOut != NULL) {
            imgOut->assign(imgOut2);
            return imgOut;
        } else {
            return imgOut2;
        }
    }
}

PIC_INLINE Image *Filter::Process(ImageVec imgIn, Image *imgOut)
{
    if(imgIn[0] == NULL) {
        return NULL;
    }

    imgOut = setupAux(imgIn, imgOut);

    //convolve
    BBox tmpBox(imgOut->width, imgOut->height, imgOut->frames);
    ProcessBBox(imgOut, imgIn, &tmpBox);

    return imgOut;
}

PIC_INLINE void Filter::ProcessPAux(ImageVec imgIn, Image *imgOut,
                                    TileList *tiles)
{
    bool state = true;
    BBox box;

    while(state) {
        unsigned int currentTile = tiles->getNext();

        if(currentTile < tiles->tiles.size()) {
            tiles->genBBox(currentTile, &box);
            box.z0 = 0;
            box.z1 = imgOut->frames;
            ProcessBBox(imgOut, imgIn, &box);
        } else {
            state = false;
        }
    }
}

PIC_INLINE Image *Filter::ProcessP(ImageVec imgIn, Image *imgOut)
{
#ifndef PIC_DISABLE_THREAD
    if(imgIn[0] == NULL) {
        return NULL;
    }

    imgOut = setupAux(imgIn, imgOut);

    if(imgOut == NULL) {
        return imgOut;
    }

    if((imgOut->width < TILE_SIZE) &&
       (imgOut->height < TILE_SIZE)) {
        BBox box(imgOut->width, imgOut->height);

        ProcessBBox(imgOut, imgIn, &box);
        return imgOut;
    }

    //create threads
    int numCores = std::thread::hardware_concurrency();

    std::thread **thrd = new std::thread*[numCores];
    TileList lst(TILE_SIZE, imgOut->width, imgOut->height);

    for(int i = 0; i < numCores; i++) {
        thrd[i] = new std::thread(
            std::bind(&Filter::ProcessPAux, this, imgIn, imgOut, &lst));
    }

    //join threads
    for(int i = 0; i < numCores; i++) {
        thrd[i]->join();
        delete thrd[i];
    }
    delete[] thrd;

    return imgOut;
#else
    return Process(imgIn, imgOut);
#endif
}

} // end namespace pic

#endif /* PIC_FILTERING_FILTER_HPP */

