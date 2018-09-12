//
//  DeepCurveTool.cpp
//  DeepCurveTool Node for Nuke
//
//  Created by Matt Greig on 23/04/2018.
//

//  Analyses the image in the same manner as CurveTool but on a Deep stream
// #include "DDImage/Iop.h"
// #include "DDImage/NoIop.h"

#include "DDImage/NoIop.h"
#include "DDImage/Knob.h"
#include "DDImage/Knobs.h"
#include "DDImage/DeepFilterOp.h"
#include "DDImage/ChannelSet.h"
#include "DDImage/DeepOp.h"
#include "DDImage/Executable.h"
#include "DDImage/RGB.h"

using namespace DD::Image;

static const char* const RCLASS = "DeepCurveTool";
static const char* const HELP = "Sample a DeepImage over a range of frames to find the min and max Deep Values per frame, and their respective XY screen co-ordinates.";

class DeepCurveTool : public DeepFilterOp, Executable
{
    ChannelSet _channels; // channels to operate on
    float xpos, ypos;
    float _xyKnobMax[2];
    float _xyKnobMin[2];
    float min_val, max_val;
    bool _colour_only;

public:

    void _validate(bool);
    DeepCurveTool(Node* node) : DeepFilterOp(node), Executable(this)
    {
        _channels = Mask_Deep;
        xpos = ypos = 0;
        min_val = max_val = 0.0;
        _colour_only = true;
        _xyKnobMax[0] = _xyKnobMax[1] = _xyKnobMin[0] = _xyKnobMin[1] = 0.0;
    }

    const char* node_shape() const 
    {
        return DeepOp::DeepNodeShape();
    }

    virtual Op* default_input(int idx) const
    {
        return NULL;
    }

    DeepOp* input0()
    {
        return dynamic_cast<DeepOp*>(Op::input(0));
    }

    virtual bool doDeepEngine(Box box, const ChannelSet& channels, DeepOutputPlane& outPlane)
    {
        DeepPlane inPlane;
        outPlane = DeepOutputPlane(channels, box);
        ChannelSet get_channels = channels;

        if (!input0()->deepEngine(box, get_channels, inPlane))
        {
            return false;
        }

        const int nOutputChans = channels.size();

        for (Box::iterator it = box.begin(); it != box.end(); it++)
        {
            if (Op::aborted())
            {
                return false;
            }

            DeepPixel in_pixel = inPlane.getPixel(it);
            const unsigned nSamples = in_pixel.getSampleCount();

            DeepOutPixel out_pixel;
            out_pixel.reserve(nSamples * nOutputChans);

            for (unsigned i = 0; i < nSamples; i++)
            {
                foreach(z, channels)
                {
                    out_pixel.push_back(in_pixel.getUnorderedSample(i,z));
                }
            }
            outPlane.addPixel(out_pixel);

        }

        // const int nOutputChans = channels.size();

        return true;
    }

    ~DeepCurveTool() {}

    void beginExecuting();
    void endExecuting();
    void execute();
    virtual Executable* executable() { return this; }

    void getDeepRequests(Box bbox, const DD::Image::ChannelSet& channels, int count, std::vector<RequestData>& requests); 
    // virtual bool doDeepEngine(Box box, const ChannelSet& channels, DeepOutputPlane& outPlane);
    virtual void knobs(Knob_Callback);
    const char* Class() const { return RCLASS; }
    const char* node_help() const { return HELP; }
    static Iop::Description d;

};

// REQUIRED METHODS

void DeepCurveTool::_validate(bool for_real) 
{
    DeepFilterOp::_validate(for_real);
    
    if (input0()) 
    {
        input0()->validate(true);
        DeepInfo srcDeepInfo = input0()->deepInfo();
        _deepInfo = srcDeepInfo;

    }
}

void DeepCurveTool::getDeepRequests(Box bbox, const DD::Image::ChannelSet& channels, int count, std::vector<RequestData>& requests) 
{
    if (!input0())
        return;
    DD::Image::ChannelSet get_channels = channels;
    requests.push_back(RequestData(input0(), bbox, get_channels, count));
}


// EXECUTABLE OVERLOADS

void DeepCurveTool::beginExecuting()
{
    // std::cout << "begin executing" << std::endl;
    knob("max_val")->set_animated();
    knob("min_val")->set_animated();
    knob("max_pos")->set_animated();
    knob("min_pos")->set_animated();
    // knob("max_y_pos")->set_animated();
}

void DeepCurveTool::endExecuting()
{
    // std::cout << "end executing" << std::endl;
    // std::cout << "max_val found was: " << max_val << std::endl;
}

void DeepCurveTool::execute()
{
    float local_max_val, local_min_val;
    local_max_val = local_min_val = 0;

    float max_xpos, max_ypos;
    int min_xpos, min_ypos;

    max_xpos = max_ypos = min_xpos = min_ypos = 0;

    ChannelSet sampleChans = _channels; // get the channels to sample as per the channels knob
    // ChannelSet sampleChans = _deepInfo.channels();
    sampleChans += Mask_RGB; // add RGB so we cal check color values on pixels

    DeepPlane inPlane;
    const Format* format = _deepInfo.format();

    Box box = Box(format->x(), format->y(), format->r(), format->t());

    if (!input0()->deepEngine(box, sampleChans, inPlane))
    {
        std::cerr << "inPlane could not be initialised" << std::endl;
    }

    ChannelSet colorChans = Mask_RGB;

    const int nOutputChans = _channels.size();

    for (Box::iterator it = box.begin(); it != box.end(); it++) {
        // std::cout << "box it: " << it.x << std::endl; 
        DeepPixel in_pixel = inPlane.getPixel(it);
        const unsigned nSamples = in_pixel.getSampleCount();

        for (unsigned i = 0; i < nSamples; ++i) {
            // std::cout << "testing sample: " << i << std::endl;
            bool color_val = false;
            bool checked_for_color = false;
            float luma;
            if (_colour_only){
                foreach(z, colorChans) {
                    if (!color_val){
                        if (z == Chan_Red || z == Chan_Green || z == Chan_Blue) {
                            luma = y_convert_rec709(in_pixel.getUnorderedSample(i, Chan_Red),
                                                    in_pixel.getUnorderedSample(i, Chan_Green),
                                                    in_pixel.getUnorderedSample(i, Chan_Blue));
                            if (luma > 0.0) {
                                // we have a color_value so proceed, otherwise skip to the next sample
                                // std::cout << "found pixel with luma of: " << luma << "so checking deep vals" << std::endl;
                                color_val = true;
                            } 
                        }
                    }
                }
            }
            if (color_val || !_colour_only){
                foreach(z, _channels) {
                    // std::cout << "Now checking: " << z <<std::endl;
                    float val = in_pixel.getUnorderedSample(i, z);
                    if (val > local_max_val) {
                        local_max_val = val;
                        max_xpos = it.x;
                        max_ypos = it.y;

                    }
                    if (local_min_val == 0) {
                        local_min_val = val;
                        min_xpos = it.x;
                        min_ypos = it.y;
                    }
                    if (val < local_min_val) {
                        local_min_val = val;
                        min_xpos = it.x;
                        min_ypos = it.y;
                    }
                }
            }
        }
    }

    knob("max_val")->set_value(local_max_val);
    knob("min_val")->set_value(local_min_val);

    // _xyKnobMax[0] = max_xpos;
    // _xyKnobMax[1] = max_ypos;

    knob("max_pos")->set_value(max_xpos, 0);
    knob("max_pos")->set_value(max_ypos, 1);

    knob("min_pos")->set_value(min_xpos, 0);
    knob("min_pos")->set_value(min_ypos, 1);


    // std::cout << "Pos var vals: " << _xyKnobMax[0] << ", " << _xyKnobMax[1] << std::endl;

}   

// KNOBS, CONTROLS AND CALLBACKS

void DeepCurveTool::knobs(Knob_Callback f) 
{
    // ChannelSet_knob(f, &_channels, "Channels");
    Text_knob(f, "Position");
    // SetFlags(f, Knob::STARTLINE);
    XY_knob(f, _xyKnobMax, "max_pos", "Max Pos");
    // Float_knob(f, &xpos, "max_x_pos", "x"); ClearFlags(f, Knob::SLIDER);
    // Float_knob(f, &ypos, "max_y_pos", "y"); 
    // ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
    Float_knob(f, &max_val, "max_val", "max");
    ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);

    XY_knob(f, _xyKnobMin, "min_pos", "Min Pos");
    Float_knob(f, &min_val, "min_val", "min ");
    ClearFlags(f, Knob::SLIDER | Knob::STARTLINE);
    
    Bool_knob(f, &_colour_only, "colour_only", "Colour Only?");
    Tooltip(f, "Only look for deep values on pixels that have a colour value.");
    SetFlags(f, Knob::STARTLINE);

    const char* render_script = "currentNode = nuke.thisNode()\n"
    "nodeList = [currentNode]\n"
    "nukescripts.render_panel(nodeList, False)\n";
    PyScript_knob(f, render_script, "Analyse");
    SetFlags(f, Knob::STARTLINE);
}

static Op* build(Node* node) { return new DeepCurveTool(node); }
Op::Description DeepCurveTool::d(RCLASS, "Color/DeepCurveTool", build);