// MyPlus.cpp
// test class by Matty G.


static const char *const CLASS = "MyPlus";
static const char *const HELP = "Test plus node";

#include "DDImage/PixelIop.h"
#include "DDImage/Row.h"
#include "DDImage/Knobs.h"

using namespace DD::Image;

class MyPlus : public PixelIop
{
    ChannelSet a_channels;
    ChannelSet b_channels;
    ChannelSet c_channels;

public:
    MyPlus(Node* node) : PixelIop(node)
    {
        inputs(2);
        a_channels = Mask_RGBA;
        b_channels = Mask_RGBA;
        c_channels = Mask_RGBA;
    }

    ~MyPlus()
    {}

    void _validate(bool for_real)
    {
        // copy_info();
        // merge_info(1);

        input0().validate(for_real);
        input1().validate(for_real);

        copy_info();
        merge_info(1);

        ChannelSet outchans(c_channels);
        // outchans += a_channels;
        // outchans &= input1().channels();
        set_out_channels(outchans);
        std::cout << "outchans: " << outchans << std::endl;
        PixelIop::_validate(for_real);

    }

    // void in_channels(int,)

    void _request(int x, int y, int r, int t, ChannelMask mask, int count)
    {

        ChannelSet all_chans = Mask_All;
        std::cout << "_request calling: " << mask << " channels" << std::endl;

        input0().request(x, y, r, t, all_chans, count);
        input1().request(x, y, r, t, all_chans, count);
    }

    void in_channels(int input_number, ChannelSet& channels) const
    {
        // // std::cout << "in_channels setting: " << channels << " for input: " << input_number << std::endl;
        // if (input_number == 0)
        // {
        //     channels += b_channels;
        // } else {
        //     std::cout << "in_channels setting: " << channels << " for input: " << input_number << std::endl;
        //     channels += a_channels;
        // }

        // // std::cout << "after op... channels is now: " << channels << std::endl;
        
        // // channels += Mask_All;

        channels += a_channels;
        channels += b_channels;
        channels += c_channels;

    }

    void pixel_engine(const Row& in, int y, int x, int r, ChannelMask mask, Row& out)
    {
        // std::cout << "pixel engine called! " << std::endl;
        // if (y % 2 == 0) {
        //     out.get(input0(), y, x, r, mask);
        // } else {
        //     out.get(input1(), y, x, r, mask);
        // }

        Row a_row(x, r);
        Row b_row(x, r);

        ChannelMask a_chans(a_channels);
        ChannelMask b_chans(b_channels);

        std::cout << "Mask: " << mask << std::endl;

        input0().get(y, x, r, b_chans, b_row);

        input1().get(y, x, r, a_chans, a_row);

        // std::cout << "a_chans[i]: " << a_chans.first() << std::endl;

        Channel a_chan = a_chans.first();
        Channel b_chan = b_chans.first();

        foreach(z, mask) {
            // std::cout << "a_chan: " << a_chan << std::endl;
            

            float *output = out.writable(z);
            const float *in_a = a_row[a_chan];
            const float *in_b = b_row[b_chan];
            for (int i = x; i < r; i++)
            {   
               output[i] = in_a[i] + in_b[i];
            }
            a_chan = a_chans.next(a_chan);
            b_chan = b_chans.next(b_chan);
        }

        // Channel a_channel = a_chans.first();
        // Channel b_channel = b_chans.first();

        // a_row = Row()

        // foreach(z, mask)
        // {   
        //     std::cout << "z: " << z << std::endl;
        //     float *output = out.writable(z);
        //     const float *in_a = a_row[a_channel];
        //     const float *in_b = b_row[b_channel];
        //     for (int i = x; i < r; i++)
        //     {
                
        //         output[i] = in_a[i] + in_b[i];
        //     }
        //     a_channel = a_chans.next(a_channel);
        //     b_channel = b_chans.next(b_channel);
        // }

        // for (int i = 0; i < mask.size(); i++) {
        //     float *output = out.writable(mask[i]);
        //     const float *in_a = a_row[a_chans[i]];
        //     const float *in_b = b_row[b_chans[i]];
        //     for (int j = x; j < r; j++)
        //     {
                
        //         output[j] = in_a[j] + in_b[j];
        //     }
        // }


        // if (x == 960 && y == 540){
        //     // std::cout << "afrom: " << AFROM << "   bfrom: " << BFROM << std::endl;
        //     // std::cout << "afrom: " << *AFROM << "   bfrom: " << *BFROM << std::endl;
        //     std::cout << "Pixel engine called" << std::endl;
        // }

        // ChannelSet done;
        // foreach(z, channels)
        // {
        //     if (done & z) {
        //         continue;
        //     }

        //     if (colourIndex(z) >=3){
        //         out.copy(in, z, x, r);
        //         continue;
        //     }

        //     Channel rchan 



        // }

        // // Row a_row(x, r);
        // // Row b_row(x, r);

        // // input0().get(y, x, r, mask, a_row);
        // // input1().get(y, x, r, mask, b_row);


        // foreach(z, mask)
        // {
        //     const float* AFROM = a_row[z];
        //     const float* BFROM = b_row[z];
        //     float* TO = out.writable(z);

        //     if (x == 960 && y == 540){
        //         std::cout << "afrom: " << AFROM << "   bfrom: " << BFROM << std::endl;
        //         std::cout << "afrom: " << *AFROM << "   bfrom: " << *BFROM << std::endl;
        //     }

        //     *TO = *AFROM + *BFROM;
        // }
    }

    static const Iop::Description d;
    void knobs(Knob_Callback);
    const char* Class() const { return d.name; }
    const char* node_help() const {return HELP; }

};

void MyPlus::knobs(Knob_Callback f)
{
    Input_ChannelMask_knob(f, &a_channels, 0, "A channels");
    Input_ChannelMask_knob(f, &b_channels, 0, "B channels");
    Input_ChannelMask_knob(f, &c_channels, 0, "output");
}

static Iop* build(Node* node) { return new MyPlus(node); }
const Iop::Description MyPlus::d(CLASS, "Color/MyPlus", build);