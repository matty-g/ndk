//
//  Scroll.cpp
//  ScrollNode
//
//  Created by Matt Greig on 27/10/2016.
//

/*
  This node will "wrap-around" images on x and y axes creating a faux
  infinite loop.
  The values knobs should be primarily expression-driven to maintain
  a constant speed, but use will vary - future versions may use the knobs to
  set a percentage "speed" value
  Currently there is no "free" implementation of motion blur, as this node's
  primary effect could not be achieved via a Matrix transform. It is recommended
  that a directional blur or even using pre-calculated vectors might be useful.
  Maybe if I get clever in the future, I may implement vector-generation inside
  the node itself... we'll see.

  TODO: add motion blur
        adjust knobs to be speed values
        add support for channel and layer selection
*/

static const char* const HELP = "Wraps around an image in x/y or top/bottom.\n"
                                "Use an expression in the knobs to drive\n"
                                "persistent motion. ie. sin(frame)\n";

#include <DDImage/NukeWrapper.h>
#include <DDImage/PixelIop.h>
#include <DDImage/Row.h>
#include <DDImage/Knobs.h>

using namespace DD::Image;

class Scroll : public PixelIop
{
public:
    void in_channels(int input, ChannelSet& mask) const;

    float horizontalValue;
    float verticalValue;
    int imgheight;
    int img_t;
    int img_h;
    int rhs, lhs, bot, top;

    Scroll(Node* node) : PixelIop(node)
    {
      // set the default knob values on construction
      horizontalValue = 0.0;
      verticalValue = 0.0;
      imgheight = img_t = img_h = 0;
    }

    virtual ~Scroll()
    {}

    // Set the min and max inputs, you can change this as your requirements
    int minimum_inputs() const { return 1; }
    int maximum_inputs() const { return 1; }

    virtual void knobs(Knob_Callback);

    // The first step in Nuke is to validate
    void _validate(bool);
    void _request(int x, int y, int r, int t, ChannelMask channels, int count);

    //! This function does all the work.
    void pixel_engine (const Row& in, int y, int x, int r, ChannelMask channels, Row& out );

    //! Return the name of the class.
    static const Iop::Description   d;

    const char* Class() const { return d.name; }
    const char* node_help() const { return HELP; }

private:

};

void Scroll::knobs(Knob_Callback f)
{
    // Add knob logic here

    Float_knob(f, &horizontalValue, "X Transform");
    Tooltip(f, "The image is offset by this value\n"
               "ie. sin(frame)");
    Float_knob(f, &verticalValue, "Y Transform");
    Tooltip(f, "The image is offset by this value\n"
               "ie. sin(frame)");
}

void Scroll::_validate(bool for_real)
{
    copy_info(); // copy bbox channels etc from input0, which will validate it.

    img_h = info_.h();
    img_t = info_.t();

    // set class variable here
    // need this to draw upon full bounds of input image in order for
    // pixel_engine to calculate based on full bounds of image
    lhs = info_.x();
    rhs = info_.r();
    top = info_.t();
    bot = info_.y();

}


void Scroll::in_channels(int input, ChannelSet& mask) const {

    // mask += Mask_All;
}

void Scroll::_request(int x, int y, int r, int t, ChannelMask channels, int count)
{
  // int height = format().height();
  // int width  = format().width();

  // make sure we ask for full bounds of input, otherwise down-stream node
  // will only ask for it's visible bounds
  input(0)->request(lhs, bot, rhs, top, channels, count);
}

void Scroll::pixel_engine (const Row& in, int y, int x, int r,
                            ChannelMask channels, Row& out )
{
    Row srcLine(lhs, rhs); // the row we are drawing from
    Row dstLine(lhs, rhs); // the row we will draw to

    int vertOffset = (img_t - (int)verticalValue + y) % img_t;
    if (vertOffset < 0){
        vertOffset += img_h;
      }

    //std::cout << "Row: " << y << " will now be: " << vertOffset << std::endl;

    srcLine.get(input0(), vertOffset, lhs, rhs, channels); // need to work on whole bounds... not just visible

    int rowWidth = rhs - lhs;

    foreach(z, channels) {

        float* OUTBUF = out.writable(z);
        float* dstbuf = dstLine.writable(z);

        // need to make sure you're operating on the whole bounds
        // not just the display window

        for (int i = lhs; i < rhs; i++) {

            int index = (rhs - (int)horizontalValue + i) % rhs;

            if (index < 0) {
                index += rowWidth;
            } // to overcome stupid C++ returning negative value from a modulo

            *dstbuf++ = srcLine[z][index];

        }
        out.copy(dstLine, z, x, r);

    }
}

static Iop* build(Node* node) { return new NukeWrapper(new Scroll(node)); }
const Iop::Description Scroll::d("Scroll", "Transform/Scroll", build);
