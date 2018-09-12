//
//  STUnmap.cpp
//  STUnmapNode
//
//  Created by Matt Greig on 10/01/2017.
//

/*
  This node will take a pre-rendered image and an associated UV/ST Map
  and 'unwrap' it into screen space a la a source UV Map
*/

static const char* const HELP = "Unwraps an image into screen space by a\n"
                                "given UV or ST Map.\n";

#include <DDImage/NukeWrapper.h>
#include <DDImage/PlanarIop.h>
#include <DDImage/Row.h>
#include <DDImage/Knobs.h>

using namespace DD::Image;

class STUnwrap : public PlanarIop
{
public:
    void in_channels(int input, ChannelSet& mask) const;


    STUnwrap(Node* node) : PlanarIop(node)
    {
      // set the default knob values on construction
      inputs(2);

    }

    virtual ~STUnwrap()
    {}

    const char* input_label(int input, char* buffer) const;

    virtual void knobs(Knob_Callback);

    // The first step in Nuke is to validate
    void _validate(bool);

    void getRequests(const Box& box, const ChannelSet& channels, int count, RequestOutput &reqData) const;

    //! This function does all the work.
    void renderStripe( ImagePlane& imagePlane );

    //! Return the name of the class.
    static const Iop::Description   d;

    const char* Class() const { return d.name; }
    const char* node_help() const { return HELP; }

private:

    Format format;

};

void STUnwrap::knobs(Knob_Callback f)
{
    // Add knob logic here

}

void STUnwrap::_validate(bool for_real)
{
    input0().validate(for_real);
    input1().validate(for_real);

    info_ = input(0).info();

    format = input(0).format();

}

const char* STUnwrap::input_label(int input, char* buffer) const {
    switch (input) {
        case 0:
            return "src";
        case 1:
            return "map";
        default:
            return "src";
    }
}


void STUnwrap::in_channels(int input, ChannelSet& mask) const {

    // mask += Mask_All;
}

void STUnwrap::_request(int x, int y, int r, int t, ChannelMask channels, int count)
{

  input(0)->request(x, y, r, t, channels, count);
  input(1)->_request(x, y, r, t, channels, count);

}

void STUnwrap::engine (int y, int x, int r, ChannelMask channels, Row& row )
{
    Row srcLine(x, r); // the src we are unwrapping
    Row mapLine(x, r); // the corresponding map
    Row dstLine(format.x(), format.r()); // the row we will draw to

    srcLine.get(input0(), y, x, r, channels); // need to work on whole bounds... not just visible
    mapLine.get(input1(), y, x, r, channels);

    int rowWidth = r - x;

    for (int i = x; i < r; i++) {

        float redVal;
        float greenVal;

        foreach(z, channels) {
            if (z == Chan_Red) {
                redVal = z;
            }
            if (z == Chan_Green) {
                greenVal = z;
            }
        }

        dstLine.get()



    foreach(z, channels) {

        float* OUTBUF = out.writable(z);
        float* dstbuf = dstLine.writable(z);

        // need to make sure you're operating on the whole bounds
        // not just the display window

        for (int i = x; i < r; i++) {

            // get the map value at this pixel
            float redVal = 

            *dstbuf++ = srcLine[z][index];

        }
        out.copy(dstLine, z, x, r);

    }
}

static Iop* build(Node* node) { return new NukeWrapper(new STUnwrap(node)); }
const Iop::Description STUnwrap::d("STUnwrap", "Transform/STUnwrap", build);
